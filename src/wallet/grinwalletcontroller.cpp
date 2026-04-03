#include "grinwalletcontroller.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QStringList>
#include <QUuid>
#include <algorithm>

#include "../3rdparty/monocypher/monocypher.h"

#include "slatev4.h"
#include "walletoutput.h"
#include "walletscanner.h"
#include "walletselection.h"
#include "walletkeychain.h"
#include "wallettxbuilder.h"
#include "grinwalletstorage.h"
#include "grinwalletplatformhelpers.h"
#include "grinwalletcontrollerhelpers.h"
#include "grinwallethistoryhelpers.h"
#include "grinwalletseedcrypto.h"
#include "grinwalletshortcutbridge.h"
#include "grinwalletnodesync.h"
#include "grinwalletnodesyncservice.h"
#include "grinwalletworkflowhelpers.h"
#include "grinwalletworkflowservice.h"
#include "binaryslatev4reader.h"
#include "binaryslatev4writer.h"
#include "walletcryptobackend.h"
#include "nodeforeignapi.h"
#include "result.h"
#include "tip.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "transaction.h"

/**
 * @brief GrinWalletController::GrinWalletController
 * @param parent
 */
GrinWalletController::GrinWalletController(QObject *parent) :
    QObject(parent),
    m_nodeApi(0),
    m_nodeSyncService(new GrinWalletNodeSyncService(this)),
    m_workflowService(new GrinWalletWorkflowService(this)),
    m_shortcutBridge(new GrinWalletShortcutBridge(this)),
    m_autoRefreshTimer(0),
    m_sessionLockTimer(0),
    m_walletExists(false),
    m_walletUnlocked(false),
    m_selectedNetwork(GrinWalletControllerHelpers::defaultNetworkName()),
    m_storagePersistenceState(QStringLiteral("unknown")),
    m_chainHeight(0),
    m_nodeBlockHeaderVersion(0),
    m_syncStatus(QStringLiteral("Idle")),
    m_totalBalance(QStringLiteral("0.000000000")),
    m_spendableBalance(QStringLiteral("0.000000000")),
    m_lockedBalance(QStringLiteral("0.000000000")),
    m_immatureBalance(QStringLiteral("0.000000000")),
    m_awaitingConfirmationBalance(QStringLiteral("0.000000000")),
    m_awaitingFinalizationBalance(QStringLiteral("0.000000000")),
    m_scanHeight(0),
    m_autoLockOnAppDeactivate(false),
    m_walletScanInFlight(false),
    m_seedScanActive(false),
    m_seedScanNextIndex(1),
    m_pendingBroadcastInputLookup(false),
    m_broadcastStatusRefreshInFlight(false),
    m_kernelStatusCheckInFlight(false)
{
}

// -------------------------------------------------------------------------------------------------------
// Basic Controller Accessors
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletController::walletExists
 * @return
 */
bool GrinWalletController::walletExists() const { return m_walletExists; }

/**
 * @brief GrinWalletController::walletUnlocked
 * @return
 */
bool GrinWalletController::walletUnlocked() const { return m_walletUnlocked; }

/**
 * @brief GrinWalletController::walletName
 * @return
 */
QString GrinWalletController::walletName() const { return m_walletName; }

/**
 * @brief GrinWalletController::mnemonicPreview
 * @return
 */
QString GrinWalletController::mnemonicPreview() const { return m_mnemonicPreview; }

/**
 * @brief GrinWalletController::seedFingerprint
 * @return
 */
QString GrinWalletController::seedFingerprint() const { return m_seedFingerprint; }

/**
 * @brief GrinWalletController::selectedNetwork
 * @return
 */
QString GrinWalletController::selectedNetwork() const { return m_selectedNetwork; }

/**
 * @brief GrinWalletController::nodeUrl
 * @return
 */
QString GrinWalletController::nodeUrl() const { return m_nodeUrl; }

/**
 * @brief GrinWalletController::storagePersistenceState
 * @return
 */
QString GrinWalletController::storagePersistenceState() const { return m_storagePersistenceState; }

/**
 * @brief GrinWalletController::chainHeight
 * @return
 */
qulonglong GrinWalletController::chainHeight() const { return m_chainHeight; }

/**
 * @brief GrinWalletController::syncStatus
 * @return
 */
QString GrinWalletController::syncStatus() const { return m_syncStatus; }

/**
 * @brief GrinWalletController::totalBalance
 * @return
 */
