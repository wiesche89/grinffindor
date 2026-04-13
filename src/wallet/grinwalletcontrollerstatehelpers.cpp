#include "grinwalletcontroller.h"

#include <QGuiApplication>
#include <QThread>
#include <QTimer>

#include "grinwalletcontrollerhelpers.h"
#include "grinwallethistoryhelpers.h"
#include "grinwalletnodesyncservice.h"
#include "grinwalletplatformhelpers.h"
#include "grinwalletstorage.h"
#include "grinwallettransactionstore.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"
#include "walletscanner.h"

namespace {

const int kSessionAutoLockIntervalMs = 15 * 60 * 1000;

} // namespace

/**
 * @brief Returns the active network name or a default when invalid.
 * @return Normalized network identifier.
 */
QString GrinWalletController::resolvedNetworkName() const
{
    return GrinWalletControllerHelpers::isAcceptedNetworkName(m_selectedNetwork) ? m_selectedNetwork : GrinWalletControllerHelpers::defaultNetworkName();
}

/**
 * @brief Updates the last error message and notifies observers.
 * @param error Error message text.
 */
void GrinWalletController::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

/**
 * @brief Updates the last informational message and notifies observers.
 * @param info Informational message text.
 */
void GrinWalletController::setLastInfo(const QString &info)
{
    m_lastInfo = info;
    emit lastInfoChanged();
}

/**
 * @brief Stores the current workflow state snapshot and notifies observers.
 * @param id Workflow identifier.
 * @param mode Workflow mode string.
 * @param state Workflow state string.
 * @param slatepack Raw workflow slatepack text.
 * @param decoded Decoded workflow payload text.
 */
void GrinWalletController::setWorkflow(const QString &id, const QString &mode, const QString &state, const QString &slatepack, const QString &decoded)
{
    m_workflowId = id;
    m_workflowMode = mode;
    m_workflowState = state;
    m_workflowSlatepack = slatepack;
    m_workflowDecoded = decoded;
    emit workflowChanged();
}

/**
 * @brief Persists transaction-store changes and reloads cached controller state.
 * @param document Updated storage document.
 * @param changed True when a transaction update was applied.
 */
void GrinWalletController::finalizeTransactionStoreUpdate(const QJsonObject &document, bool changed)
{
    if (!changed) {
        return;
    }

    GrinWalletStorage::saveDocument(document);
    refreshStateFromStorage();
}

/**
 * @brief Writes output list and balance snapshot into wallet state.
 * @param document In/out storage document.
 * @param walletState In/out wallet_state object.
 * @param outputs Output list to persist.
 * @param nextChildIndex Next derivation index candidate.
 */
void GrinWalletController::storeOutputsState(QJsonObject *document,
                                             QJsonObject *walletState,
                                             const QList<WalletOutput> &outputs,
                                             quint32 nextChildIndex) const
{
    if (!document || !walletState) {
        return;
    }

    // -------------------------------------------------------------------------------------------------------
    // Persisting Outputs And Recomputed Balances
    // -------------------------------------------------------------------------------------------------------
    walletState->insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));

    walletState->insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight, m_minimumConfirmations));
    if (nextChildIndex > static_cast<quint32>(walletState->value(QStringLiteral("next_child_index")).toInt())) {
        walletState->insert(QStringLiteral("next_child_index"), static_cast<int>(nextChildIndex));
    }
    document->insert(QStringLiteral("wallet_state"), *walletState);
}

/**
 * @brief Orders transaction entries for history display.
 * @param left Left transaction entry.
 * @param right Right transaction entry.
 * @return True when left should be ordered before right.
 */
bool GrinWalletController::transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right)
{
    return GrinWalletHistoryHelpers::transactionEntryLessThan(left, right);
}

/**
 * @brief Orders wallet outputs by spend/lock/pending and recency priority.
 * @param left Left wallet output.
 * @param right Right wallet output.
 * @return True when left should be ordered before right.
 */
bool GrinWalletController::walletOutputLessThan(const WalletOutput &left, const WalletOutput &right)
{
    if (left.height != right.height) {
        return left.height > right.height;
    }
    if (left.childIndex != right.childIndex) {
        return left.childIndex > right.childIndex;
    }
    if (left.pending != right.pending) {
        return left.pending && !right.pending;
    }
    if (left.locked != right.locked) {
        return left.locked && !right.locked;
    }
    if (left.spent != right.spent) {
        return !left.spent && right.spent;
    }
    if (left.onChain != right.onChain) {
        return left.onChain && !right.onChain;
    }
    return left.commitment > right.commitment;
}

