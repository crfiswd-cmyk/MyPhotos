#include "SequentialFileReader.h"

#include <QFile>
#include <QThread>
#include <QtConcurrent>

SequentialFileReader::SequentialFileReader()
{
    m_thread = QThread::create([this]() { workerLoop(); });
    m_thread->setObjectName("SequentialFileReader");
    m_thread->start();
}

SequentialFileReader::~SequentialFileReader()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stop = true;
        m_cond.wakeAll();
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        delete m_thread;
        m_thread = nullptr;
    }
}

SequentialFileReader& SequentialFileReader::instance()
{
    static SequentialFileReader inst;
    return inst;
}

QByteArray SequentialFileReader::readFile(const QString& path)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(path)) {
            QByteArray data = m_cache.take(path);
            m_cacheBytes -= data.size();
            return data;
        }
    }
    QByteArray buffer;
    QWaitCondition done;
    {
        QMutexLocker locker(&m_mutex);
        m_queue.enqueue({path, &buffer, &done, false});
        m_cond.wakeOne();
    }
    QMutexLocker locker(&m_mutex);
    done.wait(&m_mutex);
    return buffer;
}

SequentialFileReader::Task SequentialFileReader::takeTask()
{
    QMutexLocker locker(&m_mutex);
    while (m_queue.isEmpty() && !m_stop) {
        m_cond.wait(&m_mutex);
    }
    if (m_stop) {
        return Task();
    }
    return m_queue.dequeue();
}

void SequentialFileReader::workerLoop()
{
    for (;;) {
        Task task = takeTask();
        if (m_stop) {
            break;
        }
        if (!task.out) {
            continue;
        }
        task.out->clear();

        QFile f(task.path);
        if (f.open(QIODevice::ReadOnly)) {
            task.out->reserve(static_cast<int>(f.size()));
            while (!f.atEnd()) {
                QByteArray chunk = f.read(kChunkSize);
                if (chunk.isEmpty()) {
                    break;
                }
                task.out->append(chunk);
            }
        }

        if (task.storeInCache) {
            addToCache(task.path, std::move(*task.out), kChunkSize * 8); // limit 32MB by default
            task.out->clear();
        }

        QMutexLocker locker(&m_mutex);
        if (task.done) {
            task.done->wakeAll();
        }
    }
}

void SequentialFileReader::addToCache(const QString& path, QByteArray&& data, qint64 maxBytes)
{
    if (data.isEmpty()) return;
    QMutexLocker locker(&m_mutex);
    m_cache.insert(path, data);
    m_cacheBytes += data.size();
    while (m_cacheBytes > maxBytes && !m_cache.isEmpty()) {
        auto it = m_cache.begin();
        m_cacheBytes -= it.value().size();
        it = m_cache.erase(it);
    }
}

void SequentialFileReader::prefetchFiles(const QStringList& paths, qint64 maxBytes)
{
    QMutexLocker locker(&m_mutex);
    for (const auto& p : paths) {
        if (m_cache.contains(p)) continue;
        QByteArray dummy;
        QWaitCondition* done = nullptr;
        m_queue.enqueue({p, &dummy, done, true});
    }
    m_cond.wakeAll();
}
