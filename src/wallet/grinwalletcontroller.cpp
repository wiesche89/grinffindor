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
 * @brief Constructs and initializes the wallet controller.
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
    m_initialized(false),
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
 * @brief Returns whether a wallet is configured for the active network.
 * @return
 */
bool GrinWalletController::walletExists() const { return m_walletExists; }

/**
 * @brief Returns whether the active wallet session is unlocked.
 * @return
 */
bool GrinWalletController::walletUnlocked() const { return m_walletUnlocked; }

/**
 * @brief Returns the active wallet display name.
 * @return
 */
QString GrinWalletController::walletName() const { return m_walletName; }

/**
 * @brief Returns the currently shown mnemonic preview text.
 * @return
 */
QString GrinWalletController::mnemonicPreview() const { return m_mnemonicPreview; }

/**
 * @brief Returns the fingerprint of the active wallet seed.
 * @return
 */
QString GrinWalletController::seedFingerprint() const { return m_seedFingerprint; }

/**
 * @brief Returns the currently selected network name.
 * @return
 */
QString GrinWalletController::selectedNetwork() const { return m_selectedNetwork; }

/**
 * @brief Returns the configured node URL.
 * @return
 */
QString GrinWalletController::nodeUrl() const { return m_nodeUrl; }

/**
 * @brief Returns the browser storage persistence status.
 * @return
 */
QString GrinWalletController::storagePersistenceState() const { return m_storagePersistenceState; }

/**
 * @brief Returns the latest known chain height.
 * @return
 */
qulonglong GrinWalletController::chainHeight() const { return m_chainHeight; }

/**
 * @brief Returns the current node synchronization status text.
 * @return
 */
QString GrinWalletController::syncStatus() const { return m_syncStatus; }

/**
 * @brief Returns the total wallet balance.
 * @return
 */
QString GrinWalletController::totalBalance() const { return m_totalBalance; }

/**
 * @brief Returns the currently spendable wallet balance.
 * @return
 */
QString GrinWalletController::spendableBalance() const { return m_spendableBalance; }

/**
 * @brief Returns the currently locked wallet balance.
 * @return
 */
QString GrinWalletController::lockedBalance() const { return m_lockedBalance; }

/**
 * @brief Returns the immature wallet balance.
 * @return
 */
QString GrinWalletController::immatureBalance() const { return m_immatureBalance; }

/**
 * @brief Returns the balance awaiting on-chain confirmations.
 * @return
 */
QString GrinWalletController::awaitingConfirmationBalance() const { return m_awaitingConfirmationBalance; }

/**
 * @brief Returns the balance awaiting transaction finalization.
 * @return
 */
QString GrinWalletController::awaitingFinalizationBalance() const { return m_awaitingFinalizationBalance; }

/**
 * @brief Returns the latest scanned chain height recorded for this wallet.
 * @return
 */
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }

/**
 * @brief Returns the latest error message.
 * @return
 */
QString GrinWalletController::lastError() const { return m_lastError; }

/**
 * @brief Returns the latest informational status message.
 * @return
 */
QString GrinWalletController::lastInfo() const { return m_lastInfo; }

/**
 * @brief Returns the active workflow identifier.
 * @return
 */
QString GrinWalletController::workflowId() const { return m_workflowId; }

/**
 * @brief Returns the active workflow state code.
 * @return
 */
QString GrinWalletController::workflowState() const { return m_workflowState; }

/**
 * @brief Returns the active workflow mode.
 * @return
 */
QString GrinWalletController::workflowMode() const { return m_workflowMode; }

/**
 * @brief Returns the current workflow slatepack payload.
 * @return
 */
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }

/**
 * @brief Returns the decoded view of the current workflow payload.
 * @return
 */
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }

/**
 * @brief Returns whether auto-lock on app deactivation is enabled.
 * @return
 */
bool GrinWalletController::autoLockOnAppDeactivate() const { return m_autoLockOnAppDeactivate; }

/**
 * @brief Returns the in-memory session mnemonic.
 * @return
 */
QString GrinWalletController::sessionMnemonic() const { return m_sessionMnemonic; }

