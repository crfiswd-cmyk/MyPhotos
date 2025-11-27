#include "ImageDecoder.h"

#include <QImageReader>
#include <QSize>
#include <QtGlobal>

#ifdef HAVE_VIPS
#include <mutex>
#include <vips/vips8>
#endif

QImage ImageDecoder::decode(const QString& path, int maxEdge)
{
    if (path.isEmpty()) {
        return QImage();
    }

#ifdef HAVE_VIPS
    auto img = decodeWithVips(path, maxEdge);
    if (!img.isNull()) {
        return img;
    }
#endif
    return decodeWithQt(path, maxEdge);
}

#ifdef HAVE_VIPS
QImage ImageDecoder::decodeWithVips(const QString& path, int maxEdge)
{
    static std::once_flag vipsInitFlag;
    static bool vipsReady = false;
    std::call_once(vipsInitFlag, []() {
        if (VIPS_INIT("myphotos") == 0) {
            vipsReady = true;
            atexit(vips_shutdown);
        }
    });
    if (!vipsReady) {
        return QImage();
    }

    using namespace vips;

    try {
        VImage img = VImage::new_from_file(path.toStdString().c_str(), VImage::option()->set("access", "sequential"));

        int width = img.width();
        int height = img.height();
        if (maxEdge > 0) {
            const double scale = static_cast<double>(qMax(width, height)) / static_cast<double>(maxEdge);
            if (scale > 1.0) {
                const double shrink = std::max(1.0, std::floor(scale));
                img = img.shrink(shrink, shrink);
                const double residual = scale / shrink;
                if (residual > 1.05) {
                    img = img.resize(1.0 / residual, VImage::option()->set("kernel", "cubic"));
                }
            }
        }

        if (img.bands() == 3) {
            img = img.colourspace(VIPS_INTERPRETATION_sRGB).bandjoin(255);
        } else if (img.bands() == 4) {
            img = img.colourspace(VIPS_INTERPRETATION_sRGB);
        } else if (img.bands() == 1) {
            img = img.colourspace(VIPS_INTERPRETATION_B_W).copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB)).bandjoin(255);
        }

        const int outW = img.width();
        const int outH = img.height();
        size_t length = 0;
        void* mem = img.write_to_memory(&length);
        if (!mem || length < static_cast<size_t>(outW * outH * 4)) {
            if (mem) {
                g_free(mem);
            }
            return QImage();
        }

        QImage result(
            static_cast<uchar*>(mem),
            outW,
            outH,
            outW * 4,
            QImage::Format_RGBA8888,
            [](void* data) { g_free(data); },
            mem);
        if (result.isNull()) {
            g_free(mem);
        }
        return result;
    } catch (const std::exception&) {
        return QImage();
    }
}
#endif

QImage ImageDecoder::decodeWithQt(const QString& path, int maxEdge)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (maxEdge > 0) {
        const QSize source = reader.size();
        if (source.isValid()) {
            const QSize target = source.scaled(QSize(maxEdge, maxEdge), Qt::KeepAspectRatio);
            reader.setScaledSize(target);
        }
    }
    QImage img = reader.read();
    if (img.isNull()) {
        return img;
    }
    if (maxEdge > 0 && (img.width() > maxEdge || img.height() > maxEdge)) {
        img = img.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return img;
}
