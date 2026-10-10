#include "testnetblockprovider.h"
#include "nodeforeignapi.h"
#include <QRegularExpression>
#include <QJsonDocument>

TestnetBlockProvider::TestnetBlockProvider(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(15000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        complete({}, QStringLiteral("Node request timed out"));
    });
}

void TestnetBlockProvider::cancel(int id)
{
    if (id != m_id) return;
    m_timeout.stop();
    if (m_api) {
        m_api->disconnect(this);
        // Destruction cancels the requests owned by this API instance.
        m_api->deleteLater();
        m_api = nullptr;
    }
}

void TestnetBlockProvider::complete(const QJsonObject &block, const QString &error)
{
    const int id = m_id;
    cancel(id);
    emit finished(id, QString::fromUtf8(QJsonDocument(block).toJson(QJsonDocument::Compact)), error);
}

void TestnetBlockProvider::lookup(const QString &kernel, int id, int height)
{
    cancel(m_id);
    m_id = id;
    static const QRegularExpression excess(QStringLiteral("^[0-9a-f]{66}$"));
    if (!excess.match(kernel).hasMatch() || height < 0) {
        complete({}, QStringLiteral("Invalid kernel"));
        return;
    }
    m_api = new NodeForeignApi(QStringLiteral("https://testnet.grinffindor.org/v2/foreign"), {});
    m_api->setParent(this);
    connect(m_api, &NodeForeignApi::getKernelFinished, this,
            [this, kernel, height](const Result<LocatedTxKernel> &result) {
        if (result.hasError() || result.value().height() != quint64(height)
                || result.value().txKernel().excess() != kernel) {
            complete({}, QStringLiteral("Kernel not found"));
            return;
        }
        m_api->getBlockForDisplayAsync(int(result.value().height()));
    });
    connect(m_api, &NodeForeignApi::blockForDisplayFinished, this,
            [this](const QJsonObject &block, const QString &error) {
        complete(block, error);
    });
    m_timeout.start();
    m_api->getKernelAsync(kernel, height, height);
}