QString GrinWalletController::totalBalance() const { return m_totalBalance; }

/**
 * @brief GrinWalletController::spendableBalance
 * @return
 */
QString GrinWalletController::spendableBalance() const { return m_spendableBalance; }

/**
 * @brief GrinWalletController::lockedBalance
 * @return
 */
QString GrinWalletController::lockedBalance() const { return m_lockedBalance; }

/**
 * @brief GrinWalletController::immatureBalance
 * @return
 */
QString GrinWalletController::immatureBalance() const { return m_immatureBalance; }

/**
 * @brief GrinWalletController::awaitingConfirmationBalance
 * @return
 */
QString GrinWalletController::awaitingConfirmationBalance() const { return m_awaitingConfirmationBalance; }

/**
 * @brief GrinWalletController::awaitingFinalizationBalance
 * @return
 */
QString GrinWalletController::awaitingFinalizationBalance() const { return m_awaitingFinalizationBalance; }

/**
 * @brief GrinWalletController::scanHeight
 * @return
 */
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }

/**
 * @brief GrinWalletController::lastError
 * @return
 */
QString GrinWalletController::lastError() const { return m_lastError; }

/**
 * @brief GrinWalletController::lastInfo
 * @return
 */
QString GrinWalletController::lastInfo() const { return m_lastInfo; }

/**
 * @brief GrinWalletController::workflowId
 * @return
 */
QString GrinWalletController::workflowId() const { return m_workflowId; }

/**
 * @brief GrinWalletController::workflowState
 * @return
 */
QString GrinWalletController::workflowState() const { return m_workflowState; }

/**
 * @brief GrinWalletController::workflowMode
 * @return
 */
QString GrinWalletController::workflowMode() const { return m_workflowMode; }

/**
 * @brief GrinWalletController::workflowSlatepack
 * @return
 */
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }

/**
 * @brief GrinWalletController::workflowDecoded
 * @return
 */
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }

/**
 * @brief GrinWalletController::autoLockOnAppDeactivate
 * @return
 */
bool GrinWalletController::autoLockOnAppDeactivate() const { return m_autoLockOnAppDeactivate; }

/**
 * @brief GrinWalletController::sessionMnemonic
 * @return
 */
QString GrinWalletController::sessionMnemonic() const { return m_sessionMnemonic; }

/**
 * @brief GrinWalletController::hasUnlockedSession
 * @return
 */
bool GrinWalletController::hasUnlockedSession() const
{
    return m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty();
}

/**
 * @brief GrinWalletController::nodeApi
 * @return
 */
NodeForeignApi *GrinWalletController::nodeApi() const { return m_nodeApi; }

/**
 * @brief GrinWalletController::setNodeApi
 * @param nodeApi
 */
void GrinWalletController::setNodeApi(NodeForeignApi *nodeApi) { m_nodeApi = nodeApi; }

/**
 * @brief GrinWalletController::setSyncStatusMessage
 * @param status
 */
void GrinWalletController::setSyncStatusMessage(const QString &status) { m_syncStatus = status; }

/**
 * @brief GrinWalletController::notifyStatusChanged
 */
void GrinWalletController::notifyStatusChanged() { emit statusChanged(); }

/**
 * @brief GrinWalletController::setChainHeightValue
 * @param height
 */
void GrinWalletController::setChainHeightValue(qulonglong height) { m_chainHeight = height; }

/**
 * @brief GrinWalletController::setNodeBlockHeaderVersionValue
 * @param version
 */
void GrinWalletController::setNodeBlockHeaderVersionValue(int version) { m_nodeBlockHeaderVersion = version; }

/**
 * @brief GrinWalletController::walletScanInFlight
 * @return
 */
bool GrinWalletController::walletScanInFlight() const { return m_walletScanInFlight; }

/**
 * @brief GrinWalletController::setWalletScanInFlight
 * @param inFlight
 */
void GrinWalletController::setWalletScanInFlight(bool inFlight) { m_walletScanInFlight = inFlight; }

/**
 * @brief GrinWalletController::seedScanActive
 * @return
 */
bool GrinWalletController::seedScanActive() const { return m_seedScanActive; }

/**
 * @brief GrinWalletController::setSeedScanActive
 * @param active
 */
void GrinWalletController::setSeedScanActive(bool active) { m_seedScanActive = active; }

/**
 * @brief GrinWalletController::seedScanNextIndex
 * @return
 */
