#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "sqlite/sqlreader.h"
#include "sqlite/sqlwriter.h"

#include <QMainWindow>
#include <QString>
#include <QThread>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 导航栏
    void onStartAll();
    void onPauseAll();
    void onClearFinished();
    void onConcurrentChanged(int count);

    // 下载参数 / 任务操作
    void onStartDownload();
    void onPickSaveDir();
    void onTaskToggle(int row);
    void onTaskCancel(int row);
    void showFatalAndQuit(const QString &why);

private:
    void setupToolBar();
    void setupCentralArea();
    QWidget *createParamPanel();
    QWidget *createTaskPanel();

    // 往任务列表追加一行（骨架，后续接真实下载逻辑）
    void appendTaskRow(const QString &url);

    // 参数区
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_fileNameEdit = nullptr;
    QLineEdit *m_saveDirEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QSpinBox *m_threadSpin = nullptr;
    QPushButton *m_startButton = nullptr;

    // 导航栏
    QComboBox *m_concurrentCombo = nullptr;

    // 任务列表
    QTableWidget *m_taskTable = nullptr;  // 右侧：任务表格
    QLabel *m_taskSummary = nullptr;      // 右侧标题栏上的统计信息

    //sql读线程
    SqlReader *reader;
    QThread *read_thread;
    //sql写线程
    SqlWriter *writer;
    QThread *write_thread;
};

#endif // MAINWINDOW_H
