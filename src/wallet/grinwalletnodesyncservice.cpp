#include "grinwalletnodesyncservice.h"

#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QTimer>
#include <QtGlobal>
#include <QSet>
#include <memory>

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

namespace {

QHash<QString, int> buildTrackedOutputIndex(const QList<WalletOutput> &tracked)
{
    QHash<QString, int> trackedIndexByCommitment;
    trackedIndexByCommitment.reserve(tracked.size());
    for (int index = 0; index < tracked.size(); ++index) {
        trackedIndexByCommitment.insert(tracked.at(index).commitment, index);
    }
    return trackedIndexByCommitment;
}

void mergeDiscoveredOutput(QList<WalletOutput> &tracked,
                          QHash<QString, int> &trackedIndexByCommitment,
                          QSet<QString> &discoveredCommitmentsInScan,
                          const WalletOutput &discovered)
{
    discoveredCommitmentsInScan.insert(discovered.commitment);

    const auto existing = trackedIndexByCommitment.constFind(discovered.commitment);
    if (existing != trackedIndexByCommitment.constEnd()) {
        const int index = existing.value();

        WalletOutput merged = tracked.at(index);
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
        tracked[index] = merged;
        return;
    }

    trackedIndexByCommitment.insert(discovered.commitment, tracked.size());
    tracked.append(discovered);
}

}

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

bool GrinWalletNodeSyncService::ensureFullRescanSession(QString *errorMessage)
{
    if (m_fullRescanSession) {
        return true;
    }

    const QJsonObject document = m_controller->loadDocumentForService();
    const QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();

    std::shared_ptr<FullRescanSessionState> session = std::make_shared<FullRescanSessionState>();
    session->document = document;
    session->walletState = walletState;
    session->tracked = WalletScanner::outputsFromState(walletState);
    session->trackedIndexByCommitment = buildTrackedOutputIndex(session->tracked);
    session->keychain = m_controller->sessionKeychain();
    session->nextChildIndex = m_controller->nextChildIndexFromStateForService(walletState);
    session->transactionRescanBackup = walletState.value(QStringLiteral("transaction_rescan_backup")).toArray();
    session->fullRescanMode = walletState.value(QStringLiteral("last_sync_mode")).toString() == QStringLiteral("full-rescan")
        || m_controller->fullRescanInFlight();
    session->rebuildTransactions = walletState.value(QStringLiteral("last_sync_mode")).toString() == QStringLiteral("full-rescan");

    if (!session->keychain.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Wallet keychain could not be derived for full rescan.");
        }
        return false;
    }

    m_fullRescanSession = session;
    return true;
}

void GrinWalletNodeSyncService::clearFullRescanSession()
{
    m_fullRescanSession.reset();
}

