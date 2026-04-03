#include "nodefooterstatus.h"

NodeFooterStatus::NodeFooterStatus(QObject *parent)
    : QObject(parent)
    , m_mainnetApi(QStringLiteral("https://mainnet.grinffindor.org/v2/foreign"), QString())
    , m_testnetApi(QStringLiteral("https://testnet.grinffindor.org/v2/foreign"), QString())
{
    connectApi(&m_mainnetApi);
    connectApi(&m_testnetApi);

    m_refreshTimer.setInterval(30000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &NodeFooterStatus::refresh);
    m_refreshTimer.start();

    refresh();
}

bool NodeFooterStatus::mainnetAvailable() const
{
    return m_mainnetAvailable;
}

QString NodeFooterStatus::mainnetTip() const
{
    return m_mainnetTip;
}

QString NodeFooterStatus::mainnetVersion() const
{
    return m_mainnetVersion;
}

bool NodeFooterStatus::testnetAvailable() const
{
    return m_testnetAvailable;
}

QString NodeFooterStatus::testnetTip() const
{
    return m_testnetTip;
}

QString NodeFooterStatus::testnetVersion() const
{
    return m_testnetVersion;
}

void NodeFooterStatus::refresh()
{
    m_mainnetApi.getTipAsync();
    m_mainnetApi.getVersionAsync();
    m_testnetApi.getTipAsync();
    m_testnetApi.getVersionAsync();
}

void NodeFooterStatus::connectApi(NodeForeignApi *api)
{
    connect(api, &NodeForeignApi::getTipFinished, this, &NodeFooterStatus::handleTipFinished);
    connect(api, &NodeForeignApi::getVersionFinished, this, &NodeFooterStatus::handleVersionFinished);
}

void NodeFooterStatus::updateApiAvailability(NodeForeignApi *api, bool available)
{
    if (api == &m_mainnetApi) {
        m_mainnetAvailable = available;
        return;
    }
    if (api == &m_testnetApi) {
        m_testnetAvailable = available;
    }
}

void NodeFooterStatus::updateApiTip(NodeForeignApi *api, const QString &tipText)
{
    if (api == &m_mainnetApi) {
        m_mainnetTip = tipText;
        return;
    }
    if (api == &m_testnetApi) {
        m_testnetTip = tipText;
    }
}

void NodeFooterStatus::updateApiVersion(NodeForeignApi *api, const QString &versionText)
{
    if (api == &m_mainnetApi) {
        m_mainnetVersion = versionText;
        return;
    }
    if (api == &m_testnetApi) {
        m_testnetVersion = versionText;
    }
}

void NodeFooterStatus::handleTipFinished(const Result<Tip> &result)
{
    NodeForeignApi *api = qobject_cast<NodeForeignApi *>(sender());
    if (!api) {
        return;
    }

    updateApiAvailability(api, !result.hasError());
    updateApiTip(api, result.hasError() ? QStringLiteral("offline") : formatTip(result.value()));
    emit dataChanged();
}

void NodeFooterStatus::handleVersionFinished(const Result<NodeVersion> &result)
{
    NodeForeignApi *api = qobject_cast<NodeForeignApi *>(sender());
    if (!api) {
        return;
    }

    updateApiAvailability(api, !result.hasError());
    updateApiVersion(api, result.hasError() ? QStringLiteral("offline") : formatVersion(result.value()));
    emit dataChanged();
}

QString NodeFooterStatus::formatTip(const Tip &tip)
{
    return QString::number(tip.height());
}

QString NodeFooterStatus::formatVersion(const NodeVersion &version)
{
    if (!version.nodeVersion().isEmpty())
        return version.nodeVersion();

    if (version.blockHeaderVersion() > 0 && version.blockHeaderVersion() < 256)
        return QStringLiteral("bhv %1").arg(version.blockHeaderVersion());

    return QStringLiteral("unknown");
}
