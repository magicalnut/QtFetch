#ifndef SQLREADER_H
#define SQLREADER_H

#include "taskrecord.h"

#include <QList>
#include <QObject>
#include <QString>

class SqlReader : public QObject
{
    Q_OBJECT
public:
    explicit SqlReader(QObject *parent = nullptr);
    ~SqlReader() override;
    void init();

private:
    QString connerror;

public slots:
    void readall();

signals:
    void tasksLoaded(const QList<TaskRecord> &tasks);
    void completedLoaded(const QList<TaskRecord> &rows);
    void errorOccurred(const QString &message);
    void finished();
    void initsuccess();
    void initfail();
};

#endif // SQLREADER_H
