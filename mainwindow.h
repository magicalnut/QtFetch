#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "downloadsession.h"
#include "sqlite/sqlreader.h"
#include "sqlite/sqlwriter.h"

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QThread>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    // 主线程 → SQL 写线程
    void taskInsertRequested(const QString &url, const QString &savePath, int threads);
    void stateUpdateRequested(qint64 id, const QString &state);
    void taskRemoveRequested(qint64 id);
    void taskFinishRequested(qint64 id);
    void completedRemoveRequested(qint64 completedId);
    void completedClearRequested();

private slots:
    // 导航栏
    void onStartAll();
    void onPauseAll();
    void onClearFinished();

    // 下载参数 / 任务操作
    void onStartDownload();
    void onPickSaveDir();
    void onTaskToggle(int row);
    void onTaskCancel(int row);

    // SQL 线程回传
    void onTasksLoaded(const QList<TaskRecord> &tasks);
    void onCompletedLoaded(const QList<TaskRecord> &rows);
    void onTaskFinished(qint64 completedId, qint64 taskId);
    void onCompletedRemoved(qint64 completedId);
    void onCompletedCleared();

    void showFatalAndQuit(const QString &why);

private:
    void setupToolBar();
    void setupCentralArea();
    QWidget *createParamPanel();
    QWidget *createTaskPanel();

    // 下载会话
    DownloadSession *createSession(qint64 taskId);
    void startTask(qint64 taskId);
    void setToggleText(qint64 id, const QString &text);

    // 表格
    void appendTaskRow(qint64 id, const QString &url);
    void appendCompletedRow(const TaskRecord &row);
    int  rowOfTaskId(qint64 id) const;
    int  rowOfCompletedId(qint64 id) const;
    void refreshSummaries();

    // 参数区
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_fileNameEdit = nullptr;
    QLineEdit *m_saveDirEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QSpinBox *m_threadSpin = nullptr;
    QPushButton *m_startButton = nullptr;

    // 任务列表
    QTabWidget *m_tabs = nullptr;
    QTableWidget *m_taskTable = nullptr;       // 「下载中」页
    QTableWidget *m_completedTable = nullptr;  // 「已完成」页
    QLabel *m_taskSummary = nullptr;           // 标题栏上的统计

    // 内存里的任务参数（启动下载需要 url / savePath / threads）
    QHash<qint64, TaskRecord> m_taskInfo;
    QHash<qint64, DownloadSession*> m_sessions;

    // SQL 读写线程
    SqlReader *reader = nullptr;
    QThread *read_thread = nullptr;
    SqlWriter *writer = nullptr;
    QThread *write_thread = nullptr;
};

#endif // MAINWINDOW_H
