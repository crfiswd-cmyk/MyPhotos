#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>
#include <QStorageInfo>
#include <QFile>
#include <QByteArray>
#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#endif
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif

#include "ImageListModel.h"
#include "ThumbCache.h"
#include "ThumbProvider.h"
#include "ThumbBridge.h"
#include "ImageDecoder.h"
#include "TiledImageItem.h"
#include "TileCache.h"
#include "TileLoader.h"
#include "SequentialFileReader.h"

using namespace Qt::StringLiterals;

struct PerfConfig {
    enum class DiskMode { Auto, SSD, HDD };
    DiskMode mode = DiskMode::Auto;
    int thumbPrefetchRadius = 3;
    int fullPrefetchRadius = 2;
    int fullDecodeThreads = 4;
    bool useMmap = true;
};

static PerfConfig::DiskMode detectDiskModeAuto()
{
#if defined(Q_OS_WIN)
    // Try seek penalty property (SSD usually reports no penalty)
    BOOL seekPenalty = TRUE;
    HANDLE hDevice = CreateFileW(L"\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDevice != INVALID_HANDLE_VALUE) {
        DEVICE_SEEK_PENALTY_DESCRIPTOR seekDesc = {0};
        STORAGE_PROPERTY_QUERY query = {StorageDeviceSeekPenaltyProperty, PropertyStandardQuery};
        DWORD bytes = 0;
        if (DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &seekDesc, sizeof(seekDesc), &bytes, NULL)) {
            seekPenalty = seekDesc.IncursSeekPenalty;
        }
        CloseHandle(hDevice);
    }
    return seekPenalty ? PerfConfig::DiskMode::HDD : PerfConfig::DiskMode::SSD;
#elif defined(Q_OS_LINUX)
    // Check /sys/block/<dev>/queue/rotational: 0=SSD, 1=HDD
    const auto root = QStorageInfo::root();
    QString devPath = QString::fromUtf8(root.device());
    // devPath like /dev/sda1 -> take "sda"; /dev/nvme0n1p2 -> "nvme0n1"
    QFileInfo fi(devPath);
    const QString base = fi.fileName();
    QString block;
    if (base.startsWith("nvme")) {
        // strip partition suffix
        int pIndex = base.indexOf('p');
        block = pIndex > 0 ? base.left(pIndex) : base;
    } else {
        // remove trailing digits for partition
        int idx = base.size() - 1;
        while (idx >= 0 && base[idx].isDigit()) idx--;
        block = base.left(idx + 1);
    }
    QFile f(QStringLiteral("/sys/block/%1/queue/rotational").arg(block));
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray data = f.readAll().trimmed();
        if (data == "0") {
            return PerfConfig::DiskMode::SSD;
        }
        if (data == "1") {
            return PerfConfig::DiskMode::HDD;
        }
    }
    return PerfConfig::DiskMode::SSD; // default
#elif defined(Q_OS_MACOS)
    // Heuristic: if device name contains "ssd" or "nvme" treat as SSD; else default SSD.
    const auto root = QStorageInfo::root();
    const QString dev = QString::fromUtf8(root.device()).toLower();
    if (dev.contains("ssd") || dev.contains("nvme")) {
        return PerfConfig::DiskMode::SSD;
    }
    // Try IOKit "MediumType"
    CFMutableDictionaryRef matching = IOServiceMatching("IOMedia");
    if (matching) {
        CFDictionarySetValue(matching, CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
        io_iterator_t iter;
        if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) == KERN_SUCCESS) {
            io_registry_entry_t service;
            while ((service = IOIteratorNext(iter))) {
                CFStringRef medium = (CFStringRef)IORegistryEntryCreateCFProperty(service, CFSTR("Medium Type"), kCFAllocatorDefault, 0);
                if (medium) {
                    char buf[256];
                    if (CFStringGetCString(medium, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                        QString type = QString::fromUtf8(buf).toLower();
                        CFRelease(medium);
                        IOObjectRelease(service);
                        IOObjectRelease(iter);
                        if (type.contains("solid state"))
                            return PerfConfig::DiskMode::SSD;
                        if (type.contains("rotational"))
                            return PerfConfig::DiskMode::HDD;
                    } else {
                        CFRelease(medium);
                    }
                }
                IOObjectRelease(service);
            }
            IOObjectRelease(iter);
        }
    }
    return PerfConfig::DiskMode::SSD;
