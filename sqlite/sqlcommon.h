#ifndef SQLCOMMON_H
#define SQLCOMMON_H

#include <QSqlDatabase>
#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>

namespace Sqlcommon{

inline QString makeDbPath(){
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/qtfetch.db");
}

inline QSqlDatabase getConnection(const QString &connName){
    return QSqlDatabase::database(connName);
}

inline bool setConnection(const QString &connName, QString *errorOut){
    QSqlDatabase db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connName);
    db.setDatabaseName(makeDbPath());
    if(!db.open()){
        if(errorOut){
            *errorOut=db.lastError().text();
        }
        const QString failedName = db.connectionName();
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(failedName);
        return false;
    }
    else{
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));    // 并发读写更友好
        pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));  // WAL 下的推荐值
        pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));   // 撞锁时等 5 秒而非立刻失败
        pragma.exec(QStringLiteral("PRAGMA foreign_keys=ON"));     // SQLite 默认是关的
        if(errorOut){
            errorOut->clear();
        }
        return true;
    }
}

inline void closeConnection(const QString &connName){
    if (!QSqlDatabase::contains(connName))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(connName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(connName);
}

}

#endif // SQLCOMMON_H