qulonglong GrinWalletController::seedScanNextIndex() const { return m_seedScanNextIndex; }

/**
 * @brief GrinWalletController::setSeedScanNextIndex
 * @param nextIndex
 */
void GrinWalletController::setSeedScanNextIndex(qulonglong nextIndex) { m_seedScanNextIndex = nextIndex; }

/**
 * @brief GrinWalletController::seedScanDiscovered
 * @return
 */
const QList<WalletOutput> &GrinWalletController::seedScanDiscovered() const { return m_seedScanDiscovered; }

/**
 * @brief GrinWalletController::clearSeedScanDiscovered
 */
void GrinWalletController::clearSeedScanDiscovered() { m_seedScanDiscovered.clear(); }

/**
 * @brief GrinWalletController::appendSeedScanDiscovered
 * @param output
 */
void GrinWalletController::appendSeedScanDiscovered(const WalletOutput &output) { m_seedScanDiscovered.append(output); }

/**
 * @brief GrinWalletController::pendingBroadcastWorkflowId
 * @return
 */
QString GrinWalletController::pendingBroadcastWorkflowId() const { return m_pendingBroadcastWorkflowId; }

/**
 * @brief GrinWalletController::setPendingBroadcastWorkflowId
 * @param workflowId
 */
void GrinWalletController::setPendingBroadcastWorkflowId(const QString &workflowId) { m_pendingBroadcastWorkflowId = workflowId; }

/**
 * @brief GrinWalletController::pendingBroadcastInputLookup
 * @return
 */
bool GrinWalletController::pendingBroadcastInputLookup() const { return m_pendingBroadcastInputLookup; }

/**
 * @brief GrinWalletController::setPendingBroadcastInputLookup
 * @param pending
 */
void GrinWalletController::setPendingBroadcastInputLookup(bool pending) { m_pendingBroadcastInputLookup = pending; }

/**
 * @brief GrinWalletController::pendingBroadcastTxSkeleton
 * @return
 */
QJsonObject GrinWalletController::pendingBroadcastTxSkeleton() const { return m_pendingBroadcastTxSkeleton; }

/**
 * @brief GrinWalletController::setPendingBroadcastTxSkeleton
 * @param txSkeleton
 */
void GrinWalletController::setPendingBroadcastTxSkeleton(const QJsonObject &txSkeleton) { m_pendingBroadcastTxSkeleton = txSkeleton; }

/**
 * @brief GrinWalletController::pendingBroadcastInputCommits
 * @return
 */
QJsonArray GrinWalletController::pendingBroadcastInputCommits() const { return m_pendingBroadcastInputCommits; }

/**
 * @brief GrinWalletController::setPendingBroadcastInputCommits
 * @param commits
 */
void GrinWalletController::setPendingBroadcastInputCommits(const QJsonArray &commits) { m_pendingBroadcastInputCommits = commits; }

/**
 * @brief GrinWalletController::hasPendingBroadcastWorkflow
 * @return
 */
bool GrinWalletController::hasPendingBroadcastWorkflow() const { return !m_pendingBroadcastWorkflowId.isEmpty(); }

/**
 * @brief GrinWalletController::broadcastStatusRefreshInFlight
 * @return
 */
bool GrinWalletController::broadcastStatusRefreshInFlight() const { return m_broadcastStatusRefreshInFlight; }

/**
 * @brief GrinWalletController::setBroadcastStatusRefreshInFlight
 * @param inFlight
 */
void GrinWalletController::setBroadcastStatusRefreshInFlight(bool inFlight) { m_broadcastStatusRefreshInFlight = inFlight; }

/**
 * @brief GrinWalletController::kernelStatusCheckInFlight
 * @return
 */
bool GrinWalletController::kernelStatusCheckInFlight() const { return m_kernelStatusCheckInFlight; }

/**
 * @brief GrinWalletController::setKernelStatusCheckInFlight
 * @param inFlight
 */
void GrinWalletController::setKernelStatusCheckInFlight(bool inFlight) { m_kernelStatusCheckInFlight = inFlight; }

/**
 * @brief GrinWalletController::clearKernelStatusQueue
 */
void GrinWalletController::clearKernelStatusQueue() { m_kernelStatusQueue.clear(); }

/**
 * @brief GrinWalletController::hasPendingKernelStatusChecks
 * @return
 */
bool GrinWalletController::hasPendingKernelStatusChecks() const { return !m_kernelStatusQueue.isEmpty(); }

