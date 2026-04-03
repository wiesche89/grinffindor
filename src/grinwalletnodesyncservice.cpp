#include "grinwalletnodesyncservice.h"

#include <QDateTime>
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
#include "wallet/walletcryptobackend.h"
#include "wallet/walletkeychain.h"
#include "wallet/walletscanner.h"
#include "wallet/walletselection.h"

GrinWalletNodeSyncService::GrinWalletNodeSyncService(GrinWalletController *controller)
    : QObject(controller)
    , m_controller(controller)
{
}

void GrinWalletNodeSyncService::connectNodeClient()
{
    if (m_controller->m_nodeApi) {
        m_controller->m_nodeApi->deleteLater();
        m_controller->m_nodeApi = 0;
    }
    if (m_controller->m_nodeUrl.trimmed().isEmpty()) {
        return;
    }

    m_controller->m_nodeApi = new NodeForeignApi(m_controller->m_nodeUrl, QString());
    m_controller->m_nodeApi->setParent(this);

    connect(m_controller->m_nodeApi, &NodeForeignApi::getTipFinished, this, &GrinWalletNodeSyncService::onNodeTipFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getVersionFinished, this, &GrinWalletNodeSyncService::onNodeVersionFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getOutputsFinished, this, &GrinWalletNodeSyncService::onNodeOutputsFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getOutputCommitmentsFinished, this, &GrinWalletNodeSyncService::onNodeOutputCommitmentsFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getUnspentOutputsFinished, this, &GrinWalletNodeSyncService::onNodeUnspentOutputsFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getUnconfirmedTransactionsFinished, this, &GrinWalletNodeSyncService::onNodeUnconfirmedTransactionsFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::getKernelFinished, this, &GrinWalletNodeSyncService::onNodeKernelFinished);
    connect(m_controller->m_nodeApi, &NodeForeignApi::pushTransactionFinished, this, &GrinWalletNodeSyncService::onNodePushTransactionFinished);
}

void GrinWalletNodeSyncService::requestWalletScan()
{
    if (!m_controller->m_walletUnlocked || m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        m_controller->setLastError(QStringLiteral("Unlock the wallet before scanning outputs."));
        return;
    }

    if (m_controller->m_walletScanInFlight || m_controller->m_seedScanActive) {
        m_controller->setLastInfo(QStringLiteral("Wallet scan is already running."));
        return;
    }

    if (!m_controller->m_nodeApi) {
        connectNodeClient();
    }
    if (!m_controller->m_nodeApi) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (outputs.isEmpty()) {
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->m_chainHeight));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->refreshStateFromStorage();
        m_controller->setLastInfo(QStringLiteral("Wallet has no tracked outputs yet. Starting seed scan."));
        m_controller->m_syncStatus = QStringLiteral("Scanning wallet outputs...");
        emit m_controller->statusChanged();
        m_controller->m_walletScanInFlight = true;
        startSeedScan();
        return;
    }

    int unspentOnChainCount = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        if (!outputs.at(i).spent && outputs.at(i).onChain) {
            ++unspentOnChainCount;
        }
    }

    if (unspentOnChainCount == 0) {
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->setLastInfo(QStringLiteral("All tracked outputs are currently off-chain. Restarting seed scan from leaf 1."));
    } else {
        m_controller->setLastInfo(QStringLiteral("Refreshing tracked outputs via seed scan."));
    }

    m_controller->m_syncStatus = QStringLiteral("Scanning wallet outputs...");
    emit m_controller->statusChanged();
    m_controller->m_walletScanInFlight = true;
    startSeedScan();
}

void GrinWalletNodeSyncService::startSeedScan()
{
    m_controller->m_seedScanActive = true;
    const GrinWalletNodeSync::SeedScanState seedScanState =
        GrinWalletNodeSync::buildSeedScanState(m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject());
    m_controller->m_seedScanNextIndex = seedScanState.nextIndex;
    m_controller->m_seedScanDiscovered.clear();
    m_controller->m_syncStatus = seedScanState.syncStatus;
    emit m_controller->statusChanged();
    m_controller->m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_controller->m_seedScanNextIndex), -1, 1000, true);
}

void GrinWalletNodeSyncService::finishSeedScan(const QString &message)
{
    m_controller->m_seedScanActive = false;
    if (!message.isEmpty()) {
        m_controller->setLastInfo(message);
    }
}

void GrinWalletNodeSyncService::recoverPendingBroadcasts()
{
    if (GrinWalletNodeSync::hasRecoverableBroadcasts(m_controller->loadDocumentForService())) {
        refreshBroadcastStatuses();
    }
}

