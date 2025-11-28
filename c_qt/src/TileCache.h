#pragma once

#include <QHash>
#include <QImage>
#include <QString>
#include <list>

// Simple in-memory LRU cache for tiles, with optional disk cache pruning.
class TileCache {
public:
    TileCache(int maxItems = 256, qint64 maxBytes = 256 * 1024 * 1024, int maxDiskEntries = 0)
        : m_maxItems(maxItems), m_maxBytes(maxBytes), m_maxDiskEntries(maxDiskEntries) {}

    QString makeKey(const QString& path, int level, int tx, int ty) const;
    QImage get(const QString& key);
    void put(const QString& key, const QImage& img);
    void setDiskRoot(const QString& root);

private:
    struct Entry {
        QString key;
        QImage img;
        qint64 bytes;
    };

    void evictIfNeeded();
    void persistToDisk(const QString& key, const QImage& img);
    QImage loadFromDisk(const QString& key);
    void cleanupDisk();

    int m_maxItems;
    qint64 m_maxBytes;
    int m_maxDiskEntries;
    qint64 m_currentBytes{0};
    std::list<Entry> m_list;
    QHash<QString, std::list<Entry>::iterator> m_map;
    QString m_diskRoot;
};
