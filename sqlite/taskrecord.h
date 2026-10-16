#ifndef TASKRECORD_H
#define TASKRECORD_H

#include <QString>

struct TaskRecord{
    qint64 id=0;
    QString url;
    QString savePath;
    int threadCount=1;
    qint64 totalBytes=-1;
    QString state;
    QString sha256;
    qint64 createdAt=0;
};

#endif // TASKRECORD_H