void GrinWalletNodeSyncService::refreshBroadcastStatuses()
{
    if (!m_controller->m_nodeApi || m_controller->m_broadcastStatusRefreshInFlight || m_controller->m_kernelStatusCheckInFlight) {
        return;
    }

    if (!GrinWalletNodeSync::shouldRefreshBroadcastStatuses(m_controller->loadDocumentForService())) {
        return;
    }

    m_controller->m_broadcastStatusRefreshInFlight = true;
    m_controller->m_nodeApi->getUnconfirmedTransactionsAsync();
}

void GrinWalletNodeSyncService::startNextKernelStatusCheck()
{
    if (!m_controller->m_nodeApi || m_controller->m_kernelStatusCheckInFlight || m_controller->m_kernelStatusQueue.isEmpty()) {
        return;
    }

    const QPair<QString, QString> next = m_controller->m_kernelStatusQueue.takeFirst();
    m_controller->m_currentKernelWorkflowId = next.first;
    m_controller->m_currentKernelExcess = next.second;
    if (m_controller->m_currentKernelExcess.isEmpty()) {
        startNextKernelStatusCheck();
        return;
    }

    m_controller->m_kernelStatusCheckInFlight = true;
    m_controller->m_nodeApi->getKernelAsync(m_controller->m_currentKernelExcess,
                                            0,
                                            static_cast<int>(m_controller->m_chainHeight > 0 ? m_controller->m_chainHeight + 2 : 0));
}

void GrinWalletNodeSyncService::beginBroadcastWithInputPreflight(const QString &workflowId,
                                                                 const QJsonObject &txSkeleton)
{
    if (!m_controller->m_nodeApi) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    const Transaction tx = Transaction::fromJson(txSkeleton);
    const QVector<Input> inputs = tx.body().inputs();
    if (inputs.isEmpty()) {
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->markTransactionBroadcastFailed(workflowId, QStringLiteral("Transaction has no inputs."));
        m_controller->setLastError(QStringLiteral("Transaction has no inputs."));
        return;
    }

    QJsonArray commits;
    for (int i = 0; i < inputs.size(); ++i) {
        commits.append(inputs.at(i).commit().hex());
    }

    m_controller->m_pendingBroadcastInputLookup = true;
    m_controller->m_pendingBroadcastTxSkeleton = txSkeleton;
    m_controller->m_pendingBroadcastInputCommits = commits;
    m_controller->m_nodeApi->getOutputCommitmentsAsync(commits);
}

void GrinWalletNodeSyncService::onNodeTipFinished(const Result<Tip> &result)
{
    if (result.hasError()) {
        m_controller->m_syncStatus = QStringLiteral("Node query failed");
        emit m_controller->statusChanged();
        m_controller->setLastError(result.errorMessage());
        return;
    }

    const Tip tip = result.value();
    m_controller->m_chainHeight = tip.height();
    m_controller->m_syncStatus = QStringLiteral("Connected to external node");
    m_controller->refreshTransactionConfirmations();
    emit m_controller->statusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Node tip updated to height %1.").arg(QString::number(m_controller->m_chainHeight)));

    if (m_controller->m_walletUnlocked
        && !m_controller->m_sessionMnemonic.trimmed().isEmpty()
        && !m_controller->m_walletScanInFlight
        && !m_controller->m_seedScanActive) {
        if (m_controller->m_scanHeight == 0) {
            m_controller->rescanWallet();
        } else {
            requestWalletScan();
        }
    }

    refreshBroadcastStatuses();
    recoverPendingBroadcasts();
}

void GrinWalletNodeSyncService::onNodeVersionFinished(const Result<NodeVersion> &result)
{
    if (result.hasError()) {
        return;
    }

    const quint64 blockHeaderVersion = result.value().blockHeaderVersion();
    if (blockHeaderVersion > 0 && blockHeaderVersion < 256) {
        m_controller->m_nodeBlockHeaderVersion = static_cast<int>(blockHeaderVersion);
    }
}

