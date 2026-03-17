#include "nodefooterstatus.h"

NodeFooterStatus::NodeFooterStatus(QObject *parent)
    : QObject(parent)
    , m_mainnetApi(QStringLiteral("https://mainnet.grinffindor.org/v2/foreign"), QString())
    , m_testnetApi(QStringLiteral("https://testnet.grinffindor.org/v2/foreign"), QString())
{
    connectApi(&m_mainnetApi, m_mainnetAvailable, m_mainnetTip, m_mainnetVersion);
    connectApi(&m_testnetApi, m_testnetAvailable, m_testnetTip, m_testnetVersion);

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

void NodeFooterStatus::connectApi(NodeForeignApi *api, bool &availableTarget, QString &tipTarget, QString &versionTarget)
{
    connect(api, &NodeForeignApi::getTipFinished, this, [this, &availableTarget, &tipTarget](const Result<Tip> &result) {
        availableTarget = !result.hasError();
        tipTarget = result.hasError() ? QStringLiteral("offline") : formatTip(result.value());
        emit dataChanged();
    });

    connect(api, &NodeForeignApi::getVersionFinished, this, [this, &availableTarget, &versionTarget](const Result<NodeVersion> &result) {
        if (result.hasError()) {
            qWarning() << "[NodeFooterStatus] getVersionFinished error:" << result.errorMessage();
            availableTarget = false;
        } else {
            qDebug() << "[NodeFooterStatus] getVersionFinished nodeVersion=" << result.value().nodeVersion()
                     << "blockHeaderVersion=" << result.value().blockHeaderVersion();
            availableTarget = true;
        }
        versionTarget = result.hasError() ? QStringLiteral("offline") : formatVersion(result.value());
        emit dataChanged();
    });
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
