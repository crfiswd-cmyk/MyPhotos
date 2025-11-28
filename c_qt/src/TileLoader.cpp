#include "TileLoader.h"

#include <QImageReader>
#include <cmath>
#include <algorithm>

#include "ImageDecoder.h"

TileLoader::TileLoader(TileCache* cache, QThreadPool* pool, QObject* parent)
    : QObject(parent)
    , m_cache(cache)
    , m_pool(pool)
{
    m_thread = QThread::create([this]() { workerLoop(); });
    m_thread->setObjectName("TileLoaderWorker");
    m_thread->start();
}

TileLoader::~TileLoader()
{
    if (m_thread) {
        {
            QMutexLocker locker(&m_mutex);
            m_stop = true;
            m_cond.wakeAll();
        }
        m_thread->quit();
        m_thread->wait(2000);
    }
}

void TileLoader::requestTile(const QString& path, const QSize& fullSize, int level, int tx, int ty, int tileSize, int generation)
{
    QMutexLocker locker(&m_mutex);
    if (generation != m_generation) return;
    Task t;
    t.path = path;
    t.fullSize = fullSize;
    t.level = level;
    t.tx = tx;
    t.ty = ty;
    t.tileSize = tileSize;
    t.generation = generation;
    t.priority = 0;
    t.key = m_cache->makeKey(path, level, tx, ty);
    m_tasks.append(t);
    m_cond.wakeOne();
}

void TileLoader::cancelAll()
{
    QMutexLocker locker(&m_mutex);
    ++m_generation;
    m_tasks.clear();
    m_cond.wakeAll();
}

void TileLoader::enqueueVisible(const QList<QVariant>& requests, int generation)
{
    QMutexLocker locker(&m_mutex);
    if (generation != m_generation) return;
    for (const auto& req : requests) {
        const auto args = req.toList();
        if (args.size() != 7) continue;
        Task t;
        t.path = args[0].toString();
        t.fullSize = args[1].toSize();
        t.level = args[2].toInt();
        t.tx = args[3].toInt();
        t.ty = args[4].toInt();
        t.tileSize = args[5].toInt();
        t.priority = args[6].toInt();
        t.generation = generation;
        t.key = m_cache->makeKey(t.path, t.level, t.tx, t.ty);
        if (!m_cache->get(t.key).isNull()) {
            emit tileReady(t.key, m_cache->get(t.key), generation);
            continue;
        }
        auto it = std::find_if(m_tasks.begin(), m_tasks.end(), [&](const Task& existing) {
            return existing.key == t.key && existing.generation == t.generation;
        });
        if (it == m_tasks.end()) {
            m_tasks.append(t);
        }
    }
    std::sort(m_tasks.begin(), m_tasks.end(), [](const Task& a, const Task& b) {
        return a.priority < b.priority;
    });
    m_cond.wakeOne();
}

bool TileLoader::takeTask(Task& out)
{
    QMutexLocker locker(&m_mutex);
    while (m_tasks.isEmpty() && !m_stop) {
        m_cond.wait(&m_mutex);
    }
    if (m_stop || m_tasks.isEmpty())
        return false;
    out = m_tasks.takeFirst();
    return true;
}

void TileLoader::workerLoop()
{
    for (;;) {
        Task task;
        if (!takeTask(task)) break;
        {
            QMutexLocker locker(&m_mutex);
            if (task.generation != m_generation)
                continue;
        }
        const QString key = task.key;
        if (!m_cache->get(key).isNull()) {
            emit tileReady(key, m_cache->get(key), task.generation);
            continue;
        }
        const int divisor = 1 << task.level;
        const int targetW = std::max(1, (task.fullSize.width() + divisor - 1) / divisor);
        const int targetH = std::max(1, (task.fullSize.height() + divisor - 1) / divisor);

        QImage levelImg;
        if (m_isHDD) {
            levelImg = ImageDecoder::decode(task.path, std::max(targetW, targetH));
        } else {
            QImageReader reader(task.path);
            reader.setAutoTransform(true);
            reader.setScaledSize(QSize(targetW, targetH));
            levelImg = reader.read();
            if (levelImg.isNull()) {
                levelImg = ImageDecoder::decode(task.path, std::max(targetW, targetH));
            }
        }
        if (levelImg.isNull())
            continue;
        const int x = task.tx * task.tileSize;
        const int y = task.ty * task.tileSize;
        const int w = std::min(task.tileSize, levelImg.width() - x);
        const int h = std::min(task.tileSize, levelImg.height() - y);
        if (w <= 0 || h <= 0)
            continue;
        QImage tile = levelImg.copy(x, y, w, h);
        if (tile.isNull())
            continue;
        m_cache->put(key, tile);
        emit tileReady(key, tile, task.generation);
    }
}
