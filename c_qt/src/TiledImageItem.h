#pragma once

#include <QHash>
#include <QQuickPaintedItem>

#include "TileCache.h"
#include "TileLoader.h"

class TiledImageItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(QSize fullSize READ fullSize NOTIFY fullSizeChanged)
    Q_PROPERTY(QPointF pan READ pan WRITE setPan NOTIFY panChanged)
public:
    TiledImageItem(QQuickItem* parent = nullptr);

    QString source() const { return m_source; }
    void setSource(const QString& src);
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    qreal rotation() const { return m_rotation; }
    void setRotation(qreal r);
    QSize fullSize() const { return m_fullSize; }
    QPointF pan() const { return m_panOffset; }
    void setPan(const QPointF& p);

    void paint(QPainter* painter) override;

signals:
    void sourceChanged();
    void zoomChanged();
    void rotationChanged();
    void fullSizeChanged();
    void panChanged();

private slots:
    void onTileReady(const QString& key, const QImage& img, int generation);

private:
    void resetTiles();
    void scheduleTiles();
    QSize probeSize() const;
    QRect visibleTileRect() const;
    void panBy(const QPointF& delta);

    QString m_source;
    TileCache* m_cache{nullptr};
    TileLoader* m_loader{nullptr};
    QHash<QString, QImage> m_tiles;
    int m_tileSize = 256;
    int m_level = 0;
    QSize m_fullSize;
    qreal m_zoom = 1.0;
    qreal m_rotation = 0.0;
    bool m_isPanning = false;
    QPointF m_panOffset {0, 0};
    int m_generation = 0;
};
