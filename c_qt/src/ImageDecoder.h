#pragma once

#include <QImage>
#include <QString>

// Helper responsible for reading and scaling images.
// Uses libvips when available (HAVE_VIPS), falls back to Qt's QImageReader otherwise.
class ImageDecoder {
public:
    // Decode to fit inside maxEdge while keeping aspect ratio. If maxEdge <= 0, decode full size.
    static QImage decode(const QString& path, int maxEdge);

private:
    static QImage decodeWithVips(const QString& path, int maxEdge);
    static QImage decodeWithQt(const QString& path, int maxEdge);
};