void GrinWalletNodeSyncService::onNodeOutputsFinished(const Result<QList<OutputPrintable> > &result)
{
    if (result.hasError()) {
        m_controller->m_syncStatus = QStringLiteral("Wallet scan failed");
        emit m_controller->statusChanged();
        m_controller->setLastError(result.errorMessage());
        m_controller->m_walletScanInFlight = false;
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
        m_controller->m_walletScanInFlight = false;
        m_controller->setLastInfo(QStringLiteral("Node returned no tracked outputs. Falling back to seed scan from leaf 1."));
        startSeedScan();
        return;
    }

    tracked = WalletScanner::reconcileTrackedOutputs(tracked, chainOutputs);

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("tracked-outputs"));
    walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    document.insert(QStringLiteral("wallet_state"), walletState);

    if (!m_controller->saveDocumentForService(document)) {
        m_controller->setLastError(QStringLiteral("Failed to persist wallet scan results."));
        return;
    }

    m_controller->refreshStateFromStorage();
    m_controller->m_syncStatus = QStringLiteral("Wallet outputs synced");
    emit m_controller->statusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Wallet scan updated %1 tracked outputs from node data.")
                                  .arg(QString::number(tracked.size())));
    m_controller->m_walletScanInFlight = false;
}

void GrinWalletNodeSyncService::onNodeOutputCommitmentsFinished(const Result<QList<OutputPrintable> > &result)
{
    if (!m_controller->m_pendingBroadcastInputLookup) {
        return;
    }

    m_controller->m_pendingBroadcastInputLookup = false;
    const QString workflowId = m_controller->m_pendingBroadcastWorkflowId;
    if (workflowId.isEmpty()) {
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        return;
    }

    if (result.hasError()) {
        const QString message = QStringLiteral("Pre-broadcast input lookup failed: %1").arg(result.errorMessage());
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        m_controller->markTransactionBroadcastFailed(workflowId, message);
        m_controller->setLastError(message);
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
    for (int i = 0; i < m_controller->m_pendingBroadcastInputCommits.size(); ++i) {
        const QString commit = m_controller->m_pendingBroadcastInputCommits.at(i).toString();
        if (!foundCommitments.contains(commit)) {
            missingCommit = commit;
            break;
        }
    }

    if (!missingCommit.isEmpty()) {
        const QString message = QStringLiteral("Node preflight rejected input commit %1. Wallet state may be stale or the output is already spent.")
                                    .arg(missingCommit);
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        m_controller->markTransactionBroadcastFailed(workflowId, message);
        m_controller->setLastError(message);
        return;
    }

    Transaction tx = Transaction::fromJson(m_controller->m_pendingBroadcastTxSkeleton);
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
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        m_controller->markTransactionBroadcastFailed(workflowId, message);
        m_controller->setLastError(message);
        return;
    }

    if (!WalletCryptoBackend::validateTransactionKernelSums(tx, &validationError)) {
        const QString message = QStringLiteral("Local pre-broadcast kernel sum/offset validation failed: %1").arg(validationError);
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        m_controller->markTransactionBroadcastFailed(workflowId, message);
        m_controller->setLastError(message);
        return;
    }

    if (!WalletCryptoBackend::validateTransactionKernelSignatures(tx, &validationError)) {
        const QString message = QStringLiteral("Local pre-broadcast kernel signature validation failed: %1").arg(validationError);
        m_controller->m_pendingBroadcastWorkflowId.clear();
        m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
        m_controller->m_pendingBroadcastInputCommits = QJsonArray();
        m_controller->markTransactionBroadcastFailed(workflowId, message);
        m_controller->setLastError(message);
        return;
    }

    m_controller->m_pendingBroadcastTxSkeleton = QJsonObject();
    m_controller->m_pendingBroadcastInputCommits = QJsonArray();
    m_controller->m_nodeApi->pushTransactionAsync(tx, true);
}

