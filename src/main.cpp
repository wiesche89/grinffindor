#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QCoreApplication>

#include "nodefooterstatus.h"

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
    engine.load(QUrl(QStringLiteral("qrc:/qml/qml//Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
