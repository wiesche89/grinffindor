#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QCoreApplication>

#ifdef Q_OS_WASM
#include <emscripten/emscripten.h>
#endif

#include "nodefooterstatus.h"

#ifdef Q_OS_WASM
static QString detectAssetBaseUrl()
{
    const char *value = emscripten_run_script_string(
        "(function(){ return new URL('./', window.location.href).href; })();");
    return QString::fromUtf8(value ? value : "");
}
#else
static QString detectAssetBaseUrl()
{
    return QStringLiteral("qrc:/res/");
}
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Grinffindor"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("grinffindor.org"));
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");

    QQuickStyle::setStyle("Fusion");

    NodeFooterStatus nodeFooterStatus;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("nodeFooterStatus", &nodeFooterStatus);
    engine.rootContext()->setContextProperty("assetBaseUrl", detectAssetBaseUrl());
    engine.load(QUrl(QStringLiteral("qrc:/qml/qml//Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