/**
 * @brief GrinWalletController::appendKernelStatusCheck
 * @param workflowId
 * @param excess
 */
void GrinWalletController::appendKernelStatusCheck(const QString &workflowId, const QString &excess)
{
    m_kernelStatusQueue.append(qMakePair(workflowId, excess));
}

/**
 * @brief GrinWalletController::takeNextKernelStatusCheck
 * @return
 */
QPair<QString, QString> GrinWalletController::takeNextKernelStatusCheck()
{
    return m_kernelStatusQueue.isEmpty() ? QPair<QString, QString>() : m_kernelStatusQueue.takeFirst();
}

/**
 * @brief GrinWalletController::currentKernelWorkflowIdInternal
 * @return
 */
QString GrinWalletController::currentKernelWorkflowIdInternal() const { return m_currentKernelWorkflowId; }

/**
 * @brief GrinWalletController::currentKernelExcessInternal
 * @return
 */
QString GrinWalletController::currentKernelExcessInternal() const { return m_currentKernelExcess; }

/**
 * @brief GrinWalletController::setCurrentKernelCheck
 * @param workflowId
 * @param excess
 */
void GrinWalletController::setCurrentKernelCheck(const QString &workflowId, const QString &excess)
{
    m_currentKernelWorkflowId = workflowId;
    m_currentKernelExcess = excess;
}

/**
 * @brief GrinWalletController::clearCurrentKernelCheck
 */
void GrinWalletController::clearCurrentKernelCheck()
{
    m_currentKernelWorkflowId.clear();
    m_currentKernelExcess.clear();
}

/**
 * @brief GrinWalletController::transactionHistory
 * @return
 */
QVariantList GrinWalletController::transactionHistory() const
{
    QVariantList history;
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    QList<QJsonObject> entries;

    entries.reserve(transactions.size());
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = GrinWalletHistoryHelpers::inferredConfirmedHeightForTransactionEntry(entry, outputs);
        const QString status = entry.value(QStringLiteral("status")).toString();
        qint64 confirmations = 0;
        if (confirmedHeight > 0 && m_chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<qint64>(m_chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
        }
        if (confirmedHeight > 0) {
            entry.insert(QStringLiteral("confirmed_height"), confirmedHeight);
        }
        entry.insert(QStringLiteral("confirmations"), confirmations);
        if (confirmedHeight > 0 && confirmations > 0 && status != QStringLiteral("cancelled")) {
            entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
        }
        const QString displayAmount = GrinWalletControllerHelpers::displayAmountForTransactionEntry(entry, outputs);
        if (!displayAmount.isEmpty()) {
            entry.insert(QStringLiteral("amount"), displayAmount);
        }
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), GrinWalletController::transactionEntryLessThan);

    history.reserve(entries.size());
    for (const QJsonObject &entry : entries) {
        history.append(entry.toVariantMap());
    }
    return history;
}

/**
 * @brief GrinWalletController::walletOutputs
 * @return
 */
QVariantList GrinWalletController::walletOutputs() const
{
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    std::sort(outputs.begin(), outputs.end(), GrinWalletController::walletOutputLessThan);

    QVariantList list;

    list.reserve(outputs.size());
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        QJsonObject entry = output.toJson();

        QString status;
        if (output.spent) {
            status = QStringLiteral("spent");
        } else if (output.pending) {
            status = QStringLiteral("pending");
        } else if (output.locked) {
            status = QStringLiteral("locked");
        } else if (output.coinbase && (!output.onChain || output.height == 0 || m_chainHeight < output.height + 1000)) {
            status = QStringLiteral("immature");
        } else if (!output.onChain) {
            status = QStringLiteral("local");
        } else {
            status = QStringLiteral("spendable");
        }

        const bool mature = !output.coinbase || (output.height > 0 && m_chainHeight >= output.height + 1000);
        const bool confirmed = output.onChain && output.height > 0 && m_chainHeight >= output.height + 10;
        const qint64 confirmations =
            output.height > 0 && m_chainHeight >= output.height
                ? static_cast<qint64>(m_chainHeight - output.height + 1)
                : 0;
        const bool spendable = !output.spent && !output.locked && !output.pending && output.onChain && mature && confirmed;

        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("mature"), mature);
        entry.insert(QStringLiteral("confirmed"), confirmed);
        entry.insert(QStringLiteral("confirmations"), confirmations);
        entry.insert(QStringLiteral("spendable"), spendable);
        list.append(entry.toVariantMap());
    }

    return list;
}

