#pragma once

#include <QObject>

class ThumbProvider;
class ImageListModel;

// Lightweight QObject wrapper to expose prefetch to QML without transferring ownership.
class ThumbBridge : public QObject {
    Q_OBJECT
public:
    ThumbBridge(ThumbProvider* provider, ImageListModel* model, QObject* parent = nullptr);

    Q_INVOKABLE void prefetchAround(int centerIndex, int radius, int targetEdge);

private:
    ThumbProvider* m_provider;
    ImageListModel* m_model;
};
