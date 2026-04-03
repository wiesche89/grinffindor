#include "grinwalletcontroller.h"

#include <QGuiApplication>
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
 * @brief GrinWalletController::resolvedNetworkName
 * @return
 */
QString GrinWalletController::resolvedNetworkName() const
{
    return GrinWalletControllerHelpers::isAcceptedNetworkName(m_selectedNetwork) ? m_selectedNetwork : GrinWalletControllerHelpers::defaultNetworkName();
}

/**
 * @brief GrinWalletController::setLastError
 * @param error
 */
void GrinWalletController::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

/**
 * @brief GrinWalletController::setLastInfo
 * @param info
 */
void GrinWalletController::setLastInfo(const QString &info)
{
    m_lastInfo = info;
    emit lastInfoChanged();
}

/**
 * @brief GrinWalletController::setWorkflow
 * @param id
 * @param mode
 * @param state
 * @param slatepack
 * @param decoded
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
 * @brief GrinWalletController::finalizeTransactionStoreUpdate
 * @param document
 * @param changed
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
 * @brief GrinWalletController::storeOutputsState
 * @param document
 * @param walletState
 * @param outputs
 * @param nextChildIndex
 */
void GrinWalletController::storeOutputsState(QJsonObject *document,
                                             QJsonObject *walletState,
                                             const QList<WalletOutput> &outputs,
                                             quint32 nextChildIndex) const
{
    if (!document || !walletState) {
        return;
    }

    walletState->insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));

    walletState->insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    if (nextChildIndex > static_cast<quint32>(walletState->value(QStringLiteral("next_child_index")).toInt())) {
        walletState->insert(QStringLiteral("next_child_index"), static_cast<int>(nextChildIndex));
    }
    document->insert(QStringLiteral("wallet_state"), *walletState);
}

/**
 * @brief GrinWalletController::transactionEntryLessThan
 * @param left
 * @param right
 * @return
 */
bool GrinWalletController::transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right)
{
    return GrinWalletHistoryHelpers::transactionEntryLessThan(left, right);
}

/**
 * @brief GrinWalletController::walletOutputLessThan
 * @param left
 * @param right
 * @return
 */
bool GrinWalletController::walletOutputLessThan(const WalletOutput &left, const WalletOutput &right)
{
    if (left.spent != right.spent) {
        return !left.spent && right.spent;
    }
    if (left.locked != right.locked) {
        return left.locked && !right.locked;
    }
    if (left.pending != right.pending) {
        return left.pending && !right.pending;
    }
    if (left.onChain != right.onChain) {
        return left.onChain && !right.onChain;
    }
    if (left.height != right.height) {
        return left.height > right.height;
    }
    return left.commitment < right.commitment;
}

/**
 * @brief GrinWalletController::markTransactionBroadcastPending
 * @param workflowId
 */
void GrinWalletController::markTransactionBroadcastPending(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastPending(&document, workflowId));
}

/**
 * @brief GrinWalletController::markTransactionBroadcastFailed
 * @param workflowId
 * @param message
 */
void GrinWalletController::markTransactionBroadcastFailed(const QString &workflowId, const QString &message)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastFailed(&document, workflowId, message));
}

/**
 * @brief GrinWalletController::markTransactionKernelConfirmed
 * @param workflowId
 * @param confirmedHeight
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
 * @brief GrinWalletController::markTransactionKernelBroadcasted
 * @param workflowId
 */
void GrinWalletController::markTransactionKernelBroadcasted(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markKernelBroadcasted(&document, workflowId));
}

/**
 * @brief GrinWalletController::markTransactionBroadcastRejected
 * @param workflowId
 * @param message
 */
void GrinWalletController::markTransactionBroadcastRejected(const QString &workflowId, const QString &message)
{
    markTransactionBroadcastFailed(workflowId, message);
}

/**
 * @brief GrinWalletController::markTransactionBroadcastSucceeded
 * @param workflowId
 */
void GrinWalletController::markTransactionBroadcastSucceeded(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    finalizeTransactionStoreUpdate(document, GrinWalletTransactionStore::markBroadcastSucceeded(&document, workflowId));
}

/**
 * @brief GrinWalletController::connectNodeClient
 */
void GrinWalletController::connectNodeClient()
{
    m_nodeSyncService->connectNodeClient();
}

/**
 * @brief GrinWalletController::refreshStateFromStorage
 */
void GrinWalletController::refreshStateFromStorage()
{

    GrinWalletStorage::RefreshedState state = GrinWalletStorage::refreshState(GrinWalletStorage::loadDocument(), m_chainHeight);
    if (state.balancesChanged) {
        GrinWalletStorage::saveDocument(state.document);
    }

    m_scanHeight = state.scanHeight;
    m_totalBalance = state.totalBalance;
    m_spendableBalance = state.spendableBalance;
    m_lockedBalance = state.lockedBalance;
    m_immatureBalance = state.immatureBalance;
    m_awaitingConfirmationBalance = state.awaitingConfirmationBalance;
    m_awaitingFinalizationBalance = state.awaitingFinalizationBalance;
    emit statusChanged();
}

/**
 * @brief GrinWalletController::startAutoRefresh
 */