#else
    return PerfConfig::DiskMode::SSD;
#endif
}

static PerfConfig detectPerfConfig()
{
    PerfConfig cfg;
    const QByteArray env = qgetenv("MY_PHOTOS_DISK_MODE").toLower();
    if (env == "hdd") {
        cfg.mode = PerfConfig::DiskMode::HDD;
    } else if (env == "ssd" || env == "nvme") {
        cfg.mode = PerfConfig::DiskMode::SSD;
    } else {
        cfg.mode = detectDiskModeAuto();
    }

    const int ideal = QThread::idealThreadCount();
    if (cfg.mode == PerfConfig::DiskMode::HDD) {
        cfg.useMmap = false;
        ImageDecoder::setUseSequentialIO(true);
        cfg.fullPrefetchRadius = 5;
        cfg.thumbPrefetchRadius = 4;
        cfg.fullDecodeThreads = qMax(2, qMin(ideal > 0 ? ideal : 2, 3));
    } else { // SSD-friendly defaults
        cfg.useMmap = true;
        ImageDecoder::setUseSequentialIO(false);
        cfg.fullPrefetchRadius = 2;
        cfg.thumbPrefetchRadius = 3;
        cfg.fullDecodeThreads = qMax(2, qMin(ideal > 0 ? ideal : 4, 4));
    }
    return cfg;
}

int main(int argc, char* argv[])
{
    QGuiApplication::setApplicationName("MyPhotos");
    QGuiApplication::setOrganizationName("Example");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    const auto perf = detectPerfConfig();
    ImageDecoder::setUseMmap(perf.useMmap);

    ImageListModel model;
    ThumbCache cache(/*maxItems*/ 512, /*maxBytes*/ 512 * 1024 * 1024, /*maxDiskEntries*/ 5000); // up to ~512MB shared cache
    QThreadPool pool;
    pool.setMaxThreadCount(perf.fullDecodeThreads);

    auto* provider = new ThumbProvider(&cache, &pool); // owned by the engine
    provider->setModel(&model);
    ThumbBridge bridge(provider, &model);
    int tileDiskMax = 3000;
    bool ok = false;
    const QByteArray tileMaxEnv = qgetenv("MY_PHOTOS_TILE_CACHE_ENTRIES");
    if (!tileMaxEnv.isEmpty()) {
        int val = QString::fromUtf8(tileMaxEnv).toInt(&ok);
        if (ok && val > 0) tileDiskMax = val;
    }
    TileCache tileCache(256, 256 * 1024 * 1024, tileDiskMax);
    QString tileRoot = QString::fromUtf8(qgetenv("MY_PHOTOS_TILE_CACHE_DIR"));
    if (tileRoot.isEmpty()) {
        const auto cacheBase = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (!cacheBase.isEmpty()) {
            tileRoot = cacheBase + "/tiles";
        }
    }
    if (!tileRoot.isEmpty()) {
        QDir().mkpath(tileRoot);
        tileCache.setDiskRoot(tileRoot);
    }
    TileLoader tileLoader(&tileCache, &pool);
    tileLoader.setDiskModeHDD(perf.mode == PerfConfig::DiskMode::HDD);

    QQmlApplicationEngine engine;
    engine.addImageProvider("thumbs", provider);
    engine.rootContext()->setContextProperty("imageModel", &model);
    engine.rootContext()->setContextProperty("thumbBridge", &bridge);
    engine.rootContext()->setContextProperty("thumbPrefetchRadiusValue", perf.thumbPrefetchRadius);
    engine.rootContext()->setContextProperty("fullPrefetchRadiusValue", perf.fullPrefetchRadius);
    engine.rootContext()->setContextProperty("tileCachePtr", QVariant::fromValue(reinterpret_cast<quintptr>(&tileCache)));
    engine.rootContext()->setContextProperty("tileLoaderPtr", QVariant::fromValue(reinterpret_cast<quintptr>(&tileLoader)));
    qmlRegisterType<TiledImageItem>("MyPhotos", 1, 0, "TiledImage");

    const QUrl url(u"qrc:/qml/Main.qml"_s);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
