#include "mainwindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// 左侧参数区：字段宽度与表单节奏
constexpr int kLeftPanelMinWidth = 280;
constexpr int kLeftPanelMaxWidth = 460;
constexpr int kThreadCountDefault = 4;
constexpr int kSectionSpacing = 12;

// 「下载中」表格的列
enum TaskColumn {
    ColId = 0,
    ColUrl,
    ColProgress,
    ColControl,
    ColCancel,
    ColumnCount
};

// 「已完成」表格的列
enum CompletedColumn {
    CColUrl = 0,
    CColFile,
    CColOpen,
    CColDelete,
    CColCount
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

// 两张表共用的外观设置
void styleTable(QTableWidget *table)
{
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setWordWrap(false);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->setDefaultSectionSize(38);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("QtFetch 下载器"));
    resize(1120, 700);

    // ------------------------------------------------------------ 写线程 ----
    write_thread = new QThread();
    writer = new SqlWriter();                 // 不能给 parent，否则 moveToThread 会失败
    writer->moveToThread(write_thread);

    connect(write_thread, &QThread::started, writer, &SqlWriter::init);
    connect(write_thread, &QThread::finished, writer, &QObject::deleteLater);

    connect(writer, &SqlWriter::initfail, this, [this]() {
        showFatalAndQuit(tr("无法打开 / 创建数据库"));
    });
    connect(writer, &SqlWriter::tablefail, this, [this](int num) {
        showFatalAndQuit(tr("数据库表 %1 建立失败").arg(num));
    });
    connect(writer, &SqlWriter::errorOccurred, this, [](const QString &e) {
        qWarning() << "SqlWriter:" << e;
    });
    connect(writer, &SqlWriter::initsuccess, this, [this]() {
        statusBar()->showMessage(tr("数据库初始化完成"), 3000);
    });

    // 主线程 → 写线程
    connect(this, &MainWindow::taskInsertRequested,  writer, &SqlWriter::addTask);
    connect(this, &MainWindow::stateUpdateRequested, writer, &SqlWriter::updateState);
    connect(this, &MainWindow::taskRemoveRequested,  writer, &SqlWriter::removeTask);
    connect(this, &MainWindow::taskFinishRequested,  writer, &SqlWriter::finishTask);
    connect(this, &MainWindow::completedRemoveRequested, writer, &SqlWriter::removeCompleted);
    connect(this, &MainWindow::completedClearRequested,  writer, &SqlWriter::clearCompleted);

    // 写线程 → 主线程
    connect(writer, &SqlWriter::taskCreated, this,
            [this](qint64 id, const QString &url, const QString &savePath, int threads) {
        TaskRecord info;
        info.id          = id;
        info.url         = url;
        info.savePath    = savePath;
        info.threadCount = threads;
        info.state       = QStringLiteral("paused");
        m_taskInfo.insert(id, info);

        appendTaskRow(id, url);
        setToggleText(id, tr("开始"));       // 建出来是暂停态，等用户点开始
        if (m_tabs)
            m_tabs->setCurrentIndex(0);

        statusBar()->showMessage(tr("任务 %1 已创建，点「开始」开始下载").arg(id), 4000);
    });

    connect(writer, &SqlWriter::taskInsertFailed, this, [this](const QString &msg) {
        statusBar()->showMessage(tr("创建任务失败：%1").arg(msg), 6000);
    });
    connect(writer, &SqlWriter::taskFinished,      this, &MainWindow::onTaskFinished);
    connect(writer, &SqlWriter::completedRemoved,  this, &MainWindow::onCompletedRemoved);
    connect(writer, &SqlWriter::completedCleared,  this, &MainWindow::onCompletedCleared);
    connect(writer, &SqlWriter::completedLoaded,   this, &MainWindow::onCompletedLoaded);

    // ------------------------------------------------- 读线程（跑一次就结束）----
    read_thread = new QThread();
    reader = new SqlReader();
    reader->moveToThread(read_thread);

