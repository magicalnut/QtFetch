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

signals:
    void tasksLoaded(const QList<TaskRecord> &tasks);
    void errorOccurred(const QString &message);
    void finished();
    void initsuccess();
    void initfail();
    void tablefail(int num);
};

#endif // SQLWRITER_H
