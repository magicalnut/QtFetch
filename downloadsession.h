#ifndef DOWNLOADSESSION_H
#define DOWNLOADSESSION_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

namespace download { class Worker; }

class DownloadSession : public QObject
{
    Q_OBJECT
public:
    explicit DownloadSession(QObject *parent = nullptr);
    ~DownloadSession() override;

    void start(qint64 taskId, const QString &url, const QString &savePath, int threads);
    void stop();

signals:
    void progressChanged(qint64 taskId, int percent);
    void finished(qint64 taskId, bool ok);

private slots:
    void poll();

private:
    std::unique_ptr<download::Worker> m_worker;
    QTimer *m_timer = nullptr;
    qint64 m_taskId = -1;
};

#endif // DOWNLOADSESSION_H