    connect(read_thread, &QThread::started, reader, &SqlReader::init);
    connect(read_thread, &QThread::finished, reader, &QObject::deleteLater);
    connect(reader, &SqlReader::initsuccess, reader, &SqlReader::readall);
    connect(reader, &SqlReader::errorOccurred, this, [](const QString &e) {
        qWarning() << "SqlReader:" << e;
    });
    connect(reader, &SqlReader::tasksLoaded,     this, &MainWindow::onTasksLoaded);
    connect(reader, &SqlReader::completedLoaded, this, &MainWindow::onCompletedLoaded);

    // 读线程读完（或失败）就退出阻塞；超时兜底防止永远卡住
    QEventLoop loop;
    connect(reader, &SqlReader::finished, &loop, &QEventLoop::quit);
    connect(reader, &SqlReader::initfail, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);

    // 建表成功之后才启动读线程，否则会撞上「表不存在」
    connect(writer, &SqlWriter::initsuccess, this, [this]() {
        read_thread->start();
    });

    write_thread->start();

    setupToolBar();
    setupCentralArea();

    loop.exec();

    refreshSummaries();
    statusBar()->showMessage(tr("就绪"), 2000);
}

MainWindow::~MainWindow()
{
    // 1. 停掉所有下载会话。stop() 内部把 Worker 交给后台线程销毁，不会阻塞这里。
    const QList<DownloadSession *> sessions = m_sessions.values();
    m_sessions.clear();
    for (auto *s : sessions) {
        s->stop();
        s->deleteLater();
    }

    // 2. 停 SQL 线程：quit 让事件循环退出，wait 等它真的结束，
    //    期间 finished → deleteLater 会把 writer / reader 自己也收掉。
    if (write_thread) {
        write_thread->quit();
        write_thread->wait(5000);
        delete write_thread;
        write_thread = nullptr;
    }
    if (read_thread) {
        read_thread->quit();
        read_thread->wait(5000);
        delete read_thread;
        read_thread = nullptr;
    }
}

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
    startAllAction->setToolTip(tr("启动所有还没开始的任务"));
    connect(startAllAction, &QAction::triggered, this, &MainWindow::onStartAll);

    auto *pauseAllAction = bar->addAction(tr("全部暂停"));
    pauseAllAction->setToolTip(tr("暂停所有正在下载的任务"));
    connect(pauseAllAction, &QAction::triggered, this, &MainWindow::onPauseAll);

    auto *clearAction = bar->addAction(tr("清空已完成"));
    clearAction->setToolTip(tr("清空「已完成」页里的所有记录"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::onClearFinished);
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
    m_urlEdit->setPlaceholderText(tr("http://example.com/file.zip"));
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

    // --- 创建任务 ---
    m_startButton = new QPushButton(tr("创建任务"), panel);
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

    // 左列到此为止：上面是参数，下面是创建按钮。剩余空间留白，
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

    // --- 下载中：任务表格 ---
    m_taskTable = new QTableWidget(0, ColumnCount, panel);
    m_taskTable->setHorizontalHeaderLabels(
        {tr("任务ID"), tr("下载链接"), tr("下载进度"), tr("操作"), tr("取消")});
    styleTable(m_taskTable);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColId, QHeaderView::ResizeToContents);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColUrl, QHeaderView::Stretch);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColProgress, QHeaderView::Fixed);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColControl, QHeaderView::Fixed);
    m_taskTable->horizontalHeader()->setSectionResizeMode(ColCancel, QHeaderView::Fixed);
    m_taskTable->setColumnWidth(ColProgress, 200);
    m_taskTable->setColumnWidth(ColControl, 96);
    m_taskTable->setColumnWidth(ColCancel, 80);

    // --- 已完成：记录表格 ---
    m_completedTable = new QTableWidget(0, CColCount, panel);
    m_completedTable->setHorizontalHeaderLabels(
        {tr("下载链接"), tr("文件名"), tr("打开位置"), tr("删除记录")});
    styleTable(m_completedTable);
    m_completedTable->horizontalHeader()->setSectionResizeMode(CColUrl, QHeaderView::Stretch);
    m_completedTable->horizontalHeader()->setSectionResizeMode(CColFile, QHeaderView::Fixed);
    m_completedTable->horizontalHeader()->setSectionResizeMode(CColOpen, QHeaderView::Fixed);
    m_completedTable->horizontalHeader()->setSectionResizeMode(CColDelete, QHeaderView::Fixed);
    m_completedTable->setColumnWidth(CColFile, 200);
    m_completedTable->setColumnWidth(CColOpen, 100);
    m_completedTable->setColumnWidth(CColDelete, 100);

    auto *runningPage = new QWidget(panel);
    auto *runningLayout = new QVBoxLayout(runningPage);
    runningLayout->setContentsMargins(0, 6, 0, 0);
    runningLayout->addWidget(m_taskTable);

    auto *donePage = new QWidget(panel);
    auto *doneLayout = new QVBoxLayout(donePage);
    doneLayout->setContentsMargins(0, 6, 0, 0);
    doneLayout->addWidget(m_completedTable);

    m_tabs = new QTabWidget(panel);
    m_tabs->addTab(runningPage, tr("下载中"));
    m_tabs->addTab(donePage, tr("已完成"));
    layout->addWidget(m_tabs, 1);

    refreshSummaries();
    return panel;
}