/**
 * @brief GrinWalletController::initialize
 */
void GrinWalletController::initialize()
{
    // -------------------------------------------------------------------------------------------------------
    // Setting Application Runtime Services
    // -------------------------------------------------------------------------------------------------------
    m_shortcutBridge->install();
    loadFromStorage();
    connectNodeClient();
    startAutoRefresh();
    refreshStoragePersistenceState();
    refreshNodeStatus();
}

/**
 * @brief GrinWalletController::generateMnemonic
 * @return
 */
QString GrinWalletController::generateMnemonic() const
{
    return GrinWalletSeedCrypto::generateMnemonic();
}

/**
 * @brief GrinWalletController::validateMnemonic
 * @param mnemonic
 * @return
 */
bool GrinWalletController::validateMnemonic(const QString &mnemonic) const
{
    return GrinWalletSeedCrypto::isValidMnemonic(mnemonic);
}

/**
 * @brief GrinWalletController::clearLastError
 */
void GrinWalletController::clearLastError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    setLastError(QString());
}

/**
 * @brief GrinWalletController::refreshNodeStatus
 */
void GrinWalletController::refreshNodeStatus()
{
    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    m_syncStatus = QStringLiteral("Querying node...");
    emit statusChanged();
    m_nodeApi->getTipAsync();
    m_nodeApi->getVersionAsync();
}

/**
 * @brief GrinWalletController::syncWallet
 */
void GrinWalletController::syncWallet()
{

    touchWalletSession();
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before running a wallet sync."));
        setLastInfo(QStringLiteral("Wallet sync was skipped because the wallet is locked."));
        return;
    }

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        setLastError(QStringLiteral("Node tip is not available yet. Try sync again after refresh."));
        return;
    }
    requestWalletScan();
}

/**
 * @brief GrinWalletController::rescanWallet
 */
void GrinWalletController::rescanWallet()
{

    touchWalletSession();
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before starting a full rescan."));
        setLastInfo(QStringLiteral("Full rescan was skipped because the wallet is locked."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastError(QStringLiteral("A wallet scan is already running."));
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("transaction_rescan_backup"),
                       walletState.value(QStringLiteral("transactions")).toArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 0);
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("full-rescan"));
    walletState.insert(QStringLiteral("last_synced_at"), QString());
    document.insert(QStringLiteral("wallet_state"), walletState);
    GrinWalletStorage::saveDocument(document);
    refreshStateFromStorage();

    m_seedScanNextIndex = 1;
    setLastError(QString());

    setLastInfo(QStringLiteral("Full wallet rescan queued from the beginning."));

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        return;
    }

    requestWalletScan();
}

/**
 * @brief GrinWalletController::requestPasteText
 * @return
 */
QString GrinWalletController::requestPasteText() const
{
    return GrinWalletPlatformHelpers::requestPasteText();
}

/**
 * @brief GrinWalletController::copyTextToClipboard
 * @param text
 * @return
 */
bool GrinWalletController::copyTextToClipboard(const QString &text) const
{
    return GrinWalletPlatformHelpers::copyTextToClipboard(text);
}

/**
 * @brief GrinWalletController::downloadTextFile
 * @param suggestedName
 * @param text
 * @return
 */
bool GrinWalletController::downloadTextFile(const QString &suggestedName, const QString &text) const
{
    return GrinWalletPlatformHelpers::downloadTextFile(suggestedName, text);
}

/**
 * @brief GrinWalletController::requestPersistentBrowserStorage
 */
void GrinWalletController::requestPersistentBrowserStorage()
{
    GrinWalletPlatformHelpers::requestPersistentBrowserStorage();
#ifdef Q_OS_WASM
    setLastInfo(QStringLiteral("Browser persistent-storage request sent. Re-check the storage status after the browser responds."));
#else
    setLastInfo(QStringLiteral("Persistent browser storage is only relevant for the WASM/browser wallet."));
#endif
    refreshStoragePersistenceState();
}

/**
 * @brief GrinWalletController::updateBrowserShortcutContext
 * @param text
 * @param selectedText
 * @param focused
 */
void GrinWalletController::updateBrowserShortcutContext(const QString &text,
                                                        const QString &selectedText,
                                                        bool focused) const
{
    m_shortcutBridge->updateBrowserShortcutContext(text, selectedText, focused);
}

