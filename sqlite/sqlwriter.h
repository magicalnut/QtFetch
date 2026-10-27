#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <QObject>
#include "taskrecord.h"

class SqlWriter : public QObject
{
    Q_OBJECT
public:
    explicit SqlWriter(QObject *parent = nullptr);
    ~SqlWriter()override;
    void init();

private:
    QString connerror;

public slots:
    void addTask(const QString &url, const QString &savePath, int threads);

signals:
    void tasksLoaded(const QList<TaskRecord> &tasks);
    void errorOccurred(const QString &message);
    void finished();
    void initsuccess();
    void initfail();
    void tablefail(int num);
    void taskCreated(qint64 id, const QString &url);
    void taskInsertFailed(const QString &message);
};

#endif // SQLWRITER_H