bool GrinWalletNodeSyncService::persistFullRescanSession(quint64 restoreLeafIndex,
                                                         bool completed,
                                                         QString *errorMessage)
{
    if (!m_fullRescanSession) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Full rescan session is not initialized.");
        }
        return false;
    }

    FullRescanSessionState &session = *m_fullRescanSession;
    session.walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(session.tracked));
    session.walletState.insert(QStringLiteral("balances"),
                               WalletScanner::balancesFromOutputs(session.tracked, m_controller->chainHeight(), static_cast<qulonglong>(m_controller->minimumConfirmations())));
    session.walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->chainHeight()));
    session.walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(restoreLeafIndex));
    session.walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(session.nextChildIndex));

    if (completed) {
        // Detect locked inputs that were consumed by a confirmed transaction:
        // any tracked output with locked=true that was NOT found in the UTXO set
        // during this scan has been spent on-chain.
        for (int i = 0; i < session.tracked.size(); ++i) {
            const WalletOutput &out = session.tracked.at(i);
            if (out.locked && !out.spent
                && !session.discoveredCommitmentsInScan.contains(out.commitment)) {
                session.tracked[i].spent = true;
                session.tracked[i].locked = false;
                session.tracked[i].pending = false;
                session.tracked[i].onChain = false;
            }
        }

        // Recompute outputs and balances after spent-detection pass.
        session.walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(session.tracked));
        session.walletState.insert(QStringLiteral("balances"),
                                   WalletScanner::balancesFromOutputs(session.tracked, m_controller->chainHeight(), static_cast<qulonglong>(m_controller->minimumConfirmations())));

        if (session.rebuildTransactions) {
            const QJsonArray rebuiltTransactions =
                m_controller->rebuildTransactionHistoryFromOutputs(session.tracked, session.transactionRescanBackup);
            session.walletState.insert(QStringLiteral("transactions"), rebuiltTransactions);
            session.document.insert(QStringLiteral("workflow_contexts"),
                                    m_controller->filterWorkflowContextsForTransactionsForService(
                                        session.document.value(QStringLiteral("workflow_contexts")).toObject(),
                                        rebuiltTransactions));
        }

        session.walletState.remove(QStringLiteral("transaction_rescan_backup"));
        session.walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("seed-rewind"));
        session.walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    } else {
        if (session.fullRescanMode && session.rebuildTransactions) {
            session.walletState.insert(QStringLiteral("transaction_rescan_backup"), session.transactionRescanBackup);
        }
        if (session.fullRescanMode) {
            session.walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("full-rescan"));
            session.walletState.insert(QStringLiteral("last_synced_at"), QString());
        }
    }

    const QJsonObject walletStateToPersist = session.walletState;
    const QJsonObject workflowContextsToPersist = session.document.value(QStringLiteral("workflow_contexts")).toObject();
    if (!m_controller->updateDocumentForService([&walletStateToPersist, &workflowContextsToPersist](QJsonObject *document) {
            if (!document) {
                return false;
            }
            document->insert(QStringLiteral("wallet_state"), walletStateToPersist);
            document->insert(QStringLiteral("workflow_contexts"), workflowContextsToPersist);
            return true;
        })) {
        if (errorMessage != nullptr) {
            *errorMessage = completed
                ? QStringLiteral("Failed to persist full-rescan results.")
                : QStringLiteral("Failed to persist full-rescan checkpoint.");
        }
        return false;
    }

    session.batchesSinceCheckpoint = 0;
    return true;
}

void GrinWalletNodeSyncService::abortFullRescan(const QString &message)
{
    const bool fullRescanMode = m_fullRescanSession && m_fullRescanSession->fullRescanMode;
    clearFullRescanSession();
    m_controller->setSyncStatusMessage(fullRescanMode ? QStringLiteral("Full rescan failed")
                                                     : QStringLiteral("Seed scan failed"));
    m_controller->notifyStatusChanged();
    m_controller->setLastError(message);
    m_controller->setSeedScanActive(false);
    m_controller->setWalletScanInFlight(false);
    m_controller->setFullRescanInFlight(false);
}

