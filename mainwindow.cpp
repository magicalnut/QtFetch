#include "mainwindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

namespace {

// 左侧参数区：字段宽度与表单节奏
constexpr int kLeftPanelMinWidth = 280;
constexpr int kLeftPanelMaxWidth = 460;
constexpr int kThreadCountDefault = 4;
constexpr int kConcurrentOptions[] = {2, 4};   // 「同时下载任务」下拉框，先放两项
constexpr int kSectionSpacing = 12;

// 右侧任务列表列定义：首列给按钮，末列给链接，其余按比例分配
enum TaskColumn {
    ColId = 0,
    ColUrl,
    ColProgress,
    ColControl,
    ColCancel,
    ColumnCount
};

// 造一个带标题的小节，避免每一处都重复三行布局代码
QWidget *makeSection(const QString &title, QWidget *body, QWidget *parent = nullptr)
{
    auto *box = new QWidget(parent);
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *caption = new QLabel(title, box);
    QFont captionFont = caption->font();
    captionFont.setBold(true);
    caption->setFont(captionFont);

    auto *line = new QFrame(box);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);

    layout->addWidget(caption);
    layout->addWidget(line);
    layout->addWidget(body);
    return box;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("QtFetch 下载器"));
    resize(1120, 700);

    write_thread=new QThread();
    writer=new SqlWriter();
    writer->moveToThread(write_thread);
    connect(writer,&SqlWriter::tablefail,this,[this](int num){
        showFatalAndQuit(tr("数据库表%1建立失败").arg(num));
    });
    connect(writer,&SqlWriter::initfail,this,[this](){
        showFatalAndQuit(tr("无法打开/创建数据库"));
    });
    connect(write_thread,&QThread::started,writer,&SqlWriter::init);
    connect(writer,&SqlWriter::initsuccess,this,[this](){
        statusBar()->showMessage("数据库初始化完成");
    });
    read_thread=new QThread();
    reader=new SqlReader();
    reader->moveToThread(read_thread);
    connect(read_thread,&QThread::started,reader,&SqlReader::init);
    connect(reader,&SqlReader::initsuccess,reader,&SqlReader::readall);
    QEventLoop loop;
    connect(reader,&SqlReader::finished,&loop,&QEventLoop::quit);
    connect(reader,&SqlReader::initfail,&loop,&QEventLoop::quit);
    connect(reader,&SqlReader::errorOccurred,this,[](const QString &e){
        qWarning() << "SqlReader:" << e;
    });
    connect(reader,&SqlReader::tasksLoaded,this,&MainWindow::onTasksLoaded);


    connect(writer,&SqlWriter::initsuccess,this,[this](){
        read_thread->start();
    });

    connect(this,&MainWindow::taskInsertRequested,writer,&SqlWriter::addTask);

    connect(this,&MainWindow::stateUpdateRequested,writer,&SqlWriter::updateState);
    connect(this,&MainWindow::taskRemoveRequested,writer,&SqlWriter::removeTask);

    connect(writer,&SqlWriter::taskCreated,this,[this](qint64 id,const QString &url,const QString &savePath,int threads){
        appendTaskRow(id,url);
        statusBar()->showMessage(tr("任务 %1 已创建").arg(id),3000);

        auto *session = new DownloadSession(this);
        connect(session,&DownloadSession::progressChanged,this,
                [this](qint64 tid,int percent){
            const int row = rowOfTaskId(tid);
            if(row < 0) return;
            auto *bar = qobject_cast<QProgressBar*>(m_taskTable->cellWidget(row,ColProgress));
            if(bar) bar->setValue(percent);
        });
        connect(session,&DownloadSession::finished,this,[this](qint64 tid,bool ok){
            const int row = rowOfTaskId(tid);
            if (row >= 0 && ok) {
                auto *bar = qobject_cast<QProgressBar*>(m_taskTable->cellWidget(row,ColProgress));
                if (bar) bar->setValue(100);

                auto *btn = qobject_cast<QPushButton*>(m_taskTable->cellWidget(row,ColControl));
                if (btn) { btn->setText(tr("已完成")); btn->setEnabled(false); }
            }
            statusBar()->showMessage(ok ? tr("任务 %1 下载完成").arg(tid)
                                        : tr("任务 %1 下载失败").arg(tid), 5000);

            if (ok) {
                emit taskRemoveRequested(tid);
            } else {
                emit stateUpdateRequested(tid, QStringLiteral("failed"));
                    }

            if (auto *s = m_sessions.take(tid))
                        s->deleteLater();       // 销毁 session
        });

        m_sessions.insert(id,session);
        session->start(id,url,savePath,threads);
    });
    connect(writer,&SqlWriter::taskInsertFailed,this,[this](const QString &msg){
        statusBar()->showMessage(tr("创建任务失败：%1").arg(msg),5000);
    });

    write_thread->start();

    setupToolBar();
    setupCentralArea();

    QTimer::singleShot(5000,&loop,&QEventLoop::quit);
    loop.exec();

    statusBar()->showMessage(tr("就绪"));
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------- 导航栏 ----