// ------------------------------------------------------------ 下载会话 ----

DownloadSession *MainWindow::createSession(qint64 taskId)
{
    Q_UNUSED(taskId);

    auto *session = new DownloadSession(this);

    connect(session, &DownloadSession::progressChanged, this,
            [this](qint64 tid, int percent) {
        const int row = rowOfTaskId(tid);
        if (row < 0)
            return;
        if (auto *bar = qobject_cast<QProgressBar *>(
                m_taskTable->cellWidget(row, ColProgress)))
            bar->setValue(percent);
    });

    connect(session, &DownloadSession::finished, this, [this](qint64 tid, bool ok) {
        if (auto *s = m_sessions.take(tid))
            s->deleteLater();

        if (ok) {
            setToggleText(tid, tr("已完成"));
            emit taskFinishRequested(tid);   // 搬到 completed 表
        } else {
            setToggleText(tid, tr("重试"));
            emit stateUpdateRequested(tid, QStringLiteral("failed"));
            statusBar()->showMessage(tr("任务 %1 下载失败").arg(tid), 5000);
        }
    });

    return session;
}

void MainWindow::startTask(qint64 taskId)
{
    if (m_sessions.contains(taskId))
        return;
    if (!m_taskInfo.contains(taskId))
        return;

    const TaskRecord info = m_taskInfo.value(taskId);

    auto *session = createSession(taskId);
    m_sessions.insert(taskId, session);

    // 重新下载会清空文件，进度条归零
    const int row = rowOfTaskId(taskId);
    if (row >= 0) {
        if (auto *bar = qobject_cast<QProgressBar *>(
                m_taskTable->cellWidget(row, ColProgress)))
            bar->setValue(0);
    }

    setToggleText(taskId, tr("暂停"));
    emit stateUpdateRequested(taskId, QStringLiteral("downloading"));

    session->start(taskId, info.url, info.savePath, info.threadCount);
}

void MainWindow::setToggleText(qint64 id, const QString &text)
{
    const int row = rowOfTaskId(id);
    if (row < 0)
        return;
    if (auto *btn = qobject_cast<QPushButton *>(
            m_taskTable->cellWidget(row, ColControl)))
        btn->setText(text);
}

// -------------------------------------------------------- 列表/表格更新 ----