/**
 * @brief Returns whether an unlocked session with mnemonic material is available.
 * @return
 */
bool GrinWalletController::hasUnlockedSession() const
{
    return m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty();
}

/**
 * @brief Sets node api.
 * @return
 */
NodeForeignApi *GrinWalletController::nodeApi() const { return m_nodeApi; }

/**
 * @brief Sets node api.
 * @param nodeApi
 */
void GrinWalletController::setNodeApi(NodeForeignApi *nodeApi) { m_nodeApi = nodeApi; }

/**
 * @brief Sets sync status message.
 * @param status
 */
void GrinWalletController::setSyncStatusMessage(const QString &status) { m_syncStatus = status; }

/**
 * @brief Emits status-related change notifications.
 */
void GrinWalletController::notifyStatusChanged()
{
    emit nodeStatusChanged();
    emit statusChanged();
}

/**
 * @brief Sets chain height value.
 * @param height
 */
void GrinWalletController::setChainHeightValue(qulonglong height) { m_chainHeight = height; }

/**
 * @brief Sets node block header version value.
 * @param version
 */
void GrinWalletController::setNodeBlockHeaderVersionValue(int version) { m_nodeBlockHeaderVersion = version; }

/**
 * @brief Returns whether a wallet scan request is currently running.
 * @return
 */
bool GrinWalletController::walletScanInFlight() const { return m_walletScanInFlight; }

/**
 * @brief Sets wallet scan in flight.
 * @param inFlight
 */
void GrinWalletController::setWalletScanInFlight(bool inFlight) { m_walletScanInFlight = inFlight; }

/**
 * @brief Returns whether seed restoration scanning is currently active.
 * @return
 */
bool GrinWalletController::seedScanActive() const { return m_seedScanActive; }

/**
 * @brief Sets seed scan active.
 * @param active
 */
void GrinWalletController::setSeedScanActive(bool active) { m_seedScanActive = active; }

/**
 * @brief Returns the next seed index scheduled for scanning.
 * @return
 */
qulonglong GrinWalletController::seedScanNextIndex() const { return m_seedScanNextIndex; }

/**
 * @brief Sets seed scan next index.
 * @param nextIndex
 */
void GrinWalletController::setSeedScanNextIndex(qulonglong nextIndex) { m_seedScanNextIndex = nextIndex; }

/**
 * @brief Clears seed scan discovered.
 * @return
 */
const QList<WalletOutput> &GrinWalletController::seedScanDiscovered() const { return m_seedScanDiscovered; }

/**
 * @brief Clears seed scan discovered.
 */
void GrinWalletController::clearSeedScanDiscovered() { m_seedScanDiscovered.clear(); }

/**
 * @brief Appends a discovered output to the seed-scan results list.
 * @param output
 */
void GrinWalletController::appendSeedScanDiscovered(const WalletOutput &output) { m_seedScanDiscovered.append(output); }

/**
 * @brief Returns the workflow ID currently pending broadcast.
 * @return
 */
QString GrinWalletController::pendingBroadcastWorkflowId() const { return m_pendingBroadcastWorkflowId; }

/**
 * @brief Sets pending broadcast workflow id.
 * @param workflowId
 */
void GrinWalletController::setPendingBroadcastWorkflowId(const QString &workflowId) { m_pendingBroadcastWorkflowId = workflowId; }

/**
 * @brief Returns whether pending broadcast input lookup is in progress.
 * @return
 */
bool GrinWalletController::pendingBroadcastInputLookup() const { return m_pendingBroadcastInputLookup; }

/**
 * @brief Sets pending broadcast input lookup.
 * @param pending
 */
void GrinWalletController::setPendingBroadcastInputLookup(bool pending) { m_pendingBroadcastInputLookup = pending; }

/**
 * @brief Returns the pending transaction skeleton prepared for broadcast.
 * @return
 */
QJsonObject GrinWalletController::pendingBroadcastTxSkeleton() const { return m_pendingBroadcastTxSkeleton; }

/**
 * @brief Sets pending broadcast tx skeleton.
 * @param txSkeleton
 */
