#include "sqlreader.h"
#include "sqlcommon.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Sqlcommon;
namespace{
const char *connName="readconn";
}

SqlReader::SqlReader(QObject *parent)
    : QObject{parent}
{
    connerror.clear();
}

SqlReader::~SqlReader(){
    closeConnection(connName);
}

void SqlReader::init(){
    if(!setConnection(connName,&connerror)){
        emit errorOccurred(connerror);
        emit initfail();
        return ;
    }
    emit initsuccess();
}

void SqlReader::readall(){
    auto db=getConnection(connName);

    // ---- 未完成的任务 ----
    {
        QSqlQuery q(db);

        QList<TaskRecord> tasks;

        if (!q.exec(QStringLiteral(
                "SELECT id, url, save_path, thread_count, total_bytes, "
                "       state, sha256, created_at "
                "FROM tasks ORDER BY id ASC"))) {
            emit errorOccurred(q.lastError().text());
            emit finished();
            return;
        }

        while (q.next()) {
            TaskRecord r;
            r.id          = q.value(0).toLongLong();
            r.url         = q.value(1).toString();
            r.savePath    = q.value(2).toString();
            r.threadCount = q.value(3).toInt();
            r.totalBytes  = q.value(4).isNull() ? -1 : q.value(4).toLongLong();
            r.state       = q.value(5).toString();
            r.sha256      = q.value(6).toString();
            r.createdAt   = q.value(7).toLongLong();
            tasks.append(r);
        }

        emit tasksLoaded(tasks);
    }

    // ---- 已完成记录 ----
    {
        QSqlQuery q(db);

        QList<TaskRecord> rows;

        if (q.exec(QStringLiteral(
                "SELECT id, url, save_path, total_bytes, finished_at "
                "FROM completed ORDER BY finished_at DESC, id DESC"))) {
            while (q.next()) {
                TaskRecord r;
                r.id         = q.value(0).toLongLong();
                r.url        = q.value(1).toString();
                r.savePath   = q.value(2).toString();
                r.totalBytes = q.value(3).isNull() ? -1 : q.value(3).toLongLong();
                r.createdAt  = q.value(4).toLongLong();
                rows.append(r);
            }
        } else {
            // 老库可能还没有 completed 表，不致命，只提示
            emit errorOccurred(q.lastError().text());
        }

        emit completedLoaded(rows);
    }

    emit finished();
}