void GrinWalletController::startAutoRefresh()
{
    if (m_autoRefreshTimer) {
        return;
    }

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(30000);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &GrinWalletController::refreshNodeStatus);

    m_autoRefreshTimer->start();

    if (!m_sessionLockTimer) {
        m_sessionLockTimer = new QTimer(this);
        m_sessionLockTimer->setInterval(kSessionAutoLockIntervalMs);
        m_sessionLockTimer->setSingleShot(true);
        connect(m_sessionLockTimer, &QTimer::timeout, this, &GrinWalletController::onSessionLockTimeout);
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this, &GrinWalletController::onApplicationStateChanged);
}

/**
 * @brief GrinWalletController::onSessionLockTimeout
 */
void GrinWalletController::onSessionLockTimeout()
{
    if (!m_walletUnlocked) {
        return;
    }

    lockWallet();
    setLastInfo(QStringLiteral("Wallet locked after inactivity."));
}

/**
 * @brief GrinWalletController::onApplicationStateChanged
 * @param state
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
        lockWallet();
        setLastInfo(QStringLiteral("Wallet locked because the browser tab or app became inactive."));
    }
}

/**
 * @brief GrinWalletController::loadDocumentForService
 * @return
 */
QJsonObject GrinWalletController::loadDocumentForService() const
{
    return GrinWalletStorage::loadDocument();
}

/**
 * @brief GrinWalletController::saveDocumentForService
 * @param document
 * @return
 */
bool GrinWalletController::saveDocumentForService(const QJsonObject &document) const
{
    return GrinWalletStorage::saveDocument(document);
}

/**
 * @brief GrinWalletController::nextChildIndexFromStateForService
 * @param walletState
 * @return
 */
quint32 GrinWalletController::nextChildIndexFromStateForService(const QJsonObject &walletState) const
{
    return static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
}

/**
 * @brief GrinWalletController::filterWorkflowContextsForTransactionsForService
 * @param contexts
 * @param transactions
 * @return
 */
QJsonObject GrinWalletController::filterWorkflowContextsForTransactionsForService(const QJsonObject &contexts,
                                                                                  const QJsonArray &transactions) const
{
    return GrinWalletControllerHelpers::filterWorkflowContextsForTransactions(contexts, transactions);
}

/**
 * @brief GrinWalletController::storeOwnedOutput
 * @param source
 * @param amount
 * @param commit
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
 * @brief GrinWalletController::storeOwnedOutput
 * @param output
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
 * @brief GrinWalletController::buildOwnedOutput
 * @param source
 * @param amount
 * @param outputOut
 * @param commitOut
 * @param errorOut
 * @return
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
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet must be unlocked to derive owned outputs.");
        }
        return false;
    }

    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const quint32 childIndex = static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());

    const WalletKeychain keychain(m_sessionMnemonic);
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
 * @brief GrinWalletController::updateTransactionEntry
 * @param workflowId
 * @param updater
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
 * @brief GrinWalletController::kernelExcessFromEntry
 * @param entry
 * @return
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
 * @brief GrinWalletController::refreshTransactionConfirmations
 */
void GrinWalletController::refreshTransactionConfirmations()
{

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletStorage::refreshTransactionConfirmations(&document, m_chainHeight)) {
        GrinWalletStorage::saveDocument(document);
    }
}

/**
 * @brief GrinWalletController::refreshStoragePersistenceState
 */
void GrinWalletController::refreshStoragePersistenceState()
{
    m_storagePersistenceState = GrinWalletPlatformHelpers::storagePersistenceState();
    emit statusChanged();
}

/**
 * @brief GrinWalletController::touchWalletSession
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
 * @brief GrinWalletController::recoverPendingBroadcasts
 */
void GrinWalletController::recoverPendingBroadcasts()
{
    m_nodeSyncService->recoverPendingBroadcasts();
}

/**
 * @brief GrinWalletController::rebuildTransactionHistoryFromOutputs
 * @param outputs
 * @param existingTransactions
 * @return
 */
QJsonArray GrinWalletController::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                                     const QJsonArray &existingTransactions) const
{
    return GrinWalletHistoryHelpers::rebuildTransactionHistoryFromOutputs(
        outputs, existingTransactions, m_chainHeight);
}

/**
 * @brief GrinWalletController::refreshBroadcastStatuses
 */
void GrinWalletController::refreshBroadcastStatuses()
{
    m_nodeSyncService->refreshBroadcastStatuses();
}

/**
 * @brief GrinWalletController::startNextKernelStatusCheck
 */
void GrinWalletController::startNextKernelStatusCheck()
{
    m_nodeSyncService->startNextKernelStatusCheck();
}

/**
 * @brief GrinWalletController::startSeedScan
 */
void GrinWalletController::startSeedScan()
{
    m_nodeSyncService->startSeedScan();
}

/**
 * @brief GrinWalletController::finishSeedScan
 * @param message
 */
void GrinWalletController::finishSeedScan(const QString &message)
{
    m_nodeSyncService->finishSeedScan(message);
}

/**
 * @brief GrinWalletController::requestWalletScan
 */
void GrinWalletController::requestWalletScan()
{
    m_nodeSyncService->requestWalletScan();
}