/**
 * @brief Marks a workflow transaction as broadcast pending.
 * @param workflowId Workflow identifier.
 */
void GrinWalletController::markTransactionBroadcastPending(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastPending(&document, workflowId));
}

/**
 * @brief Marks a workflow transaction as broadcast failed.
 * @param workflowId Workflow identifier.
 * @param message Failure reason.
 */
void GrinWalletController::markTransactionBroadcastFailed(const QString &workflowId, const QString &message)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastFailed(&document, workflowId, message));
}

/**
 * @brief Marks a workflow transaction kernel as confirmed on-chain.
 * @param workflowId Workflow identifier.
 * @param confirmedHeight Confirmed block height.
 */
void GrinWalletController::markTransactionKernelConfirmed(const QString &workflowId, qulonglong confirmedHeight)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document,
                                   GrinWalletTransactionStore::markKernelConfirmed(&document,
                                                                                   workflowId,
                                                                                   m_chainHeight,
                                                                                   confirmedHeight));
}

/**
 * @brief Marks a workflow transaction kernel as observed in mempool.
 * @param workflowId Workflow identifier.
 */
void GrinWalletController::markTransactionKernelBroadcasted(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markKernelBroadcasted(&document, workflowId));
}

/**
 * @brief Marks a workflow transaction as broadcast rejected.
 * @param workflowId Workflow identifier.
 * @param message Rejection reason.
 */
void GrinWalletController::markTransactionBroadcastRejected(const QString &workflowId, const QString &message)
{
    markTransactionBroadcastFailed(workflowId, message);
}

/**
 * @brief Marks a workflow transaction as broadcast succeeded.
 * @param workflowId Workflow identifier.
 */
void GrinWalletController::markTransactionBroadcastSucceeded(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastSucceeded(&document, workflowId));
}

/**
 * @brief Connects the node synchronization service to the current node endpoint.
 */
void GrinWalletController::connectNodeClient()
{
    m_nodeSyncService->connectNodeClient();
}

/**
 * @brief Reloads balances and scan state from persistent storage.
 */
void GrinWalletController::refreshStateFromStorage()
{
    // -------------------------------------------------------------------------------------------------------
    // Refreshing Derived Wallet Status Fields
    // -------------------------------------------------------------------------------------------------------
    GrinWalletStorage::RefreshedState state = GrinWalletStorage::refreshState(GrinWalletStorage::loadDocument(), m_chainHeight);
    if (state.documentChanged) {
        GrinWalletStorage::saveDocument(state.document);
    }

    m_scanHeight = state.scanHeight;
    m_totalBalance = state.totalBalance;
    m_spendableBalance = state.spendableBalance;
    m_lockedBalance = state.lockedBalance;
    m_immatureBalance = state.immatureBalance;
    m_awaitingConfirmationBalance = state.awaitingConfirmationBalance;
    m_awaitingFinalizationBalance = state.awaitingFinalizationBalance;
    m_revertedBalance = state.revertedBalance;
    refreshDerivedModels();
    emit walletSummaryChanged();
    emit statusChanged();
}

/**
 * @brief Refreshes cached UI-facing list models and emits narrow change signals.
 */
void GrinWalletController::refreshDerivedModels()
{
    const QVariantList nextWalletOutputs = buildWalletOutputs();
    if (m_walletOutputsCache != nextWalletOutputs) {
        m_walletOutputsCache = nextWalletOutputs;
        emit walletOutputsChanged();
    }

    const QVariantList nextTransactionHistory = buildTransactionHistory();
    if (m_transactionHistoryCache != nextTransactionHistory) {
        m_transactionHistoryCache = nextTransactionHistory;
        emit transactionHistoryChanged();
    }
}

/**
 * @brief Starts periodic refresh and auto-lock timers.
 */
void GrinWalletController::startAutoRefresh()
{
    if (m_autoRefreshTimer) {
        return;
    }

    // -------------------------------------------------------------------------------------------------------
    // Configuring Periodic Node Refresh
    // -------------------------------------------------------------------------------------------------------
    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(30000);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &GrinWalletController::refreshNodeStatus);

    m_autoRefreshTimer->start();

    // -------------------------------------------------------------------------------------------------------
    // Configuring Session Auto-Lock Timer
    // -------------------------------------------------------------------------------------------------------
    if (!m_sessionLockTimer) {
        m_sessionLockTimer = new QTimer(this);
        m_sessionLockTimer->setInterval(kSessionAutoLockIntervalMs);
        m_sessionLockTimer->setSingleShot(true);
        connect(m_sessionLockTimer, &QTimer::timeout, this, &GrinWalletController::onSessionLockTimeout);
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this, &GrinWalletController::onApplicationStateChanged);
}