void GrinWalletController::setPendingBroadcastTxSkeleton(const QJsonObject &txSkeleton) { m_pendingBroadcastTxSkeleton = txSkeleton; }

/**
 * @brief Returns input commitments collected for pending broadcast validation.
 * @return
 */
QJsonArray GrinWalletController::pendingBroadcastInputCommits() const { return m_pendingBroadcastInputCommits; }

/**
 * @brief Sets pending broadcast input commits.
 * @param commits
 */
void GrinWalletController::setPendingBroadcastInputCommits(const QJsonArray &commits) { m_pendingBroadcastInputCommits = commits; }

/**
 * @brief Returns whether any workflow is waiting for broadcast.
 * @return
 */
bool GrinWalletController::hasPendingBroadcastWorkflow() const { return !m_pendingBroadcastWorkflowId.isEmpty(); }

/**
 * @brief Returns whether a broadcast-status refresh is currently running.
 * @return
 */
bool GrinWalletController::broadcastStatusRefreshInFlight() const { return m_broadcastStatusRefreshInFlight; }

/**
 * @brief Sets broadcast status refresh in flight.
 * @param inFlight
 */
void GrinWalletController::setBroadcastStatusRefreshInFlight(bool inFlight) { m_broadcastStatusRefreshInFlight = inFlight; }

/**
 * @brief Returns whether a kernel status check is currently running.
 * @return
 */
bool GrinWalletController::kernelStatusCheckInFlight() const { return m_kernelStatusCheckInFlight; }

/**
 * @brief Sets kernel status check in flight.
 * @param inFlight
 */
void GrinWalletController::setKernelStatusCheckInFlight(bool inFlight) { m_kernelStatusCheckInFlight = inFlight; }

/**
 * @brief Clears kernel status queue.
 */
void GrinWalletController::clearKernelStatusQueue() { m_kernelStatusQueue.clear(); }

/**
 * @brief Returns whether kernel status checks are queued.
 * @return
 */
bool GrinWalletController::hasPendingKernelStatusChecks() const { return !m_kernelStatusQueue.isEmpty(); }

/**
 * @brief Appends a kernel status check request to the processing queue.
 * @param workflowId
 * @param excess
 */
void GrinWalletController::appendKernelStatusCheck(const QString &workflowId, const QString &excess)
{
    m_kernelStatusQueue.append(qMakePair(workflowId, excess));
}

/**
 * @brief Returns and removes the next pending kernel status check request.
 * @return
 */
QPair<QString, QString> GrinWalletController::takeNextKernelStatusCheck()
{
    return m_kernelStatusQueue.isEmpty() ? QPair<QString, QString>() : m_kernelStatusQueue.takeFirst();
}

/**
 * @brief Returns the internally tracked workflow ID for kernel checks.
 * @return
 */
QString GrinWalletController::currentKernelWorkflowIdInternal() const { return m_currentKernelWorkflowId; }

/**
 * @brief Returns the internally tracked kernel excess commitment.
 * @return
 */
QString GrinWalletController::currentKernelExcessInternal() const { return m_currentKernelExcess; }

/**
 * @brief Sets current kernel check.
 * @param workflowId
 * @param excess
 */
void GrinWalletController::setCurrentKernelCheck(const QString &workflowId, const QString &excess)
{
    m_currentKernelWorkflowId = workflowId;
    m_currentKernelExcess = excess;
}

/**
 * @brief Clears current kernel check.
 */
void GrinWalletController::clearCurrentKernelCheck()
{
    m_currentKernelWorkflowId.clear();
    m_currentKernelExcess.clear();
}

/**
 * @brief Returns the wallet transaction history enriched with computed status data.
 * @return
 */
QVariantList GrinWalletController::transactionHistory() const
{
    return m_transactionHistoryCache;
}

/**
 * @brief Builds wallet transaction history enriched with computed status data.
 * @return
 */
QVariantList GrinWalletController::buildTransactionHistory() const
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
 * @brief Returns wallet outputs enriched with maturity and spendability metadata.
 * @return
 */
QVariantList GrinWalletController::walletOutputs() const
{
    return m_walletOutputsCache;
}

