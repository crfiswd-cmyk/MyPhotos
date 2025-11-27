#include "ThumbBridge.h"

#include "ThumbProvider.h"
#include "ImageListModel.h"

ThumbBridge::ThumbBridge(ThumbProvider* provider, ImageListModel* model, QObject* parent)
    : QObject(parent)
    , m_provider(provider)
    , m_model(model)
{
}

void ThumbBridge::prefetchAround(int centerIndex, int radius, int targetEdge)
{
    if (!m_provider) {
        return;
    }
    m_provider->setModel(m_model);
    m_provider->prefetchAround(centerIndex, radius, targetEdge);
}