void MainWindow::appendTaskRow(qint64 id, const QString &url)
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

    // 新任务默认「开始」：建出来是暂停态，等用户点
    auto *toggleButton = new QPushButton(tr("开始"), m_taskTable);
    toggleButton->setToolTip(tr("开始 / 暂停 / 继续该任务"));
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
    cancelButton->setStyleSheet(QStringLiteral("QPushButton { color: #d64545; }"));
    m_taskTable->setCellWidget(row, ColCancel, cancelButton);
    connect(cancelButton, &QPushButton::clicked, this, [this, cancelButton]() {
        for (int r = 0; r < m_taskTable->rowCount(); ++r) {
            if (m_taskTable->cellWidget(r, ColCancel) == cancelButton) {
                onTaskCancel(r);
                return;
            }
        }
    });

    refreshSummaries();
}

void MainWindow::appendCompletedRow(const TaskRecord &rowData)
{
    const int row = m_completedTable->rowCount();
    m_completedTable->insertRow(row);

    auto *urlItem = new QTableWidgetItem(rowData.url);
    urlItem->setToolTip(rowData.url);
    urlItem->setData(Qt::UserRole, rowData.id);      // 记住 completed 表的 id
    m_completedTable->setItem(row, CColUrl, urlItem);

    auto *fileItem = new QTableWidgetItem(QFileInfo(rowData.savePath).fileName());
    fileItem->setToolTip(rowData.savePath);
    m_completedTable->setItem(row, CColFile, fileItem);

    // 打开文件所在目录
    auto *openButton = new QPushButton(tr("打开位置"), m_completedTable);
    m_completedTable->setCellWidget(row, CColOpen, openButton);
    const QString folder = QFileInfo(rowData.savePath).absolutePath();
    connect(openButton, &QPushButton::clicked, this, [folder]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
    });

    // 只删数据库记录，不动磁盘文件
    auto *deleteButton = new QPushButton(tr("删除记录"), m_completedTable);
    deleteButton->setStyleSheet(QStringLiteral("QPushButton { color: #d64545; }"));
    deleteButton->setToolTip(tr("仅删除记录，不删除已下载的文件"));
    m_completedTable->setCellWidget(row, CColDelete, deleteButton);
    const qint64 completedId = rowData.id;
    connect(deleteButton, &QPushButton::clicked, this, [this, completedId]() {
        emit completedRemoveRequested(completedId);
    });

    refreshSummaries();
}

int MainWindow::rowOfTaskId(qint64 id) const
{
    for (int r = 0; r < m_taskTable->rowCount(); ++r) {
        const QTableWidgetItem *item = m_taskTable->item(r, ColId);
        if (item && item->text().toLongLong() == id)
            return r;
    }
    return -1;
}

int MainWindow::rowOfCompletedId(qint64 id) const
{
    if (!m_completedTable)
        return -1;
    for (int r = 0; r < m_completedTable->rowCount(); ++r) {
        const QTableWidgetItem *item = m_completedTable->item(r, CColUrl);
        if (item && item->data(Qt::UserRole).toLongLong() == id)
            return r;
    }
    return -1;
}

void MainWindow::refreshSummaries()
{
    if (!m_taskSummary)
        return;

    const int running = m_taskTable ? m_taskTable->rowCount() : 0;
    const int done = m_completedTable ? m_completedTable->rowCount() : 0;
    m_taskSummary->setText(tr("下载中 %1 · 已完成 %2").arg(running).arg(done));
}

// ------------------------------------------------ SQL 线程回传的处理槽 ----

void MainWindow::onTasksLoaded(const QList<TaskRecord> &tasks)
{
    for (const TaskRecord &r : tasks) {
        m_taskInfo.insert(r.id, r);
        appendTaskRow(r.id, r.url);

        // 上个进程没下完的任务：没有 session，也不能续传，
        // 按钮写「重新下载」而不是「继续」，如实反映「会从 0 开始」。
        setToggleText(r.id, tr("重新下载"));
    }

    refreshSummaries();
    statusBar()->showMessage(tr("已载入 %1 个未完成的任务").arg(tasks.size()));
}

void MainWindow::onCompletedLoaded(const QList<TaskRecord> &rows)
{
    for (const TaskRecord &r : rows)
        appendCompletedRow(r);

    refreshSummaries();
}

