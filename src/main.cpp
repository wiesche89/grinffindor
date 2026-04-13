#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QUrl>
#include <QCoreApplication>

#ifdef Q_OS_WASM
#include <emscripten/emscripten.h>
#endif

#include "platformbridge.h"
#include "nodefooterstatus.h"
#include "wallet/grinwalletcontroller.h"
#include "wallet/walletsecurerandom.h"

#ifdef Q_OS_WASM
/**
 * @brief Resolves the base URL used to load static assets in a WebAssembly build.
 * @return The absolute base URL for browser-hosted assets.
 */
static QString detectAssetBaseUrl()
{
    const char *value = emscripten_run_script_string(
        "(function(){ return new URL('./', window.location.href).href; })();");
    return QString::fromUtf8(value ? value : "");
}
#else
/**
 * @brief Resolves the base URL used to load static assets in a native build.
 * @return The Qt resource base URL.
 */
static QString detectAssetBaseUrl()
{
    return QStringLiteral("qrc:/res/");
}
#endif

/**
 * @brief Application entry point.
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument array.
 * @return Application exit code.
 */
int main(int argc, char *argv[])
{
    // -------------------------------------------------------------------------------------------------------
    // Setting Application Metadata And Runtime Behavior
    // -------------------------------------------------------------------------------------------------------
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Grinffindor"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("grinffindor.org"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    const QIcon appIcon(QStringLiteral(":/res/media/icons/IconGrinffindor.ico"));
    app.setWindowIcon(appIcon);
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");
    if (!WalletSecureRandom::selfTest()) {
        return -1;
    }

    // -------------------------------------------------------------------------------------------------------
    // Initializing UI Styling
    // -------------------------------------------------------------------------------------------------------
    QQuickStyle::setStyle("Fusion");

    // -------------------------------------------------------------------------------------------------------
    // Wiring Backend Services To The QML Engine
    // -------------------------------------------------------------------------------------------------------
    NodeFooterStatus nodeFooterStatus;
    PlatformBridge platformBridge;
    GrinWalletController grinWalletController;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("nodeFooterStatus", &nodeFooterStatus);
    engine.rootContext()->setContextProperty("PlatformBridge", &platformBridge);
    engine.rootContext()->setContextProperty("grinWalletController", &grinWalletController);
    engine.rootContext()->setContextProperty("assetBaseUrl", detectAssetBaseUrl());
    engine.load(QUrl(QStringLiteral("qrc:/qml/qml//Main.qml")));

    // -------------------------------------------------------------------------------------------------------
    // Validating QML Startup
    // -------------------------------------------------------------------------------------------------------
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    for (QObject *rootObject : engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(rootObject)) {
            window->setIcon(appIcon);
        }
    }

    // -------------------------------------------------------------------------------------------------------
    // Starting The Main Event Loop
    // -------------------------------------------------------------------------------------------------------
    return app.exec();
}
