#include "ThumbCache.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

ThumbCache::ThumbCache(int maxItems, qint64 maxBytes)
    : m_maxItems(maxItems)
    , m_maxBytes(maxBytes)
{
    auto base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }
    m_diskRoot = QDir(base).filePath("myphotos/thumbs");
    QDir().mkpath(m_diskRoot);
}

QString ThumbCache::keyFor(const QString& path, int edge) const
{
    return QString::number(edge) + "|" + path;
}

QImage ThumbCache::get(const QString& key)
{
    QMutexLocker locker(&m_mutex);
    auto it = m_lookup.find(key);
    if (it != m_lookup.end()) {
        m_items.splice(m_items.begin(), m_items, it->second);
        return it->second->image;
    }
    locker.unlock();
    return loadFromDisk(key);
}

void ThumbCache::put(const QString& key, const QImage& image)
{
    if (image.isNull()) {
        return;
    }

    const qint64 bytes = image.sizeInBytes();
    QMutexLocker locker(&m_mutex);

    auto it = m_lookup.find(key);
    if (it != m_lookup.end()) {
        m_currentBytes -= it->second->bytes;
        m_items.erase(it->second);
        m_lookup.erase(it);
    }

    m_items.push_front({key, image, bytes});
    m_lookup[key] = m_items.begin();
    m_currentBytes += bytes;

    ensureCapacity(0);
    locker.unlock();
    persistToDisk(key, image);
}

void ThumbCache::ensureCapacity(qint64 additionalBytes)
{
    while ((m_items.size() > static_cast<size_t>(m_maxItems)) || (m_currentBytes + additionalBytes > m_maxBytes)) {
        if (m_items.empty()) {
            break;
        }
        auto last = --m_items.end();
        m_currentBytes -= last->bytes;
        m_lookup.erase(last->key);
        m_items.erase(last);
    }
}

QImage ThumbCache::loadFromDisk(const QString& key)
{
    const QByteArray hashed = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString path = QDir(m_diskRoot).filePath(QString::fromLatin1(hashed) + ".png");
    QFile file(path);
    if (!file.exists()) {
        return QImage();
    }
    QImage img;
    img.load(path);
    if (!img.isNull()) {
        put(key, img);
    }
    return img;
}

void ThumbCache::persistToDisk(const QString& key, const QImage& image)
{
    const QByteArray hashed = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString path = QDir(m_diskRoot).filePath(QString::fromLatin1(hashed) + ".png");

    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());

    // Fire-and-forget save.
    image.save(path, "PNG");
}