/**
 * @brief Processes on session lock timeout.
 */
void GrinWalletController::onSessionLockTimeout()
{
    if (!m_walletUnlocked) {
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive || m_fullRescanInFlight) {
        touchWalletSession();
        return;
    }

    lockWallet();
    setLastInfo(QStringLiteral("Wallet locked after inactivity."));
}

/**
 * @brief Locks the wallet when the app is deactivated and auto-lock is enabled.
 * @param state New application state.
 */
void GrinWalletController::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (!m_walletUnlocked) {
        return;
    }
    if (!m_autoLockOnAppDeactivate) {
        return;
    }
    if (state == Qt::ApplicationHidden || state == Qt::ApplicationInactive) {
        if (m_walletScanInFlight || m_seedScanActive || m_fullRescanInFlight) {
            touchWalletSession();
            return;
        }
        lockWallet();
        setLastInfo(QStringLiteral("Wallet locked because the browser tab or app became inactive."));
    }
}

/**
 * @brief Loads the storage document for service operations.
 * @return Current storage document.
 */
QJsonObject GrinWalletController::loadDocumentForService() const
{
    return GrinWalletStorage::loadDocument();
}

/**
 * @brief Saves a storage document requested by a service component.
 * @param document Storage document to persist.
 * @return True on successful save.
 */
bool GrinWalletController::saveDocumentForService(const QJsonObject &document) const
{
    return updateDocumentForService([&document](QJsonObject *current) {
        if (!current) {
            return false;
        }
        *current = document;
        return true;
    });
}

bool GrinWalletController::updateDocumentForService(const std::function<bool(QJsonObject *)> &updater) const
{
    Q_ASSERT(thread() == QThread::currentThread());
    if (!updater) {
        return false;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (!updater(&document)) {
        return false;
    }
    return GrinWalletStorage::saveDocument(document);
}

/**
 * @brief Reads the next child index from wallet state.
 * @param walletState Wallet state object.
 * @return Next child derivation index.
 */
quint32 GrinWalletController::nextChildIndexFromStateForService(const QJsonObject &walletState) const
{
    return static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
}

/**
 * @brief Filters workflow contexts to the transaction set visible to services.
 * @param contexts Full workflow context object.
 * @param transactions Transaction array to keep.
 * @return Filtered workflow contexts.
 */
QJsonObject GrinWalletController::filterWorkflowContextsForTransactionsForService(const QJsonObject &contexts,
                                                                                  const QJsonArray &transactions) const
{
    return GrinWalletControllerHelpers::filterWorkflowContextsForTransactions(contexts, transactions);
}

/**
 * @brief Stores or updates an owned output derived from a commit object.
 * @param source Output source label.
 * @param amount Output amount string.
 * @param commit Derived commitment and proof.
 */
void GrinWalletController::storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit)
{
    if (commit.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();

    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commit.commitment) {
            return;
        }
    }

    WalletOutput output;
    output.commitment = commit.commitment;
    output.proof = commit.proof;
    output.amount = amount;
    output.source = source;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    storeOwnedOutput(output);
}

/**
 * @brief Stores or updates an owned wallet output in persistent state.
 * @param output Output to persist.
 */
void GrinWalletController::storeOwnedOutput(const WalletOutput &output)
{
    if (output.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();

    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == output.commitment) {
            outputs[i] = output;
            storeOutputsState(&document, &walletState, outputs, output.childIndex + 1);
            GrinWalletStorage::saveDocument(document);
            refreshStateFromStorage();
            return;
        }
    }

    outputs.append(output);
    storeOutputsState(&document, &walletState, outputs, output.childIndex + 1);
    GrinWalletStorage::saveDocument(document);
    refreshStateFromStorage();
}

/**
 * @brief Derives a new owned output and matching commit from the unlocked wallet.
 * @param source Output source label.
 * @param amount Output amount string.
 * @param outputOut Output wallet output record.
 * @param commitOut Output slate commit record.
 * @param errorOut Optional derivation error message.
 * @return True when output derivation succeeds.
 */
