#include "TiledImageItem.h"

#include <QImageReader>
#include <QPainter>
#include <QtMath>
#include <QVariant>

TiledImageItem::TiledImageItem(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    // cache/loader pointers are injected via context properties (tileCache, tileLoader)
}

void TiledImageItem::setSource(const QString& src)
{
    if (src == m_source)
        return;
    m_source = src;
    // Lazy init cache/loader from context properties if provided
    if (!m_cache) {
        QVariant v = property("tileCache");
        if (v.isValid()) {
            m_cache = reinterpret_cast<TileCache*>(v.value<quintptr>());
        }
    }
    if (!m_loader) {
        QVariant v = property("tileLoader");
        if (v.isValid()) {
            m_loader = reinterpret_cast<TileLoader*>(v.value<quintptr>());
            connect(m_loader, &TileLoader::tileReady, this, &TiledImageItem::onTileReady, Qt::UniqueConnection);
        }
    }
    emit sourceChanged();
    resetTiles();
}

void TiledImageItem::setZoom(qreal z)
{
    z = qMax<qreal>(0.05, qMin<qreal>(20.0, z));
    if (qFuzzyCompare(z, m_zoom))
        return;
    m_zoom = z;
    emit zoomChanged();
    scheduleTiles();
    update();
}

void TiledImageItem::setRotation(qreal r)
{
    if (qFuzzyCompare(r, m_rotation))
        return;
    m_rotation = r;
    emit rotationChanged();
    update();
}

void TiledImageItem::setPan(const QPointF& p)
{
    if (m_panOffset == p)
        return;
    m_panOffset = p;
    emit panChanged();
    scheduleTiles();
    update();
}

void TiledImageItem::resetTiles()
{
    if (m_loader) {
        m_loader->cancelAll();
    }
    m_generation++;
    m_tiles.clear();
    m_fullSize = probeSize();
    emit fullSizeChanged();
    m_level = 0;
    if (m_fullSize.isEmpty()) {
        update();
        return;
    }
    const int w = m_fullSize.width();
    const int h = m_fullSize.height();
    const qreal fx = (w * m_zoom) / width();
    const qreal fy = (h * m_zoom) / height();
    const qreal f = qMax(fx, fy);
    int lvl = 0;
    while ((1 << lvl) < f && lvl < 6) {
        ++lvl;
    }
    m_level = lvl;
    scheduleTiles();
}

QSize TiledImageItem::probeSize() const
{
    if (m_source.isEmpty())
        return QSize();
    QImageReader reader(m_source);
    const QSize s = reader.size();
    return s.isValid() ? s : QSize();
}

QRect TiledImageItem::visibleTileRect() const
{
    if (m_fullSize.isEmpty())
        return QRect();
    const int div = 1 << m_level;
    const int levelW = (m_fullSize.width() + div - 1) / div;
    const int levelH = (m_fullSize.height() + div - 1) / div;
    const qreal scale = qMin(width() / levelW, height() / levelH) * m_zoom;
    const qreal drawW = levelW * scale;
    const qreal drawH = levelH * scale;
    const qreal offsetX = (width() - drawW) / 2 + m_panOffset.x();
    const qreal offsetY = (height() - drawH) / 2 + m_panOffset.y();

    QRectF viewRect(0, 0, width(), height());
    QRect visTiles;
    visTiles.setLeft(qFloor((viewRect.left() - offsetX) / (m_tileSize * scale)));
    visTiles.setTop(qFloor((viewRect.top() - offsetY) / (m_tileSize * scale)));
    visTiles.setRight(qCeil((viewRect.right() - offsetX) / (m_tileSize * scale)));
    visTiles.setBottom(qCeil((viewRect.bottom() - offsetY) / (m_tileSize * scale)));
    return visTiles;
}

void TiledImageItem::scheduleTiles()
{
    if (m_fullSize.isEmpty() || !m_loader || !m_cache)
        return;
    const int div = 1 << m_level;
    const int levelW = (m_fullSize.width() + div - 1) / div;
    const int levelH = (m_fullSize.height() + div - 1) / div;
    const int tilesX = (levelW + m_tileSize - 1) / m_tileSize;
    const int tilesY = (levelH + m_tileSize - 1) / m_tileSize;

    QRect vr = visibleTileRect();
    const int cx = (vr.left() + vr.right()) / 2;
    const int cy = (vr.top() + vr.bottom()) / 2;
    QList<QVariant> reqs;
    for (int y = vr.top(); y <= vr.bottom() && y < tilesY; ++y) {
        for (int x = vr.left(); x <= vr.right() && x < tilesX; ++x) {
            const QString key = m_cache->makeKey(m_source, m_level, x, y);
            if (m_tiles.contains(key))
                continue;
            int priority = qAbs(x - cx) + qAbs(y - cy);
            QVariantList args;
            args << m_source << m_fullSize << m_level << x << y << m_tileSize << priority;
            reqs << args;
        }
    }
    if (!reqs.isEmpty()) {
        m_loader->enqueueVisible(reqs, m_generation);
    }
    update();
}

void TiledImageItem::onTileReady(const QString& key, const QImage& img, int generation)
{
    if (generation != m_generation)
        return;
    if (img.isNull())
        return;
    m_tiles.insert(key, img);
    update();
}

void TiledImageItem::paint(QPainter* p)
{
    p->fillRect(boundingRect(), Qt::black);
    if (m_fullSize.isEmpty())
        return;

    const int div = 1 << m_level;
    const int levelW = (m_fullSize.width() + div - 1) / div;
    const int levelH = (m_fullSize.height() + div - 1) / div;
    const qreal scale = qMin(width() / levelW, height() / levelH) * m_zoom;
    const qreal drawW = levelW * scale;
    const qreal drawH = levelH * scale;
    const qreal offsetX = (width() - drawW) / 2 + m_panOffset.x();
    const qreal offsetY = (height() - drawH) / 2 + m_panOffset.y();

    p->save();
    p->translate(width() / 2.0, height() / 2.0);
    p->rotate(m_rotation);
    p->translate(-width() / 2.0, -height() / 2.0);

    const int tilesX = (levelW + m_tileSize - 1) / m_tileSize;
    const int tilesY = (levelH + m_tileSize - 1) / m_tileSize;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            const QString key = m_cache->makeKey(m_source, m_level, tx, ty);
            auto it = m_tiles.find(key);
            if (it == m_tiles.end())
                continue;
            const QImage& tile = it.value();
            const int x = tx * m_tileSize;
            const int y = ty * m_tileSize;
            const int w = tile.width();
            const int h = tile.height();
            const QRectF target(offsetX + x * scale, offsetY + y * scale, w * scale, h * scale);
            p->drawImage(target, tile);
        }
    }
    p->restore();
}