/**
 * @brief GrinWalletController::isValidNodeUrl
 * @param nodeUrl
 * @return
 */
bool GrinWalletController::isValidNodeUrl(const QString &nodeUrl) const
{
    return GrinWalletControllerHelpers::isNodeUrlAccepted(nodeUrl);
}

/**
 * @brief GrinWalletController::createSlatepackTemplate
 * @param sender
 * @return
 */
QString GrinWalletController::createSlatepackTemplate(const QString &sender) const
{
    QJsonObject slate;
    slate.insert(QStringLiteral("ver"), QStringLiteral("4:3"));
    slate.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    slate.insert(QStringLiteral("sta"), QStringLiteral("S1"));
    slate.insert(QStringLiteral("amt"), QStringLiteral("0.000000000"));
    slate.insert(QStringLiteral("wallet"), m_walletName);
    slate.insert(QStringLiteral("network"), resolvedNetworkName());
    return encodeSlatepack(QString::fromUtf8(QJsonDocument(slate).toJson(QJsonDocument::Compact)),
                           sender.trimmed());
}

/**
 * @brief GrinWalletController::startSendWorkflow
 * @param amount
 * @param note
 */
void GrinWalletController::startSendWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startSendWorkflow(amount, note);
}

/**
 * @brief GrinWalletController::startReceiveWorkflow
 * @param amount
 * @param note
 */
void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startReceiveWorkflow(amount, note);
}

/**
 * @brief GrinWalletController::processWorkflowSlatepack
 * @param slatepack
 */
void GrinWalletController::processWorkflowSlatepack(const QString &slatepack)
{
    m_workflowService->processWorkflowSlatepack(slatepack);
}

/**
 * @brief GrinWalletController::clearWorkflow
 */
void GrinWalletController::clearWorkflow()
{
    m_workflowService->clearWorkflow();
}

/**
 * @brief GrinWalletController::cleanupLocalAndCancelledItems
 */
void GrinWalletController::cleanupLocalAndCancelledItems()
{
    m_workflowService->cleanupLocalAndCancelledItems();
}

/**
 * @brief GrinWalletController::broadcastCurrentWorkflowTransaction
 */
void GrinWalletController::broadcastCurrentWorkflowTransaction()
{
    m_workflowService->broadcastCurrentWorkflowTransaction();
}

/**
 * @brief GrinWalletController::broadcastTransaction
 * @param workflowId
 */
void GrinWalletController::broadcastTransaction(const QString &workflowId)
{
    m_workflowService->broadcastTransaction(workflowId);
}

/**
 * @brief GrinWalletController::beginBroadcastWithInputPreflight
 * @param workflowId
 * @param txSkeleton
 */
void GrinWalletController::beginBroadcastWithInputPreflight(const QString &workflowId,
                                                            const QJsonObject &txSkeleton)
{
    m_nodeSyncService->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
}

/**
 * @brief GrinWalletController::cancelTransaction
 * @param workflowId
 */
void GrinWalletController::cancelTransaction(const QString &workflowId)
{
    m_workflowService->cancelTransaction(workflowId);
}

/**
 * @brief GrinWalletController::encodeSlatepack
 * @param slateJson
 * @param sender
 * @return
 */
QString GrinWalletController::encodeSlatepack(const QString &slateJson, const QString &sender) const
{
    const QString trimmed = slateJson.trimmed();

    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8());
    if (document.isObject()) {
        const SlateV4 slate = SlateV4::fromJson(document.object());
        if (slate.state != SlateV4::Unknown && !slate.id.trimmed().isEmpty()) {
            QString binarySlatepack;
            QString writerError;
            if (BinarySlateV4Writer::encodeSlatepack(
                    slate,
                    &binarySlatepack,
                    &writerError,
                    sender.trimmed(),
                    QStringList(),
                    currentSlatepackSecret())) {
                return binarySlatepack;
            }
        }
    }

    return GrinWalletWorkflowHelpers::encodeSlatepackArmor(trimmed, sender.trimmed());
}

/**
 * @brief GrinWalletController::decodeSlatepack
 * @param slatepack
 * @return
 */
QString GrinWalletController::decodeSlatepack(const QString &slatepack) const
{
    QByteArray decryptionKey;
    if (m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            decryptionKey = keychain.slatepackSecretKey();
        }
    }
    return GrinWalletWorkflowHelpers::decodeIncomingSlatepack(slatepack, decryptionKey);
}













