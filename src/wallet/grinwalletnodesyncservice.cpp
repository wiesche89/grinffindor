#include "grinwalletnodesyncservice.h"

#include <QDateTime>
#include <QtGlobal>
#include <QSet>

#include "grinwalletcontroller.h"
#include "grinwalletnodesync.h"
#include "grinwalletstorage.h"
#include "grinwalletworkflow.h"
#include "input.h"
#include "nodeforeignapi.h"
#include "outputfeatures.h"
#include "transaction.h"
#include "transactionbody.h"
#include "txkernel.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"
#include "walletscanner.h"
#include "walletselection.h"

/**
 * @brief GrinWalletNodeSyncService::GrinWalletNodeSyncService
 * @param controller
 */
GrinWalletNodeSyncService::GrinWalletNodeSyncService(GrinWalletController *controller)
    : QObject(controller)
    , m_controller(controller)
{
}

// -------------------------------------------------------------------------------------------------------
// Handling Node Responses And Wallet State Reconciliation
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletNodeSyncService::clearPendingBroadcastState
 */
void GrinWalletNodeSyncService::clearPendingBroadcastState()
{
    m_controller->setPendingBroadcastWorkflowId(QString());
    m_controller->setPendingBroadcastTxSkeleton(QJsonObject());
    m_controller->setPendingBroadcastInputCommits(QJsonArray());
}

/**
 * @brief GrinWalletNodeSyncService::failPendingBroadcast
 * @param workflowId
 * @param message
 */
void GrinWalletNodeSyncService::failPendingBroadcast(const QString &workflowId, const QString &message)
{
    clearPendingBroadcastState();
    m_controller->markTransactionBroadcastFailed(workflowId, message);
    m_controller->setLastError(message);
}