void GrinWalletNodeSyncService::onNodeUnspentOutputsFinished(const Result<OutputListing> &result)
{
    if (result.hasError()) {
        m_controller->m_syncStatus = QStringLiteral("Seed scan failed");
        emit m_controller->statusChanged();
        m_controller->setLastError(result.errorMessage());
        m_controller->m_seedScanActive = false;
        m_controller->m_walletScanInFlight = false;
        return;
    }

    if (!m_controller->m_seedScanActive || !m_controller->m_walletUnlocked || m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        return;
    }

    WalletKeychain keychain(m_controller->m_sessionMnemonic);
    if (!keychain.isValid()) {
        m_controller->setLastError(QStringLiteral("Wallet keychain could not be derived for seed scan."));
        m_controller->m_seedScanActive = false;
        m_controller->m_walletScanInFlight = false;
        return;
    }

    const QList<WalletOutput> discovered = WalletScanner::discoverOwnedOutputs(result.value().outputs(), keychain);
    for (int i = 0; i < discovered.size(); ++i) {
        bool exists = false;
        for (int j = 0; j < m_controller->m_seedScanDiscovered.size(); ++j) {
            if (m_controller->m_seedScanDiscovered.at(j).commitment == discovered.at(i).commitment) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_controller->m_seedScanDiscovered.append(discovered.at(i));
        }
    }

    const OutputListing listing = result.value();
    if (listing.lastRetrievedIndex() > 0 && listing.highestIndex() > 0
        && listing.lastRetrievedIndex() < listing.highestIndex()) {
        m_controller->m_seedScanNextIndex = listing.lastRetrievedIndex() + 1;
        QJsonObject document = m_controller->loadDocumentForService();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(listing.lastRetrievedIndex()));
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->m_syncStatus = QStringLiteral("Seed scan page %1 / %2")
                                         .arg(QString::number(listing.lastRetrievedIndex()))
                                         .arg(QString::number(listing.highestIndex()));
        emit m_controller->statusChanged();
        m_controller->m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_controller->m_seedScanNextIndex), -1, 1000, true);
        return;
    }
    if (listing.lastRetrievedIndex() == 0 && discovered.size() >= 1000) {
        m_controller->m_seedScanNextIndex += 1000;
        QJsonObject document = m_controller->loadDocumentForService();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(m_controller->m_seedScanNextIndex - 1));
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->m_syncStatus = QStringLiteral("Seed scan page starting at %1").arg(QString::number(m_controller->m_seedScanNextIndex));
        emit m_controller->statusChanged();
        m_controller->m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_controller->m_seedScanNextIndex), -1, 1000, true);
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < m_controller->m_seedScanDiscovered.size(); ++i) {
        bool exists = false;
        for (int j = 0; j < tracked.size(); ++j) {
            if (tracked.at(j).commitment == m_controller->m_seedScanDiscovered.at(i).commitment) {
                WalletOutput merged = tracked.at(j);
                const WalletOutput discovered = m_controller->m_seedScanDiscovered.at(i);
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
                    merged.locked = false;
                }
                tracked[j] = merged;
                exists = true;
                break;
            }
        }
        if (!exists) {
            tracked.append(m_controller->m_seedScanDiscovered.at(i));
        }
    }

    quint32 nextChildIndex = m_controller->nextChildIndexFromStateForService(walletState);
    for (int i = 0; i < tracked.size(); ++i) {
        if (tracked.at(i).childIndex + 1 > nextChildIndex) {
            nextChildIndex = tracked.at(i).childIndex + 1;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("restore_leaf_index"),
                       QString::number(listing.lastRetrievedIndex() > 0
                                           ? listing.lastRetrievedIndex()
                                           : (m_controller->m_seedScanNextIndex > 0 ? m_controller->m_seedScanNextIndex - 1 : 1)));
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
    m_controller->m_syncStatus = QStringLiteral("Seed scan complete");
    emit m_controller->statusChanged();
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QString());
    m_controller->m_seedScanActive = false;
    m_controller->m_walletScanInFlight = false;
}

void GrinWalletNodeSyncService::onNodeUnconfirmedTransactionsFinished(const Result<QList<PoolEntry> > &result)
{
    m_controller->m_broadcastStatusRefreshInFlight = false;
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
    m_controller->m_kernelStatusQueue.clear();

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
        m_controller->m_kernelStatusQueue.append(qMakePair(entry.value(QStringLiteral("workflow_id")).toString(), excess));
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();
    startNextKernelStatusCheck();
}

void GrinWalletNodeSyncService::onNodeKernelFinished(const Result<LocatedTxKernel> &result)
{
    m_controller->m_kernelStatusCheckInFlight = false;
    if (!m_controller->m_currentKernelWorkflowId.isEmpty()) {
        if (!result.hasError()) {
            m_controller->markTransactionKernelConfirmed(m_controller->m_currentKernelWorkflowId, result.value().height());
            m_controller->finalizeBroadcastedWorkflow(m_controller->m_currentKernelWorkflowId);
        } else if (result.errorMessage() == QStringLiteral("NotFound")) {
            m_controller->markTransactionKernelBroadcasted(m_controller->m_currentKernelWorkflowId);
            m_controller->setLastError(QString());
        }
    }

    m_controller->m_currentKernelWorkflowId.clear();
    m_controller->m_currentKernelExcess.clear();
    startNextKernelStatusCheck();
}

void GrinWalletNodeSyncService::onNodePushTransactionFinished(const Result<bool> &result)
{
    const QString workflowId = m_controller->m_pendingBroadcastWorkflowId;
    m_controller->m_pendingBroadcastWorkflowId.clear();

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