void GrinWalletNodeSyncService::requestNextFullRescanBatch()
{
    if (!m_controller->nodeApi() || !m_controller->seedScanActive()) {
        return;
    }

    m_controller->touchWalletSession();

    if (!m_controller->hasUnlockedSession()) {
        abortFullRescan(QStringLiteral("Unlock the wallet before continuing the full rescan."));
        return;
    }

    QString sessionError;
    if (!ensureFullRescanSession(&sessionError)) {
        abortFullRescan(sessionError);
        return;
    }

    const std::shared_ptr<FullRescanSessionState> session = m_fullRescanSession;

    m_controller->nodeApi()->getUnspentOutputsForRescanAsync(
        static_cast<int>(m_controller->seedScanNextIndex()),
        -1,
        kFullRescanBatchSize,
        kFullRescanIncludeProof,
        [session](const NodeForeignApi::RescanOutput &output) -> QString {
            if (output.proof.isEmpty()) {
                return QStringLiteral("Full rescan requires output proofs for rewind.");
            }

            WalletOutput discovered;
            if (!WalletScanner::discoverOwnedOutput(output.commitment,
                                                    output.proof,
                                                    output.blockHeight,
                                                    output.spent,
                                                    output.coinbase,
                                                    session->keychain,
                                                    &discovered)) {
                return QString();
            }

            mergeDiscoveredOutput(session->tracked, session->trackedIndexByCommitment, session->discoveredCommitmentsInScan, discovered);
            if (discovered.childIndex + 1 > session->nextChildIndex) {
                session->nextChildIndex = discovered.childIndex + 1;
            }

            return QString();
        },
        [this, session](const Result<NodeForeignApi::RescanBatchProgress> &result) {
            if (m_fullRescanSession != session) {
                return;
            }

            m_controller->touchWalletSession();

            if (result.hasError()) {
                abortFullRescan(result.errorMessage());
                return;
            }

            const NodeForeignApi::RescanBatchProgress progress = result.value();
            const bool hasMore = progress.lastRetrievedIndex > 0
                && progress.highestIndex > 0
                && progress.lastRetrievedIndex < progress.highestIndex;
            const bool moreWithoutIndices = progress.lastRetrievedIndex == 0 && progress.outputsProcessed >= kFullRescanBatchSize;
            session->batchesSinceCheckpoint += 1;

            if (hasMore) {
                m_controller->setSeedScanNextIndex(progress.lastRetrievedIndex + 1);
            } else if (moreWithoutIndices) {
                m_controller->setSeedScanNextIndex(m_controller->seedScanNextIndex() + kFullRescanBatchSize);
            }

            const quint64 restoreLeafIndex = progress.lastRetrievedIndex > 0
                ? progress.lastRetrievedIndex
                : (m_controller->seedScanNextIndex() > 0 ? m_controller->seedScanNextIndex() - 1 : 1);

            if (hasMore || moreWithoutIndices) {
                const int progressPercent = progress.highestIndex > 0
                    ? qBound(0,
                             static_cast<int>((restoreLeafIndex * 100) / progress.highestIndex),
                             99)
                    : 0;

                if (session->batchesSinceCheckpoint >= kFullRescanCheckpointInterval) {
                    QString persistError;
                    if (!persistFullRescanSession(restoreLeafIndex, false, &persistError)) {
                        abortFullRescan(persistError);
                        return;
                    }
                }

                m_controller->setSyncStatusMessage(QStringLiteral("%1 %2% (%3 / %4)")
                                                       .arg(session->fullRescanMode ? QStringLiteral("Full rescan")
                                                                                   : QStringLiteral("Seed scan"))
                                                       .arg(QString::number(progressPercent))
                                                       .arg(QString::number(restoreLeafIndex))
                                                       .arg(QString::number(progress.highestIndex)));
                m_controller->notifyStatusChanged();

                QTimer::singleShot(0, this, [this]() {
                    requestNextFullRescanBatch();
                });
                return;
            }

            QString persistError;
            if (!persistFullRescanSession(restoreLeafIndex, true, &persistError)) {
                abortFullRescan(persistError);
                return;
            }

            clearFullRescanSession();
            m_controller->refreshStateFromStorage();
            m_controller->setSyncStatusMessage(session->fullRescanMode
                                                   ? QStringLiteral("Full rescan 100% complete")
                                                   : QStringLiteral("Seed scan 100% complete"));
            m_controller->notifyStatusChanged();
            m_controller->setLastError(QString());
            m_controller->setLastInfo(QString());
            m_controller->setSeedScanActive(false);
            m_controller->setWalletScanInFlight(false);
            m_controller->setFullRescanInFlight(false);
        });
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
        if (m_controller->resumePendingFullRescan()) {
            // Resume takes precedence over starting a fresh rescan because it preserves leaf progress.
            qInfo().noquote() << QStringLiteral("Auto-sync decision: resume full rescan from persisted state.");
        } else if (m_controller->scanHeight() == 0 && m_controller->chainHeight() > 0) {
            qInfo().noquote() << QStringLiteral("Auto-sync decision: start initial full rescan because scan height is zero.");
            m_controller->rescanWallet();
        } else if (m_controller->scanHeight() < m_controller->chainHeight()) {
            qInfo().noquote() << QStringLiteral("Auto-sync decision: start incremental wallet scan because wallet height lags node height.");
            requestWalletScan();
        } else {
            qInfo().noquote() << QStringLiteral("Auto-sync decision: wallet already up to date; no automatic scan started.");
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

    QJsonObject walletState = m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject();
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
        m_controller->setWalletScanInFlight(false);
        m_controller->setLastInfo(QStringLiteral("Node returned no tracked outputs. Falling back to seed scan from stored leaf progress."));
        startSeedScan();
        return;
    }

    tracked = WalletScanner::reconcileTrackedOutputs(tracked, chainOutputs);

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_controller->chainHeight(), static_cast<qulonglong>(m_controller->minimumConfirmations())));
    walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->chainHeight()));
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("tracked-outputs"));
    walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!m_controller->updateDocumentForService([&walletState](QJsonObject *document) {
            if (!document) {
                return false;
            }
            document->insert(QStringLiteral("wallet_state"), walletState);
            return true;
        })) {
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

    QJsonObject walletState = m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject();
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
    m_controller->updateDocumentForService([&walletState](QJsonObject *document) {
        if (!document) {
            return false;
        }
        document->insert(QStringLiteral("wallet_state"), walletState);
        return true;
    });
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