void MainWindow::onTaskFinished(qint64 completedId, qint64 taskId)
{
    // 1. 从「下载中」移除
    const int row = rowOfTaskId(taskId);
    if (row >= 0)
        m_taskTable->removeRow(row);

    // 2. 加到「已完成」
    if (m_taskInfo.contains(taskId)) {
        TaskRecord done = m_taskInfo.take(taskId);
        done.id = completedId;        // 换成 completed 表的 id，删除记录时要用
        appendCompletedRow(done);
    }

    refreshSummaries();
    statusBar()->showMessage(tr("任务 %1 已完成").arg(taskId), 3000);
}

void MainWindow::onCompletedRemoved(qint64 completedId)
{
    const int row = rowOfCompletedId(completedId);
    if (row >= 0)
        m_completedTable->removeRow(row);

    refreshSummaries();
    statusBar()->showMessage(tr("已删除完成记录"), 3000);
}

void MainWindow::onCompletedCleared()
{
    m_completedTable->setRowCount(0);
    refreshSummaries();
    statusBar()->showMessage(tr("已清空完成记录"), 3000);
}

// -------------------------------------------------------------- 导航栏槽 ----

void MainWindow::onStartAll()
{
    const QList<qint64> ids = m_taskInfo.keys();
    int started = 0;

    for (qint64 id : ids) {
        if (m_sessions.contains(id))
            continue;                 // 已经在跑
        startTask(id);
        ++started;
    }

    statusBar()->showMessage(started ? tr("已开始 %1 个任务").arg(started)
                                     : tr("没有可开始的任务"), 3000);
}

void MainWindow::onPauseAll()
{
    int paused = 0;

    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        it.value()->pause();
        setToggleText(it.key(), tr("继续"));
        emit stateUpdateRequested(it.key(), QStringLiteral("paused"));
        ++paused;
    }

    statusBar()->showMessage(paused ? tr("已暂停 %1 个任务").arg(paused)
                                    : tr("没有正在下载的任务"), 3000);
}

void MainWindow::onClearFinished()
{
    if (!m_completedTable || m_completedTable->rowCount() == 0) {
        statusBar()->showMessage(tr("没有已完成的记录"), 3000);
        return;
    }
    emit completedClearRequested();
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
    if (!idItem)
        return;
    const qint64 id = idItem->text().toLongLong();

    // 没有 session：还没开始（新任务 / 重启恢复 / 失败重试）→ 直接启动
    if (!m_sessions.contains(id)) {
        startTask(id);
        return;
    }

    auto *session = m_sessions.value(id);
    const bool pausing = (button->text() == tr("暂停"));

    if (pausing)
        session->pause();
    else
        session->resume();

    button->setText(pausing ? tr("继续") : tr("暂停"));
    emit stateUpdateRequested(id, pausing ? QStringLiteral("paused")
                                          : QStringLiteral("downloading"));

    statusBar()->showMessage(pausing ? tr("任务 %1 已暂停").arg(id)
                                     : tr("任务 %1 已继续").arg(id), 3000);
}

void MainWindow::onTaskCancel(int row)
{
    const QTableWidgetItem *idItem = m_taskTable->item(row, ColId);
    if (!idItem)
        return;
    const qint64 id = idItem->text().toLongLong();

    m_taskInfo.remove(id);

    if (auto *s = m_sessions.take(id)) {      // take：同时从 map 里移除
        s->stop();                            // 释放文件句柄（Worker 交给后台线程销毁）
        s->deleteLater();
    }

    emit taskRemoveRequested(id);

    // 不能在这里直接 removeRow：本函数是从「取消」按钮的 clicked 里进来的，
    // 当场删行会销毁那个按钮（等于在信号处理中删掉发送者）。排到队列里再做。
    QMetaObject::invokeMethod(this, [this, row, id]() {
        m_taskTable->removeRow(row);
        refreshSummaries();
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
