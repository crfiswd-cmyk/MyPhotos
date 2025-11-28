#include "TileCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStringList>

QString TileCache::makeKey(const QString& path, int level, int tx, int ty) const
{
    return QStringLiteral("%1|%2|%3|%4").arg(path).arg(level).arg(tx).arg(ty);
}

QImage TileCache::get(const QString& key)
{
    auto it = m_map.find(key);
    if (it == m_map.end()) {
        if (!m_diskRoot.isEmpty()) {
            QImage disk = loadFromDisk(key);
            if (!disk.isNull()) {
                put(key, disk);
                return disk;
            }
        }
        return QImage();
    }
    // move to front
    m_list.splice(m_list.begin(), m_list, it.value());
    return it.value()->img;
}

void TileCache::put(const QString& key, const QImage& img)
{
    if (img.isNull()) {
        return;
    }
    auto it = m_map.find(key);
    if (it != m_map.end()) {
        m_currentBytes -= it.value()->bytes;
        m_list.erase(it.value());
        m_map.erase(it);
    }
    Entry e{key, img, img.sizeInBytes()};
    m_list.push_front(e);
    m_map.insert(key, m_list.begin());
    m_currentBytes += e.bytes;
    evictIfNeeded();
    if (!m_diskRoot.isEmpty()) {
        persistToDisk(key, img);
    }
}

void TileCache::evictIfNeeded()
{
    while ((int)m_list.size() > m_maxItems || m_currentBytes > m_maxBytes) {
        if (m_list.empty()) break;
        auto last = std::prev(m_list.end());
        m_currentBytes -= last->bytes;
        m_map.remove(last->key);
        m_list.erase(last);
    }
}

void TileCache::setDiskRoot(const QString& root)
{
    m_diskRoot = root;
    if (!m_diskRoot.isEmpty()) {
        QDir().mkpath(m_diskRoot);
    }
}

void TileCache::persistToDisk(const QString& key, const QImage& img)
{
    if (m_diskRoot.isEmpty() || m_maxDiskEntries <= 0)
        return;
    const QByteArray hashed = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString path = QDir(m_diskRoot).filePath(QString::fromLatin1(hashed) + ".png");
    img.save(path, "PNG");
    cleanupDisk();
}

QImage TileCache::loadFromDisk(const QString& key)
{
    if (m_diskRoot.isEmpty())
        return QImage();
    const QByteArray hashed = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString path = QDir(m_diskRoot).filePath(QString::fromLatin1(hashed) + ".png");
    QImage img;
    img.load(path);
    return img;
}

void TileCache::cleanupDisk()
{
    if (m_diskRoot.isEmpty() || m_maxDiskEntries <= 0)
        return;
    QDir dir(m_diskRoot);
    const auto files = dir.entryInfoList(QStringList() << "*.png", QDir::Files, QDir::Time | QDir::Reversed);
    if (files.size() <= m_maxDiskEntries)
        return;
    const int toRemove = files.size() - m_maxDiskEntries;
    for (int i = 0; i < toRemove; ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
