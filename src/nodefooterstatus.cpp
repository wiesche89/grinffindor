#include "nodefooterstatus.h"

/**
 * @brief Constructs a status bridge for mainnet and testnet node footer data.
 * @param parent Optional parent object.
 */
NodeFooterStatus::NodeFooterStatus(QObject *parent)
    : QObject(parent)
    , m_mainnetApi(QStringLiteral("https://mainnet.grinffindor.org/v2/foreign"), QString())
    , m_testnetApi(QStringLiteral("https://testnet.grinffindor.org/v2/foreign"), QString())
{
    // -------------------------------------------------------------------------------------------------------
    // Connecting API Signals
    // -------------------------------------------------------------------------------------------------------
    connectApi(&m_mainnetApi);
    connectApi(&m_testnetApi);

    // -------------------------------------------------------------------------------------------------------
    // Configuring Periodic Refresh
    // -------------------------------------------------------------------------------------------------------
    m_refreshTimer.setInterval(30000);
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, &NodeFooterStatus::refresh);
    m_refreshTimer.start();

    // -------------------------------------------------------------------------------------------------------
    // Running Initial Data Fetch
    // -------------------------------------------------------------------------------------------------------
    refresh();
}

/**
 * @brief Returns whether the mainnet endpoint is currently available.
 * @return True if the mainnet endpoint is reachable.
 */
bool NodeFooterStatus::mainnetAvailable() const
{
    return m_mainnetAvailable;
}

/**
 * @brief Returns the mainnet tip text used in the footer.
 * @return Mainnet tip text.
 */
QString NodeFooterStatus::mainnetTip() const
{
    return m_mainnetTip;
}

/**
 * @brief Returns the mainnet version text used in the footer.
 * @return Mainnet version text.
 */
QString NodeFooterStatus::mainnetVersion() const
{
    return m_mainnetVersion;
}

/**
 * @brief Returns whether the testnet endpoint is currently available.
 * @return True if the testnet endpoint is reachable.
 */
bool NodeFooterStatus::testnetAvailable() const
{
    return m_testnetAvailable;
}

/**
 * @brief Returns the testnet tip text used in the footer.
 * @return Testnet tip text.
 */
QString NodeFooterStatus::testnetTip() const
{
    return m_testnetTip;
}

/**
 * @brief Returns the testnet version text used in the footer.
 * @return Testnet version text.
 */
QString NodeFooterStatus::testnetVersion() const
{
    return m_testnetVersion;
}

/**
 * @brief Triggers asynchronous tip and version requests for both networks.
 */
void NodeFooterStatus::refresh()
{
    m_mainnetApi.getTipAsync();
    m_mainnetApi.getVersionAsync();
    m_testnetApi.getTipAsync();
    m_testnetApi.getVersionAsync();
}

/**
 * @brief Connects asynchronous API completion signals to this object.
 * @param api API instance to connect.
 */
void NodeFooterStatus::connectApi(NodeForeignApi *api)
{
    connect(api, &NodeForeignApi::getTipFinished, this, &NodeFooterStatus::handleTipFinished);
    connect(api, &NodeForeignApi::getVersionFinished, this, &NodeFooterStatus::handleVersionFinished);
}

/**
 * @brief Stores endpoint availability for the network represented by the API instance.
 * @param api API instance that reported state.
 * @param available Availability flag.
 */
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

/**
 * @brief Stores formatted tip text for the network represented by the API instance.
 * @param api API instance that reported data.
 * @param tipText Formatted tip value.
 */
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

/**
 * @brief Stores formatted version text for the network represented by the API instance.
 * @param api API instance that reported data.
 * @param versionText Formatted version value.
 */
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

/**
 * @brief Handles completion of a tip request and updates related footer fields.
 * @param result Result carrying tip data or an error.
 */
void NodeFooterStatus::handleTipFinished(const Result<Tip> &result)
{
    // -------------------------------------------------------------------------------------------------------
    // Resolving Calling API Instance
    // -------------------------------------------------------------------------------------------------------
    NodeForeignApi *api = qobject_cast<NodeForeignApi *>(sender());
    if (!api) {
        return;
    }

    // -------------------------------------------------------------------------------------------------------
    // Updating Cached Tip State
    // -------------------------------------------------------------------------------------------------------
    updateApiAvailability(api, !result.hasError());
    updateApiTip(api, result.hasError() ? QStringLiteral("offline") : formatTip(result.value()));
    emit dataChanged();
}

/**
 * @brief Handles completion of a version request and updates related footer fields.
 * @param result Result carrying version data or an error.
 */
void NodeFooterStatus::handleVersionFinished(const Result<NodeVersion> &result)
{
    // -------------------------------------------------------------------------------------------------------
    // Resolving Calling API Instance
    // -------------------------------------------------------------------------------------------------------
    NodeForeignApi *api = qobject_cast<NodeForeignApi *>(sender());
    if (!api) {
        return;
    }

    // -------------------------------------------------------------------------------------------------------
    // Updating Cached Version State
    // -------------------------------------------------------------------------------------------------------
    updateApiAvailability(api, !result.hasError());
    updateApiVersion(api, result.hasError() ? QStringLiteral("offline") : formatVersion(result.value()));
    emit dataChanged();
}

/**
 * @brief Converts tip data into a compact footer string.
 * @param tip Tip model to format.
 * @return Tip height as text.
 */
QString NodeFooterStatus::formatTip(const Tip &tip)
{
    return QString::number(tip.height());
}

/**
 * @brief Converts node version data into a compact footer string.
 * @param version Node version model to format.
 * @return Preferred version text.
 */
QString NodeFooterStatus::formatVersion(const NodeVersion &version)
{
    if (!version.nodeVersion().isEmpty())
        return version.nodeVersion();

    if (version.blockHeaderVersion() > 0 && version.blockHeaderVersion() < 256)
        return QStringLiteral("bhv %1").arg(version.blockHeaderVersion());

    return QStringLiteral("unknown");
}
