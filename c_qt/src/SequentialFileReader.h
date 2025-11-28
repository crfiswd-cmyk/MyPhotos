#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QHash>
#include <QThread>

// Single-threaded sequential file reader (intended for HDD to avoid random seeks).
class SequentialFileReader : public QObject {
    Q_OBJECT
public:
    static SequentialFileReader& instance();

    QByteArray readFile(const QString& path);
    // Prefetch a list of paths into an internal cache (HDD mode).
    void prefetchFiles(const QStringList& paths, qint64 maxBytes = 32 * 1024 * 1024);

private:
    SequentialFileReader();
    ~SequentialFileReader();

    static constexpr qint64 kChunkSize = 4 * 1024 * 1024; // 4MB buffered read to reduce random I/O cost on HDD

    struct Task {
        QString path;
        QByteArray* out;
        QWaitCondition* done;
        bool storeInCache{false};
    };

    void workerLoop();
    Task takeTask();
    void addToCache(const QString& path, QByteArray&& data, qint64 maxBytes);

    QMutex m_mutex;
    QWaitCondition m_cond;
    QQueue<Task> m_queue;
    bool m_stop{false};
    QThread* m_thread{nullptr};
    QHash<QString, QByteArray> m_cache;
    qint64 m_cacheBytes{0};
};
