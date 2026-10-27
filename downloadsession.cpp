#include "downloadsession.h"
#include "client.hpp"

#include <QDebug>
#include <exception>

DownloadSession::DownloadSession(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &DownloadSession::poll);
}

DownloadSession::~DownloadSession() = default;

void DownloadSession::start(qint64 taskId, const QString &url,const QString &savePath, int threads)
{
    m_taskId = taskId;
    try {
        m_worker = std::make_unique<download::Worker>(url.toStdString(),savePath.toStdString(),threads);
        m_worker->run();       // 同步做 HEAD，然后启动下载线程
    } catch (const std::exception &e) {
        qWarning() << "下载启动失败:" << e.what();
        emit finished(taskId, false);
        return;
    }
    m_timer->start();
}

void DownloadSession::poll()
{
    if (!m_worker) return;

    const double p = m_worker->get_progress();

    // HEAD 失败时 need_download_size 还是 -1，进度会是负数
    if (p < 0.0) {
        m_timer->stop();
        m_worker.reset();               // ← 释放文件句柄
        emit finished(m_taskId, false);
        return;
    }

    emit progressChanged(m_taskId, qBound(0, int(p * 100.0), 100));

    if (p >= 1.0) {
        m_timer->stop();
        m_worker.reset();               // Worker 析构才会 close(file_fd)
        emit finished(m_taskId, true);
    }
}

void DownloadSession::stop()
{
    m_timer->stop();
    m_worker.reset();
}