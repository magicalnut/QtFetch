#ifndef SQLWRITER_H
#define SQLWRITER_H

#include <QObject>
#include "taskrecord.h"

class SqlWriter : public QObject
{
    Q_OBJECT
public:
    explicit SqlWriter(QObject *parent = nullptr);

signals:
};

#endif // SQLWRITER_H
