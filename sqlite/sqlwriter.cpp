#include "sqlwriter.h"
#include "sqlcommon.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Sqlcommon;
namespace{
const char *connName="writeconn";
}

SqlWriter::SqlWriter(QObject *parent)
    : QObject{parent}
{
    connerror.clear();
}

SqlWriter::~SqlWriter(){
    closeConnection(connName);
}

void SqlWriter::init(){
    if(!setConnection(connName,&connerror)){
        emit errorOccurred(connerror);
        emit initfail();
        return ;
    }

    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);

    const QString createTasks = QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            url          TEXT    NOT NULL UNIQUE,
            save_path    TEXT    NOT NULL,
            thread_count INTEGER NOT NULL DEFAULT 1,
            total_bytes  INTEGER,
            state        TEXT    NOT NULL DEFAULT 'paused',
            sha256       TEXT,
            error        TEXT,
            created_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')),
            file_mtime   INTEGER
        ))");

    if (!q.exec(createTasks)) {
        emit errorOccurred(q.lastError().text());
        emit tablefail(1);
        return;
    }

    // 已完成记录。（原来的 segments 表已删除：断点续传没做，那张表从未写入过）
    const QString createCompleted = QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS completed (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            url         TEXT    NOT NULL,
            save_path   TEXT    NOT NULL,
            total_bytes INTEGER,
            finished_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))
        ))");

    if (!q.exec(createCompleted)) {
        emit errorOccurred(q.lastError().text());
        emit tablefail(2);
        return;
    }

    emit initsuccess();
}

void SqlWriter::addTask(const QString &url, const QString &savePath, int threads)
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);

    // 建出来就是暂停态：等用户点「开始」才真正下载
    q.prepare(QStringLiteral(
        "INSERT INTO tasks (url, save_path, thread_count, state) "
        "VALUES (:url, :path, :threads, 'paused')"));
    q.bindValue(QStringLiteral(":url"),     url);
    q.bindValue(QStringLiteral(":path"),    savePath);
    q.bindValue(QStringLiteral(":threads"), threads);

    if(!q.exec()){
        emit taskInsertFailed(q.lastError().text());
        return;
    }

    const qint64 id = q.lastInsertId().toLongLong();
    emit taskCreated(id, url, savePath, threads);
}

void SqlWriter::updateState(qint64 id, const QString &state)
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE tasks SET state=:state WHERE id=:id"));
    q.bindValue(QStringLiteral(":state"), state);
    q.bindValue(QStringLiteral(":id"),    id);

    if(!q.exec()){
        emit errorOccurred(q.lastError().text());
        return;
    }
    emit stateUpdated(id, state);
}

void SqlWriter::removeTask(qint64 id)
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM tasks WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), id);

    if(!q.exec()){
        emit errorOccurred(q.lastError().text());
        return;
    }
    emit taskRemoved(id);
}

void SqlWriter::finishTask(qint64 id)
{
    QSqlDatabase db=getConnection(connName);
    if(!db.transaction()){
        emit errorOccurred(db.lastError().text());
        return;
    }

    qint64 completedId = 0;

    // 先搬进 completed
    {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral(
            "INSERT INTO completed (url, save_path, total_bytes) "
            "SELECT url, save_path, total_bytes FROM tasks WHERE id=:id"));
        ins.bindValue(QStringLiteral(":id"), id);

        if(!ins.exec()){
            emit errorOccurred(ins.lastError().text());
            db.rollback();
            return;
        }
        completedId = ins.lastInsertId().toLongLong();
    }

    // 再从 tasks 删掉
    {
        QSqlQuery del(db);
        del.prepare(QStringLiteral("DELETE FROM tasks WHERE id=:id"));
        del.bindValue(QStringLiteral(":id"), id);

        if(!del.exec()){
            emit errorOccurred(del.lastError().text());
            db.rollback();
            return;
        }
    }

    if(!db.commit()){
        emit errorOccurred(db.lastError().text());
        db.rollback();
        return;
    }

    emit taskFinished(completedId, id);
}

void SqlWriter::loadCompleted()
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);

    QList<TaskRecord> rows;

    if(!q.exec(QStringLiteral(
            "SELECT id, url, save_path, total_bytes, finished_at "
            "FROM completed ORDER BY finished_at DESC, id DESC"))){
        emit errorOccurred(q.lastError().text());
        return;
    }

    while(q.next()){
        TaskRecord r;
        r.id         = q.value(0).toLongLong();
        r.url        = q.value(1).toString();
        r.savePath   = q.value(2).toString();
        r.totalBytes = q.value(3).isNull() ? -1 : q.value(3).toLongLong();
        r.createdAt  = q.value(4).toLongLong();
        rows.append(r);
    }

    emit completedLoaded(rows);
}

void SqlWriter::removeCompleted(qint64 completedId)
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM completed WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), completedId);

    if(!q.exec()){
        emit errorOccurred(q.lastError().text());
        return;
    }
    emit completedRemoved(completedId);
}

void SqlWriter::clearCompleted()
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);

    if(!q.exec(QStringLiteral("DELETE FROM completed"))){
        emit errorOccurred(q.lastError().text());
        return;
    }
    emit completedCleared();
}