bool GrinWalletController::buildOwnedOutput(const QString &source,
                                            const QString &amount,
                                            WalletOutput *outputOut,
                                            SlateV4::Commit *commitOut,
                                            QString *errorOut) const
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output target is missing.");
        }
        return false;
    }
    if (!hasUnlockedSession()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet must be unlocked to derive owned outputs.");
        }
        return false;
    }

    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const quint32 childIndex = static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());

    const WalletKeychain keychain = sessionKeychain();
    if (!keychain.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet keychain could not be derived.");
        }
        return false;
    }

    const WalletCryptoBackend::OwnedCommitment owned =
        WalletCryptoBackend::createOwnedCommitment(keychain, childIndex, amount);
    if (!owned.success) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet output derivation failed.");
        }
        return false;
    }

    WalletOutput output;
    output.commitment = owned.commit.commitment;
    output.proof = owned.commit.proof;
    output.amount = amount;
    output.source = source;
    output.keyPath = owned.keyPath;
    output.blindingFactor = owned.blindingFactor;
    output.childIndex = owned.childIndex;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    output.pending = false;

    *outputOut = output;
    *commitOut = owned.commit;
    return true;
}

/**
 * @brief Updates a transaction entry by workflow id using a mutation callback.
 * @param workflowId Workflow identifier.
 * @param updater Mutation callback applied to the matching entry.
 */
void GrinWalletController::updateTransactionEntry(const QString &workflowId, const std::function<void (QJsonObject &)> &updater)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool updated = false;
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        if (entry.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        updater(entry);
        transactions.replace(i, entry);
        updated = true;
        break;
    }

    if (!updated) {
        return;
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    GrinWalletStorage::saveDocument(document);
    refreshStateFromStorage();
}

/**
 * @brief Extracts kernel excess from a transaction entry or its skeleton.
 * @param entry Transaction entry object.
 * @return Kernel excess hex string, or empty when unavailable.
 */
QString GrinWalletController::kernelExcessFromEntry(const QJsonObject &entry) const
{
    const QString direct = entry.value(QStringLiteral("kernel_excess")).toString();
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonObject txSkeleton = entry.value(QStringLiteral("tx_skeleton")).toObject();

    const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
    if (!kernels.isEmpty()) {
        return kernels.first().toObject().value(QStringLiteral("excess")).toString();
    }

    return QString();
}

/**
 * @brief Recomputes transaction confirmations using current chain height.
 */
void GrinWalletController::refreshTransactionConfirmations()
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletStorage::refreshTransactionConfirmations(&document, m_chainHeight)) {
        GrinWalletStorage::saveDocument(document);
    }
}

/**
 * @brief Refreshes platform storage persistence status.
 */
void GrinWalletController::refreshStoragePersistenceState()
{
    m_storagePersistenceState = GrinWalletPlatformHelpers::storagePersistenceState();
    emit storageStateChanged();
    emit statusChanged();
}

/**
 * @brief Resets the wallet session inactivity timer when unlocked.
 */
void GrinWalletController::touchWalletSession()
{
    if (!m_walletUnlocked) {
        return;
    }

    if (!m_sessionLockTimer) {
        startAutoRefresh();
    }
    if (m_sessionLockTimer) {
        m_sessionLockTimer->start();
    }
}

/**
 * @brief Requests recovery of pending broadcasts from node sync service.
 */
void GrinWalletController::recoverPendingBroadcasts()
{
    m_nodeSyncService->recoverPendingBroadcasts();
}

/**
 * @brief Rebuilds transaction history from outputs while preserving existing entries.
 * @param outputs Current wallet outputs.
 * @param existingTransactions Existing transaction entries.
 * @return Rebuilt transaction history array.
 */
QJsonArray GrinWalletController::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                                     const QJsonArray &existingTransactions) const
{
    return GrinWalletHistoryHelpers::rebuildTransactionHistoryFromOutputs(
        outputs, existingTransactions, m_chainHeight);
}

/**
 * @brief Starts refresh of broadcast statuses through node sync service.
 */
void GrinWalletController::refreshBroadcastStatuses()
{
    m_nodeSyncService->refreshBroadcastStatuses();
}

/**
 * @brief Starts processing the next queued kernel status check.
 */
void GrinWalletController::startNextKernelStatusCheck()
{
    m_nodeSyncService->startNextKernelStatusCheck();
}

/**
 * @brief Starts wallet seed scan via node sync service.
 */
void GrinWalletController::startSeedScan()
{
    m_nodeSyncService->startSeedScan();
}

/**
 * @brief Finishes an active seed scan and publishes completion message.
 * @param message Completion status message.
 */
void GrinWalletController::finishSeedScan(const QString &message)
{
    m_nodeSyncService->finishSeedScan(message);
}

/**
 * @brief Requests an output scan refresh from node sync service.
 */
void GrinWalletController::requestWalletScan()
{
    m_nodeSyncService->requestWalletScan();
}
