#pragma once

#include <QDir>
#include <QImage>
#include <QMutex>
#include <QStandardPaths>
#include <QString>
#include <unordered_map>
#include <list>

// Memory + optional disk cache for thumbnails/full images.
class ThumbCache {
public:
    ThumbCache(int maxItems, qint64 maxBytes, int maxDiskEntries = 5000);

    QString keyFor(const QString& path, int edge) const;

    QImage get(const QString& key);
    void put(const QString& key, const QImage& image);

private:
    struct Entry {
        QString key;
        QImage image;
        qint64 bytes;
    };

    void ensureCapacity(qint64 additionalBytes);
    QImage loadFromDisk(const QString& key);
    void persistToDisk(const QString& key, const QImage& image);
    void cleanupDisk();

    const int m_maxItems;
    const qint64 m_maxBytes;
    const int m_maxDiskEntries;
    qint64 m_currentBytes = 0;
    QString m_diskRoot;

    std::list<Entry> m_items;
    std::unordered_map<QString, std::list<Entry>::iterator> m_lookup;
    QMutex m_mutex;
};