void MainWindow::setupToolBar()
{
    auto *bar = addToolBar(tr("导航栏"));
    bar->setMovable(false);
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->setStyleSheet(QStringLiteral(
        "QToolBar { spacing: 6px; padding: 6px 8px; }"
        "QToolButton { padding: 5px 14px; }"));

    auto *startAllAction = bar->addAction(tr("全部开始"));
    startAllAction->setToolTip(tr("开始所有已暂停/等待中的任务"));
    connect(startAllAction, &QAction::triggered, this, &MainWindow::onStartAll);

    auto *pauseAllAction = bar->addAction(tr("全部暂停"));
    pauseAllAction->setToolTip(tr("暂停所有正在下载的任务"));
    connect(pauseAllAction, &QAction::triggered, this, &MainWindow::onPauseAll);

    auto *clearAction = bar->addAction(tr("清空已完成"));
    clearAction->setToolTip(tr("从列表中移除所有已完成的任务"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::onClearFinished);

    // 「同时下载任务」：先给两项，按需改 kConcurrentOptions 或 m_concurrentCombo
    auto *spacer = new QWidget(bar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);

    bar->addWidget(new QLabel(tr("同时下载任务："), bar));

    m_concurrentCombo = new QComboBox(bar);
    for (int count : kConcurrentOptions)
        m_concurrentCombo->addItem(tr("%1 个").arg(count), count);
    m_concurrentCombo->setCurrentIndex(1);   // 默认 4
    m_concurrentCombo->setMinimumWidth(96);
    bar->addWidget(m_concurrentCombo);
    connect(m_concurrentCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        onConcurrentChanged(m_concurrentCombo->itemData(index).toInt());
    });
}

// ------------------------------------------------------------ 中央两列区 ----

void MainWindow::setupCentralArea()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(createParamPanel());
    splitter->addWidget(createTaskPanel());
    splitter->setStretchFactor(0, 0);   // 左列固定，右列吃掉多余宽度
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({340, 780});

    layout->addWidget(splitter);
    setCentralWidget(central);
}

QWidget *MainWindow::createParamPanel()
{
    auto *panel = new QWidget;
    panel->setMinimumWidth(kLeftPanelMinWidth);
    panel->setMaximumWidth(kLeftPanelMaxWidth);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, kSectionSpacing, 0);
    layout->setSpacing(kSectionSpacing);

    // --- 下载参数表单 ---
    auto *form = new QWidget(panel);
    auto *formLayout = new QFormLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setRowWrapPolicy(QFormLayout::DontWrapRows);

    m_urlEdit = new QLineEdit(form);
    m_urlEdit->setPlaceholderText(tr("https://example.com/file.zip"));
    m_urlEdit->setClearButtonEnabled(true);
    formLayout->addRow(tr("下载链接："), m_urlEdit);

    m_fileNameEdit = new QLineEdit(form);
    m_fileNameEdit->setPlaceholderText(tr("留空则用链接中的文件名，如 file.zip"));
    m_fileNameEdit->setClearButtonEnabled(true);
    m_fileNameEdit->setToolTip(tr("保存到「保存地址」时使用的文件名"));
    formLayout->addRow(tr("文件保存名："), m_fileNameEdit);

    auto *dirRow = new QWidget(form);
    auto *dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(6);

    m_saveDirEdit = new QLineEdit(dirRow);
    m_saveDirEdit->setPlaceholderText(tr("默认为「下载」目录"));
    m_saveDirEdit->setText(QDir::homePath() + QStringLiteral("/Downloads"));
    m_saveDirEdit->setClearButtonEnabled(true);

    m_browseButton = new QPushButton(tr("浏览…"), dirRow);
    connect(m_browseButton, &QPushButton::clicked, this, &MainWindow::onPickSaveDir);

    dirLayout->addWidget(m_saveDirEdit, 1);
    dirLayout->addWidget(m_browseButton);
    formLayout->addRow(tr("保存地址："), dirRow);

    m_threadSpin = new QSpinBox(form);
    m_threadSpin->setRange(1, 64);
    m_threadSpin->setValue(kThreadCountDefault);
    m_threadSpin->setSuffix(tr(" 线程"));
    m_threadSpin->setToolTip(tr("单个任务使用的分片下载线程数"));
    formLayout->addRow(tr("下载线程数："), m_threadSpin);

    layout->addWidget(makeSection(tr("下载参数"), form, panel));

    // --- 开始下载 ---
    m_startButton = new QPushButton(tr("开始下载"), panel);
    m_startButton->setMinimumHeight(36);
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2d7ff9; color: white; border: none;"
        "              border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1f6fe5; }"
        "QPushButton:pressed { background-color: #1a5fc7; }"
        "QPushButton:disabled { background-color: #b9c6d6; }"));
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartDownload);
    layout->addWidget(m_startButton);

    // 左列到此为止：上面是参数，下面是「开始下载」。剩余空间留白，
    // 参数区不会因为窗口拉高而被拉伸变形。
    layout->addStretch(1);

    return panel;
}