/**
 * @brief Builds wallet outputs enriched with maturity and spendability metadata.
 * @return
 */
QVariantList GrinWalletController::buildWalletOutputs() const
{
    const QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    const QJsonObject workflowContexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    std::sort(outputs.begin(), outputs.end(), GrinWalletController::walletOutputLessThan);

    QVariantList list;

    list.reserve(outputs.size());
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        QJsonObject entry = output.toJson();

        const bool mature = !output.coinbase
            || output.height == 0
            || m_chainHeight == 0
            || m_chainHeight >= output.height + 1000;
        const qint64 confirmations =
            output.onChain && output.height > 0 && m_chainHeight >= output.height
                ? static_cast<qint64>(m_chainHeight - output.height + 1)
                : (output.onChain && !output.coinbase && output.height == 0 ? 10 : 0);
        const bool confirmed = confirmations >= 10;

        QString status;
        if (output.spent) {
            status = QStringLiteral("spent");
        } else if (output.locked) {
            status = QStringLiteral("locked");
        } else if (output.pending) {
            status = output.coinbase
                ? QStringLiteral("immature")
                : QStringLiteral("awaiting_finalization");
        } else if (!output.onChain) {
            status = output.coinbase
                ? QStringLiteral("immature")
                : QStringLiteral("awaiting_finalization");
        } else if (output.coinbase && !mature) {
            status = QStringLiteral("immature");
        } else if (!confirmed) {
            status = QStringLiteral("awaiting_confirmation");
        } else {
            status = QStringLiteral("spendable");
        }
        const bool spendable = !output.spent && !output.locked && !output.pending && output.onChain && mature && confirmed;

        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("mature"), mature);
        entry.insert(QStringLiteral("confirmed"), confirmed);
        entry.insert(QStringLiteral("confirmations"), confirmations);
        entry.insert(QStringLiteral("spendable"), spendable);

        if (output.locked) {
            QString lockWorkflowId;
            QString lockWorkflowState;
            QString lockWorkflowMode;
            QString lockWorkflowStatus;

            const QStringList contextKeys = workflowContexts.keys();
            for (int j = 0; j < contextKeys.size(); ++j) {
                const QString workflowId = contextKeys.at(j);
                const QJsonObject context = workflowContexts.value(workflowId).toObject();
                const QJsonArray selectedInputCommits = context.value(QStringLiteral("selected_input_commits")).toArray();

                bool foundCommit = false;
                for (int k = 0; k < selectedInputCommits.size(); ++k) {
                    if (selectedInputCommits.at(k).toString() == output.commitment) {
                        foundCommit = true;
                        break;
                    }
                }

                if (!foundCommit) {
                    continue;
                }

                for (int k = 0; k < transactions.size(); ++k) {
                    const QJsonObject tx = transactions.at(k).toObject();
                    if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
                        continue;
                    }

                    const QString txStatus = tx.value(QStringLiteral("status")).toString();
                    if (GrinWalletControllerHelpers::isFinalTransactionStatus(txStatus)) {
                        break;
                    }

                    lockWorkflowId = workflowId;
                    lockWorkflowState = tx.value(QStringLiteral("state")).toString();
                    lockWorkflowMode = tx.value(QStringLiteral("mode")).toString();
                    lockWorkflowStatus = txStatus;
                    break;
                }

                if (!lockWorkflowId.isEmpty()) {
                    break;
                }
            }

            if (!lockWorkflowId.isEmpty()) {
                entry.insert(QStringLiteral("lock_workflow_id"), lockWorkflowId);
                entry.insert(QStringLiteral("lock_workflow_state"), lockWorkflowState);
                entry.insert(QStringLiteral("lock_workflow_mode"), lockWorkflowMode);
                entry.insert(QStringLiteral("lock_workflow_status"), lockWorkflowStatus);
            }
        }

        list.append(entry.toVariantMap());
    }

    return list;

    return list;
}

/**
 * @brief Initializes runtime services and loads persisted wallet state.
 */