/**
 * @brief GrinWalletNodeSyncService::onNodeTipFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeTipFinished(const Result<Tip> &result)
{
    if (result.hasError()) {
        m_controller->setSyncStatusMessage(QStringLiteral("Node query failed"));
        m_controller->notifyStatusChanged();
        m_controller->setLastError(result.errorMessage());
        return;
    }

    const Tip tip = result.value();
    m_controller->setChainHeightValue(tip.height());
    m_controller->setSyncStatusMessage(QStringLiteral("Connected to external node"));
    m_controller->refreshTransactionConfirmations();
    m_controller->notifyStatusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Node tip updated to height %1.").arg(QString::number(m_controller->chainHeight())));

    if (m_controller->hasUnlockedSession()

        && !m_controller->walletScanInFlight()
        && !m_controller->seedScanActive()) {
        if (m_controller->scanHeight() == 0) {
            m_controller->rescanWallet();
        } else {
            requestWalletScan();
        }
    }

    refreshBroadcastStatuses();
    recoverPendingBroadcasts();
}

/**
 * @brief GrinWalletNodeSyncService::onNodeVersionFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeVersionFinished(const Result<NodeVersion> &result)
{
    if (result.hasError()) {
        return;
    }

    const quint64 blockHeaderVersion = result.value().blockHeaderVersion();
    if (blockHeaderVersion > 0 && blockHeaderVersion < 256) {
        m_controller->setNodeBlockHeaderVersionValue(static_cast<int>(blockHeaderVersion));
    }
}

/**
 * @brief GrinWalletNodeSyncService::onNodeOutputsFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeOutputsFinished(const Result<QList<OutputPrintable> > &result)
{
    if (result.hasError()) {
        m_controller->setSyncStatusMessage(QStringLiteral("Wallet scan failed"));
        m_controller->notifyStatusChanged();
        m_controller->setLastError(result.errorMessage());
        m_controller->setWalletScanInFlight(false);
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
    const QList<OutputPrintable> chainOutputs = result.value();

    QSet<QString> trackedCommitments;
    for (int i = 0; i < tracked.size(); ++i) {
        trackedCommitments.insert(tracked.at(i).commitment);
    }

    int matchedCommitments = 0;
    for (int i = 0; i < chainOutputs.size(); ++i) {
        const QString commitHex = chainOutputs.at(i).commit().hex();
        if (trackedCommitments.contains(commitHex)) {
            ++matchedCommitments;
        }
    }

    if (!tracked.isEmpty() && (chainOutputs.isEmpty() || matchedCommitments == 0)) {
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->setWalletScanInFlight(false);
        m_controller->setLastInfo(QStringLiteral("Node returned no tracked outputs. Falling back to seed scan from leaf 1."));
        startSeedScan();
        return;
    }

    tracked = WalletScanner::reconcileTrackedOutputs(tracked, chainOutputs);

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_controller->chainHeight()));
    walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->chainHeight()));
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("tracked-outputs"));
    walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    document.insert(QStringLiteral("wallet_state"), walletState);

    if (!m_controller->saveDocumentForService(document)) {
        m_controller->setLastError(QStringLiteral("Failed to persist wallet scan results."));
        return;
    }

    m_controller->refreshStateFromStorage();
    m_controller->setSyncStatusMessage(QStringLiteral("Wallet outputs synced"));
    m_controller->notifyStatusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Wallet scan updated %1 tracked outputs from node data.")
                                  .arg(QString::number(tracked.size())));
    m_controller->setWalletScanInFlight(false);
}

/**
 * @brief GrinWalletNodeSyncService::onNodeOutputCommitmentsFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeOutputCommitmentsFinished(const Result<QList<OutputPrintable> > &result)
{
    if (!m_controller->pendingBroadcastInputLookup()) {
        return;
    }

    m_controller->setPendingBroadcastInputLookup(false);

    const QString workflowId = m_controller->pendingBroadcastWorkflowId();
    if (workflowId.isEmpty()) {
        m_controller->setPendingBroadcastTxSkeleton(QJsonObject());
        m_controller->setPendingBroadcastInputCommits(QJsonArray());
        return;
    }

    if (result.hasError()) {
        const QString message = QStringLiteral("Pre-broadcast input lookup failed: %1").arg(result.errorMessage());
        failPendingBroadcast(workflowId, message);
        return;
    }

    QSet<QString> foundCommitments;
    QHash<QString, OutputPrintable::OutputType> outputTypes;

    const QList<OutputPrintable> foundOutputs = result.value();
    for (int i = 0; i < foundOutputs.size(); ++i) {
        const QString commitHex = foundOutputs.at(i).commit().hex();
        foundCommitments.insert(commitHex);
        outputTypes.insert(commitHex, foundOutputs.at(i).outputType());
    }

    QString missingCommit;

    const QJsonArray pendingInputCommits = m_controller->pendingBroadcastInputCommits();
    for (int i = 0; i < pendingInputCommits.size(); ++i) {
        const QString commit = pendingInputCommits.at(i).toString();
        if (!foundCommitments.contains(commit)) {
            missingCommit = commit;
            break;
        }
    }

    if (!missingCommit.isEmpty()) {
        const QString message = QStringLiteral("Node preflight rejected input commit %1. Wallet state may be stale or the output is already spent.")
                                    .arg(missingCommit);
        failPendingBroadcast(workflowId, message);
        return;
    }

    Transaction tx = Transaction::fromJson(m_controller->pendingBroadcastTxSkeleton());
    TransactionBody body = tx.body();
    QVector<Input> inputs = body.inputs();
    bool adjustedInputFeatures = false;
    for (int i = 0; i < inputs.size(); ++i) {
        Input input = inputs.at(i);
        const QString commitHex = input.commit().hex();
        const OutputFeatures::Feature expectedFeature =
            outputTypes.value(commitHex) == OutputPrintable::OutputType::OutputTypeCoinbase
                ? OutputFeatures::Coinbase
                : OutputFeatures::Plain;
        if (input.features() != expectedFeature) {
            input.setFeatures(expectedFeature);
            inputs[i] = input;
            adjustedInputFeatures = true;
        }
    }
    if (adjustedInputFeatures) {
        body.setInputs(inputs);
        tx.setBody(body);
    }

    QString validationError;
    if (!WalletCryptoBackend::validateTransactionBody(tx, &validationError)) {
        const QString message = QStringLiteral("Local pre-broadcast body validation failed: %1").arg(validationError);
        failPendingBroadcast(workflowId, message);
        return;
    }

    if (!WalletCryptoBackend::validateTransactionKernelSums(tx, &validationError)) {
        const QString message = QStringLiteral("Local pre-broadcast kernel sum/offset validation failed: %1").arg(validationError);
        failPendingBroadcast(workflowId, message);
        return;
    }

    if (!WalletCryptoBackend::validateTransactionKernelSignatures(tx, &validationError)) {
        const QString message = QStringLiteral("Local pre-broadcast kernel signature validation failed: %1").arg(validationError);
        failPendingBroadcast(workflowId, message);
        return;
    }

    m_controller->setPendingBroadcastTxSkeleton(QJsonObject());
    m_controller->setPendingBroadcastInputCommits(QJsonArray());
    m_controller->nodeApi()->pushTransactionAsync(tx, true);
}

/**
 * @brief GrinWalletNodeSyncService::onNodeUnspentOutputsFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeUnspentOutputsFinished(const Result<OutputListing> &result)
{
    if (result.hasError()) {
        m_controller->setSyncStatusMessage(QStringLiteral("Seed scan failed"));
        m_controller->notifyStatusChanged();
        m_controller->setLastError(result.errorMessage());
        m_controller->setSeedScanActive(false);
        m_controller->setWalletScanInFlight(false);
        m_controller->setFullRescanInFlight(false);
        return;
    }

    if (!m_controller->seedScanActive() || !m_controller->hasUnlockedSession()) {
        return;
    }

    WalletKeychain keychain(m_controller->sessionMnemonic());
    if (!keychain.isValid()) {
        m_controller->setLastError(QStringLiteral("Wallet keychain could not be derived for seed scan."));
        m_controller->setSeedScanActive(false);
        m_controller->setWalletScanInFlight(false);
        m_controller->setFullRescanInFlight(false);
        return;
    }

    const QList<WalletOutput> discovered = WalletScanner::discoverOwnedOutputs(result.value().outputs(), keychain);
    for (int i = 0; i < discovered.size(); ++i) {
        bool exists = false;
        const QList<WalletOutput> seedScanDiscovered = m_controller->seedScanDiscovered();
        for (int j = 0; j < seedScanDiscovered.size(); ++j) {
            if (seedScanDiscovered.at(j).commitment == discovered.at(i).commitment) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_controller->appendSeedScanDiscovered(discovered.at(i));
        }
    }

    const OutputListing listing = result.value();
    if (listing.lastRetrievedIndex() > 0 && listing.highestIndex() > 0

        && listing.lastRetrievedIndex() < listing.highestIndex()) {
        const int progressPercent = qBound(0,
                                           static_cast<int>((listing.lastRetrievedIndex() * 100) / listing.highestIndex()),
                                           99);
        m_controller->setSeedScanNextIndex(listing.lastRetrievedIndex() + 1);
        QJsonObject document = m_controller->loadDocumentForService();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(listing.lastRetrievedIndex()));
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->setSyncStatusMessage(QStringLiteral("Seed scan %1% (%2 / %3)")
                                               .arg(QString::number(progressPercent))
                                               .arg(QString::number(listing.lastRetrievedIndex()))
                                               .arg(QString::number(listing.highestIndex())));
        m_controller->notifyStatusChanged();
        m_controller->nodeApi()->getUnspentOutputsAsync(static_cast<int>(m_controller->seedScanNextIndex()), -1, 1000, true);
        return;
    }
    if (listing.lastRetrievedIndex() == 0 && discovered.size() >= 1000) {
        m_controller->setSeedScanNextIndex(m_controller->seedScanNextIndex() + 1000);
        QJsonObject document = m_controller->loadDocumentForService();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(m_controller->seedScanNextIndex() - 1));
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->setSyncStatusMessage(QStringLiteral("Seed scan 0% (starting at leaf %1)")
                               .arg(QString::number(m_controller->seedScanNextIndex())));
        m_controller->notifyStatusChanged();
        m_controller->nodeApi()->getUnspentOutputsAsync(static_cast<int>(m_controller->seedScanNextIndex()), -1, 1000, true);
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);

    const QList<WalletOutput> seedScanDiscovered = m_controller->seedScanDiscovered();
    for (int i = 0; i < seedScanDiscovered.size(); ++i) {
        bool exists = false;
        for (int j = 0; j < tracked.size(); ++j) {
            if (tracked.at(j).commitment == seedScanDiscovered.at(i).commitment) {
                WalletOutput merged = tracked.at(j);
                const WalletOutput discovered = seedScanDiscovered.at(i);
                merged.proof = discovered.proof;
                merged.amount = discovered.amount;
                merged.keyPath = discovered.keyPath;
                merged.blindingFactor = discovered.blindingFactor;
                merged.childIndex = discovered.childIndex;
                merged.height = discovered.height;
                merged.coinbase = discovered.coinbase;
                merged.onChain = discovered.onChain;
                merged.spent = discovered.spent;
                if (discovered.onChain && !discovered.spent) {
                    merged.pending = false;
                }
                tracked[j] = merged;
                exists = true;
                break;
            }
        }
        if (!exists) {
            tracked.append(seedScanDiscovered.at(i));
        }
    }

    quint32 nextChildIndex = m_controller->nextChildIndexFromStateForService(walletState);
    for (int i = 0; i < tracked.size(); ++i) {
        if (tracked.at(i).childIndex + 1 > nextChildIndex) {
            nextChildIndex = tracked.at(i).childIndex + 1;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_controller->chainHeight()));
    walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->chainHeight()));
    walletState.insert(QStringLiteral("restore_leaf_index"),
                       QString::number(listing.lastRetrievedIndex() > 0
                                           ? listing.lastRetrievedIndex()
                                           : (m_controller->seedScanNextIndex() > 0 ? m_controller->seedScanNextIndex() - 1 : 1)));
    walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(nextChildIndex));
    const QString previousSyncMode = walletState.value(QStringLiteral("last_sync_mode")).toString();

    const bool rebuildingTransactions = previousSyncMode == QStringLiteral("full-rescan");
    if (rebuildingTransactions) {
        const QJsonArray rebuiltTransactions =
            m_controller->rebuildTransactionHistoryFromOutputs(tracked, walletState.value(QStringLiteral("transaction_rescan_backup")).toArray());
        walletState.insert(QStringLiteral("transactions"), rebuiltTransactions);
        document.insert(QStringLiteral("workflow_contexts"),
                        m_controller->filterWorkflowContextsForTransactionsForService(
                            document.value(QStringLiteral("workflow_contexts")).toObject(),
                            rebuiltTransactions));
    }
    walletState.remove(QStringLiteral("transaction_rescan_backup"));
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("seed-rewind"));
    walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    document.insert(QStringLiteral("wallet_state"), walletState);

    if (!m_controller->saveDocumentForService(document)) {
        m_controller->setLastError(QStringLiteral("Failed to persist seed scan results."));
        return;
    }

    m_controller->refreshStateFromStorage();
    m_controller->setSyncStatusMessage(QStringLiteral("Seed scan 100% complete"));
    m_controller->notifyStatusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QString());
    m_controller->setSeedScanActive(false);
    m_controller->setWalletScanInFlight(false);
    m_controller->setFullRescanInFlight(false);
}

/**
 * @brief GrinWalletNodeSyncService::onNodeUnconfirmedTransactionsFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeUnconfirmedTransactionsFinished(const Result<QList<PoolEntry> > &result)
{

    m_controller->setBroadcastStatusRefreshInFlight(false);
    if (result.hasError()) {
        return;
    }

    QSet<QString> mempoolExcesses;

    const QList<PoolEntry> entries = result.value();
    for (int i = 0; i < entries.size(); ++i) {
        const QVector<TxKernel> kernels = entries.at(i).tx().body().kernels();
        for (int j = 0; j < kernels.size(); ++j) {
            if (!kernels.at(j).excess().isEmpty()) {
                mempoolExcesses.insert(kernels.at(j).excess());
            }
        }
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();

    m_controller->clearKernelStatusQueue();

    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        if (!entry.value(QStringLiteral("broadcasted")).toBool()
            && status != QStringLiteral("broadcast_pending")) {
            continue;
        }
        if (status == QStringLiteral("confirmed") || status == QStringLiteral("cancelled")) {
            continue;
        }

        const QString excess = m_controller->kernelExcessFromEntry(entry);
        if (excess.isEmpty()) {
            continue;
        }

        entry.insert(QStringLiteral("kernel_excess"), excess);
        entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        entry.insert(QStringLiteral("broadcasted"),
                     entry.value(QStringLiteral("broadcasted")).toBool()
                         || mempoolExcesses.contains(excess));
        entry.insert(QStringLiteral("status"),
                     mempoolExcesses.contains(excess)
                         ? QStringLiteral("in_mempool")
                         : (status == QStringLiteral("broadcast_pending")
                               ? QStringLiteral("broadcast_pending")
                               : QStringLiteral("broadcasted")));
        entry.insert(QStringLiteral("confirmations"), 0);
        transactions.replace(i, entry);
        m_controller->appendKernelStatusCheck(entry.value(QStringLiteral("workflow_id")).toString(), excess);
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();
    startNextKernelStatusCheck();
}

/**
 * @brief GrinWalletNodeSyncService::onNodeKernelFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodeKernelFinished(const Result<LocatedTxKernel> &result)
{

    m_controller->setKernelStatusCheckInFlight(false);
    if (!m_controller->currentKernelWorkflowIdInternal().isEmpty()) {
        if (!result.hasError()) {
            m_controller->markTransactionKernelConfirmed(m_controller->currentKernelWorkflowIdInternal(), result.value().height());
            m_controller->finalizeBroadcastedWorkflow(m_controller->currentKernelWorkflowIdInternal());
        } else if (result.errorMessage() == QStringLiteral("NotFound")) {
            m_controller->markTransactionKernelBroadcasted(m_controller->currentKernelWorkflowIdInternal());
            m_controller->setLastError(QString());
        }
    }

    m_controller->clearCurrentKernelCheck();
    startNextKernelStatusCheck();
}

/**
 * @brief GrinWalletNodeSyncService::onNodePushTransactionFinished
 * @param result
 */
void GrinWalletNodeSyncService::onNodePushTransactionFinished(const Result<bool> &result)
{
    const QString workflowId = m_controller->pendingBroadcastWorkflowId();

    m_controller->setPendingBroadcastWorkflowId(QString());

    if (result.hasError() || !result.value()) {
        const QString message = result.hasError()
            ? result.errorMessage()
            : QStringLiteral("Node rejected transaction broadcast.");
        if (!workflowId.isEmpty()) {
            m_controller->markTransactionBroadcastRejected(workflowId, message);
        }
        m_controller->setLastError(message);
        return;
    }

    if (!workflowId.isEmpty()) {
        m_controller->markTransactionBroadcastSucceeded(workflowId);
        m_controller->finalizeBroadcastedWorkflow(workflowId);
    }

    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Transaction broadcast submitted to node."));
    refreshBroadcastStatuses();
}