QWidget *MainWindow::createTaskPanel()
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(kSectionSpacing, 0, 0, 0);
    layout->setSpacing(kSectionSpacing);

    // --- 右侧标题 + 统计 ---
    auto *head = new QWidget(panel);
    auto *headLayout = new QHBoxLayout(head);
    headLayout->setContentsMargins(0, 0, 0, 0);
    headLayout->setSpacing(8);

    auto *caption = new QLabel(tr("任务列表"), head);
    QFont captionFont = caption->font();
    captionFont.setBold(true);
    caption->setFont(captionFont);

    m_taskSummary = new QLabel(head);
    m_taskSummary->setStyleSheet(QStringLiteral("color: #7a8699;"));

    headLayout->addWidget(caption);
    headLayout->addStretch(1);
    headLayout->addWidget(m_taskSummary);
    layout->addWidget(head);

    // --- 任务表格 ---
    m_taskTable = new QTableWidget(0, ColumnCount, panel);
    m_taskTable->setHorizontalHeaderLabels({tr("任务ID"), tr("下载链接"), tr("下载进度"),tr("操作"), tr("取消")});
    m_taskTable->verticalHeader()->setVisible(false);
    m_taskTable->setShowGrid(false);
    m_taskTable->setAlternatingRowColors(true);
    m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_taskTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_taskTable->setWordWrap(false);
    m_taskTable->horizontalHeader()->setHighlightSections(false);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColId, QHeaderView::ResizeToContents);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColUrl, QHeaderView::Stretch);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColProgress, QHeaderView::Fixed);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColControl, QHeaderView::Fixed);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColCancel, QHeaderView::Fixed);
    m_taskTable->setColumnWidth(ColProgress, 200);
    m_taskTable->setColumnWidth(ColControl, 96);
    m_taskTable->setColumnWidth(ColCancel, 80);
    m_taskTable->verticalHeader()->setDefaultSectionSize(38);
    layout->addWidget(m_taskTable, 1);

    m_taskSummary->setText(tr("共 0 个任务"));
    return panel;
}

// -------------------------------------------------------- 列表/表格更新 ----

