#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQuickControls2/QQuickStyle>

#include "ImageListModel.h"
#include "ThumbCache.h"
#include "ThumbProvider.h"
#include "ThumbBridge.h"

using namespace Qt::StringLiterals;

int main(int argc, char* argv[])
{
    QGuiApplication::setApplicationName("MyPhotos");
    QGuiApplication::setOrganizationName("Example");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    ImageListModel model;
    ThumbCache cache(/*maxItems*/ 512, /*maxBytes*/ 512 * 1024 * 1024); // up to ~512MB shared cache
    QThreadPool pool;
    pool.setMaxThreadCount(qMax(QThread::idealThreadCount(), 8));

    auto* provider = new ThumbProvider(&cache, &pool); // owned by the engine
    provider->setModel(&model);
    ThumbBridge bridge(provider, &model);

    QQmlApplicationEngine engine;
    engine.addImageProvider("thumbs", provider);
    engine.rootContext()->setContextProperty("imageModel", &model);
    engine.rootContext()->setContextProperty("thumbBridge", &bridge);

    const QUrl url(u"qrc:/qml/Main.qml"_s);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* obj, const QUrl& objUrl) {
        if (!obj && url == objUrl) {
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
