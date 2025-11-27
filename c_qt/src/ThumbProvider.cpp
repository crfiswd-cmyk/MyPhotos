#include "ThumbProvider.h"

#include <QImage>
#include <QSize>
#include <QStringList>

ThumbResponse::ThumbResponse(const QString& path, int targetEdge, ThumbCache* cache, QThreadPool* pool)
{
    auto future = QtConcurrent::run(pool, [path, targetEdge, cache]() -> QImage {
        const QString key = cache->keyFor(path, targetEdge);
        auto cached = cache->get(key);
        if (!cached.isNull()) {
            return cached;
        }
        QImage img = ImageDecoder::decode(path, targetEdge);
        cache->put(key, img);
        return img;
    });

    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher]() {
        m_image = watcher->result();
        if (m_image.isNull()) {
            m_error = QStringLiteral("decode failed");
        }
        emit finished();
    });
    watcher->setFuture(future);
}

QQuickTextureFactory* ThumbResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

ThumbProvider::ThumbProvider(ThumbCache* cache, QThreadPool* pool)
    : m_cache(cache)
    , m_pool(pool)
{
}

QQuickImageResponse* ThumbProvider::requestImageResponse(const QString& id, const QSize& requestedSize)
{
    Q_UNUSED(requestedSize);
    // id format: "<edge>/<absolute path>" or "full/<absolute path>"
    const int slash = id.indexOf('/');
    if (slash <= 0 || slash + 1 >= id.size()) {
        return new ThumbResponse(QString(), 0, m_cache, m_pool);
    }
    const QString head = id.left(slash);
    const QString path = id.mid(slash + 1);
    const int edge = head == QStringLiteral("full") ? 0 : head.toInt();
    return new ThumbResponse(path, edge, m_cache, m_pool);
}

void ThumbProvider::prefetchAround(int centerIndex, int radius, int targetEdge)
{
    if (!m_model) {
        return;
    }
    const int start = qMax(0, centerIndex - radius);
    const int end = centerIndex + radius;
    for (int i = start; i <= end; ++i) {
        const QString path = m_model->pathAt(i);
        if (path.isEmpty()) {
            continue;
        }
        enqueueDecode(path, targetEdge);
    }
}

void ThumbProvider::enqueueDecode(const QString& path, int edge)
{
    const QString key = m_cache->keyFor(path, edge);
    if (!m_cache->get(key).isNull()) {
        return;
    }
    [[maybe_unused]] auto future = QtConcurrent::run(m_pool, [path, edge, this]() {
        const QString innerKey = m_cache->keyFor(path, edge);
        auto cached = m_cache->get(innerKey);
        if (!cached.isNull()) {
            return;
        }
        QImage img = ImageDecoder::decode(path, edge);
        m_cache->put(innerKey, img);
    });
}
