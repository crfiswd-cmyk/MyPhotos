#pragma once

#include <QFutureWatcher>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QThreadPool>
#include <QtConcurrent>

#include "ImageDecoder.h"
#include "ThumbCache.h"
#include "ImageListModel.h"

class ThumbResponse : public QQuickImageResponse {
    Q_OBJECT
public:
    ThumbResponse(const QString& path, int targetEdge, ThumbCache* cache, QThreadPool* pool);
    ~ThumbResponse() override = default;

    QQuickTextureFactory* textureFactory() const override;
    QString errorString() const override { return m_error; }

private:
    QImage m_image;
    QString m_error;
};

class ThumbProvider : public QQuickAsyncImageProvider {
public:
    explicit ThumbProvider(ThumbCache* cache, QThreadPool* pool);

    void setModel(ImageListModel* model) { m_model = model; }

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    void prefetchAround(int centerIndex, int radius, int targetEdge);

private:
    void enqueueDecode(const QString& path, int edge);

    ThumbCache* m_cache;
    QThreadPool* m_pool;
    ImageListModel* m_model = nullptr;
};
