#include "sqlwriter.h"
#include "sqlcommon.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

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
            state        TEXT    NOT NULL DEFAULT 'waiting',
            sha256       TEXT,
            error        TEXT,
            created_at   INTEGER NOT NULL DEFAULT (strftime('%s','now')),
            file_mtime   INTEGER
        ))");

    if (!q.exec(createTasks)) {
        emit tablefail(1);
        return;
    }

    const QString createSegments = QStringLiteral(R"(
        CREATE TABLE IF NOT EXISTS segments (
            task_id       INTEGER NOT NULL,
            segment_index INTEGER NOT NULL,
            start_byte    INTEGER NOT NULL,
            end_byte      INTEGER NOT NULL,
            position      INTEGER NOT NULL,
            state         TEXT    NOT NULL DEFAULT 'waiting',
            updated_at    INTEGER NOT NULL DEFAULT (strftime('%s','now')),
            PRIMARY KEY (task_id, segment_index),
            FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
        ))");

    if (!q.exec(createSegments)) {
        emit tablefail(2);
        return;
    }

    emit initsuccess();
}

void SqlWriter::addTask(const QString &url, const QString &savePath, int threads)
{
    QSqlDatabase db=getConnection(connName);
    QSqlQuery q(db);

    q.prepare(QStringLiteral(
        "INSERT INTO tasks (url, save_path, thread_count, state) "
        "VALUES (:url, :path, :threads, 'waiting')"));
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
