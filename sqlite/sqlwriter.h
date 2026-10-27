#ifndef SQLWRITER_H
#define SQLWRITER_H

#include "taskrecord.h"

#include <QList>
#include <QObject>
#include <QString>

class SqlWriter : public QObject
{
    Q_OBJECT
public:
    explicit SqlWriter(QObject *parent = nullptr);
    ~SqlWriter() override;
    void init();

public slots:
    // 建任务：默认 state = 'paused'，等用户点「开始」再下载
    void addTask(const QString &url, const QString &savePath, int threads);
    void updateState(qint64 id, const QString &state);
    void removeTask(qint64 id);

    // 完成：把行从 tasks 搬到 completed（同一个事务里）
    void finishTask(qint64 id);

    // 已完成记录
    void loadCompleted();
    void removeCompleted(qint64 completedId);
    void clearCompleted();

signals:
    void errorOccurred(const QString &message);
    void initsuccess();
    void initfail();
    void tablefail(int num);

    void taskCreated(qint64 id, const QString &url, const QString &savePath, int threads);
    void taskInsertFailed(const QString &message);
    void taskFinished(qint64 completedId, qint64 taskId);
    void taskRemoved(qint64 id);
    void stateUpdated(qint64 id, const QString &state);

    void completedLoaded(const QList<TaskRecord> &rows);
    void completedRemoved(qint64 completedId);
    void completedCleared();

private:
    QString connerror;
};

#endif // SQLWRITER_H