void GrinWalletController::initialize()
{
    if (m_initialized) {
        refreshStoragePersistenceState();
        return;
    }

    m_initialized = true;

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
 * @brief Generates a new mnemonic phrase for wallet creation.
 * @return
 */
QString GrinWalletController::generateMnemonic() const
{
    return GrinWalletSeedCrypto::generateMnemonic();
}

/**
 * @brief Validates mnemonic.
 * @param mnemonic
 * @return
 */
bool GrinWalletController::validateMnemonic(const QString &mnemonic) const
{
    return GrinWalletSeedCrypto::isValidMnemonic(mnemonic);
}

/**
 * @brief Clears last error.
 */
void GrinWalletController::clearLastError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    setLastError(QString());
}

/**
 * @brief Refreshes node status.
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
    emit nodeStatusChanged();
    emit statusChanged();
    m_nodeApi->getTipAsync();
    m_nodeApi->getVersionAsync();
}

/**
 * @brief Synchronizes wallet.
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
 * @brief Processes rescan wallet.
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
 * @brief Requests paste text.
 * @return
 */
QString GrinWalletController::requestPasteText() const
{
    return GrinWalletPlatformHelpers::requestPasteText();
}

/**
 * @brief Copies the provided text to the system clipboard.
 * @param text
 * @return
 */
bool GrinWalletController::copyTextToClipboard(const QString &text) const
{
    return GrinWalletPlatformHelpers::copyTextToClipboard(text);
}

/**
 * @brief Downloads the provided text as a file using platform integration.
 * @param suggestedName
 * @param text
 * @return
 */
bool GrinWalletController::downloadTextFile(const QString &suggestedName, const QString &text) const
{
    return GrinWalletPlatformHelpers::downloadTextFile(suggestedName, text);
}

/**
 * @brief Requests persistent browser storage.
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
 * @brief Updates browser shortcut context.
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
 * @brief Returns whether the provided node URL is accepted by wallet rules.
 * @param nodeUrl
 * @return
 */
bool GrinWalletController::isValidNodeUrl(const QString &nodeUrl) const
{
    return GrinWalletControllerHelpers::isNodeUrlAccepted(nodeUrl);
}

/**
 * @brief Creates slatepack template.
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
 * @brief Starts send workflow.
 * @param amount
 * @param note
 */
void GrinWalletController::startSendWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startSendWorkflow(amount, note);
}

/**
 * @brief Starts receive workflow.
 * @param amount
 * @param note
 */
void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startReceiveWorkflow(amount, note);
}

/**
 * @brief Processes a workflow slatepack through the workflow service.
 * @param slatepack
 */
void GrinWalletController::processWorkflowSlatepack(const QString &slatepack)
{
    m_workflowService->processWorkflowSlatepack(slatepack);
}

/**
 * @brief Clears workflow.
 */
void GrinWalletController::clearWorkflow()
{
    m_workflowService->clearWorkflow();
}

/**
 * @brief Cleans up locally staged and cancelled transaction artifacts.
 */
void GrinWalletController::cleanupLocalAndCancelledItems()
{
    m_workflowService->cleanupLocalAndCancelledItems();
}

/**
 * @brief Broadcasts the transaction of the currently active workflow.
 */
void GrinWalletController::broadcastCurrentWorkflowTransaction()
{
    m_workflowService->broadcastCurrentWorkflowTransaction();
}

/**
 * @brief Broadcasts the transaction associated with the given workflow ID.
 * @param workflowId
 */
void GrinWalletController::broadcastTransaction(const QString &workflowId)
{
    m_workflowService->broadcastTransaction(workflowId);
}

/**
 * @brief Starts transaction broadcast after input preflight verification.
 * @param workflowId
 * @param txSkeleton
 */
void GrinWalletController::beginBroadcastWithInputPreflight(const QString &workflowId,
                                                            const QJsonObject &txSkeleton)
{
    m_nodeSyncService->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
}

/**
 * @brief Cancels the transaction associated with the given workflow ID.
 * @param workflowId
 */
void GrinWalletController::cancelTransaction(const QString &workflowId)
{
    m_workflowService->cancelTransaction(workflowId);
}

/**
 * @brief Encodes slate JSON into a binary or armored slatepack payload.
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
 * @brief Decodes an incoming slatepack into readable slate JSON.
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













