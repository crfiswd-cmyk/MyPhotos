#pragma once

#include <QObject>
#include <QThreadPool>
#include <QSize>
#include <QMutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <QVector>

#include "TileCache.h"

class TileLoader : public QObject {
    Q_OBJECT
public:
    TileLoader(TileCache* cache, QThreadPool* pool, QObject* parent = nullptr);
    ~TileLoader();
    void setDiskModeHDD(bool hdd) { m_isHDD = hdd; }
    void cancelAll();
    int generation() const { return m_generation; }
    void enqueueVisible(const QList<QVariant>& requests, int generation);

public slots:
    void requestTile(const QString& path, const QSize& fullSize, int level, int tx, int ty, int tileSize, int generation);

signals:
    void tileReady(const QString& key, const QImage& img, int generation);

private:
    TileCache* m_cache;
    QThreadPool* m_pool;
    bool m_isHDD{false};
    int m_generation{0};
    mutable QMutex m_mutex;
    QWaitCondition m_cond;

    struct Task {
        QString path;
        QSize fullSize;
        int level{0};
        int tx{0};
        int ty{0};
        int tileSize{256};
        int generation{0};
        int priority{0}; // lower is higher priority
        QString key;
    };
    QVector<Task> m_tasks;
    QThread* m_thread{nullptr};
    bool m_stop{false};

    void workerLoop();
    bool takeTask(Task& out);
};