void MainWindow::appendTaskRow(qint64 id,const QString &url)
{
    const int row = m_taskTable->rowCount();
    m_taskTable->insertRow(row);

    auto *idItem = new QTableWidgetItem(QString::number(id));
    idItem->setTextAlignment(Qt::AlignCenter);
    m_taskTable->setItem(row, ColId, idItem);

    auto *urlItem = new QTableWidgetItem(url);
    urlItem->setToolTip(url);
    m_taskTable->setItem(row, ColUrl, urlItem);

    auto *progress = new QProgressBar(m_taskTable);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setFormat(QStringLiteral("%p%"));
    progress->setTextVisible(true);
    progress->setFixedHeight(18);
    m_taskTable->setCellWidget(row, ColProgress, progress);

    auto *toggleButton = new QPushButton(tr("暂停"), m_taskTable);
    toggleButton->setToolTip(tr("暂停 / 继续该任务"));
    m_taskTable->setCellWidget(row, ColControl, toggleButton);
    connect(toggleButton, &QPushButton::clicked, this, [this, toggleButton]() {
        // 行号会随删除/排序变化，点击时按控件反查当前行
        for (int r = 0; r < m_taskTable->rowCount(); ++r) {
            if (m_taskTable->cellWidget(r, ColControl) == toggleButton) {
                onTaskToggle(r);
                return;
            }
        }
    });

    auto *cancelButton = new QPushButton(tr("取消"), m_taskTable);
    cancelButton->setToolTip(tr("取消并移除该任务"));
    cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton { color: #d64545; }"));
    m_taskTable->setCellWidget(row, ColCancel, cancelButton);
    connect(cancelButton, &QPushButton::clicked, this, [this, cancelButton]() {
        for (int r = 0; r < m_taskTable->rowCount(); ++r) {
            if (m_taskTable->cellWidget(r, ColCancel) == cancelButton) {
                onTaskCancel(r);
                return;
            }
        }
    });

    m_taskSummary->setText(tr("共 %1 个任务").arg(m_taskTable->rowCount()));
}

int MainWindow::rowOfTaskId(qint64 id) const
{
    for (int r = 0; r < m_taskTable->rowCount(); ++r) {
        const QTableWidgetItem *item = m_taskTable->item(r, ColId);
        if (item && item->text().toLongLong() == id) return r;
    }
    return -1;
}

void MainWindow::onTasksLoaded(const QList<TaskRecord> &tasks)
{
    for (const TaskRecord &r : tasks)
        appendTaskRow(r.id, r.url);

    statusBar()->showMessage(tr("已载入 %1 个任务").arg(tasks.size()));
}

// -------------------------------------------------------------- 导航栏槽 ----

void MainWindow::onStartAll()
{
    statusBar()->showMessage(tr("全部开始：待接入调度器"), 3000);
}

void MainWindow::onPauseAll()
{
    statusBar()->showMessage(tr("全部暂停：待接入调度器"), 3000);
}

void MainWindow::onClearFinished()
{
    // 骨架：真实实现按进度 100% 的行倒序删除即可
    statusBar()->showMessage(tr("清空已完成：待接入任务管理"), 3000);
}

void MainWindow::onConcurrentChanged(int count)
{
    statusBar()->showMessage(tr("同时下载任务数：%1").arg(count), 3000);
}

// ---------------------------------------------------------- 参数/任务槽 ----

void MainWindow::onStartDownload()
{
    const QString url = m_urlEdit->text().trimmed();
    const QString fileName = m_fileNameEdit->text().trimmed();
    const QString dir = m_saveDirEdit->text().trimmed();
    const int threads = m_threadSpin->value();

    if (url.isEmpty()) {
        statusBar()->showMessage(tr("请先填写下载链接"), 3000);
        m_urlEdit->setFocus();
        return;
    }
    if (dir.isEmpty()) {
        statusBar()->showMessage(tr("请先选择保存地址"), 3000);
        return;
    }

    // 目录不存在就建
    if (!QDir().mkpath(dir)) {
        statusBar()->showMessage(tr("保存目录不可用：%1").arg(dir), 5000);
        return;
    }

    // 文件保存名留空 → 从链接里取最后一段
    QString name = fileName;
    if (name.isEmpty()) {
        name = QUrl(url).fileName();
        if (name.isEmpty())
            name = QStringLiteral("download.bin");   // 链接以 / 结尾时兜底
    }
    const QString savePath = QDir(dir).filePath(name);

    emit taskInsertRequested(url, savePath, threads);
    statusBar()->showMessage(tr("正在创建任务…"), 3000);
    m_urlEdit->clear();
    m_fileNameEdit->clear();
}

void MainWindow::onPickSaveDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择保存目录"), m_saveDirEdit->text(), QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty())
        m_saveDirEdit->setText(dir);
}

void MainWindow::onTaskToggle(int row)
{
    auto *button = qobject_cast<QPushButton *>(m_taskTable->cellWidget(row, ColControl));
    if (!button)
        return;

    const QTableWidgetItem *idItem = m_taskTable->item(row, ColId);
    if (!idItem) return;
    const qint64 id = idItem->text().toLongLong();

    const bool pausing = (button->text() == tr("暂停"));

    if (auto *s = m_sessions.value(id)) {
        if (pausing) s->pause();
        else         s->resume();
    }

    button->setText(pausing ? tr("继续") : tr("暂停"));
    emit stateUpdateRequested(id, pausing ? QStringLiteral("paused")
                                          : QStringLiteral("downloading"));

    statusBar()->showMessage(pausing ? tr("任务 %1 已暂停").arg(id)
                                     : tr("任务 %1 已继续").arg(id), 3000);
}

void MainWindow::onTaskCancel(int row)
{
    const QTableWidgetItem *idItem = m_taskTable->item(row, ColId);
    if (!idItem) return;
    const qint64 id = idItem->text().toLongLong();

    if (auto *s = m_sessions.take(id)) {      // take：同时从 map 里移除
        s->stop();                            // 释放文件句柄
        s->deleteLater();
    }

    emit taskRemoveRequested(id);

    // 不能在这里直接 removeRow
    QMetaObject::invokeMethod(this, [this, row, id]() {
        m_taskTable->removeRow(row);
        m_taskSummary->setText(tr("共 %1 个任务").arg(m_taskTable->rowCount()));
        statusBar()->showMessage(tr("任务 %1 已取消").arg(id), 3000);
    }, Qt::QueuedConnection);
}

void MainWindow::showFatalAndQuit(const QString &why)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle(tr("数据库初始化失败"));
    box.setText(why);
    box.setStandardButtons(QMessageBox::Close);
    box.setDefaultButton(QMessageBox::Close);

    box.exec();

    QCoreApplication::quit();
}
