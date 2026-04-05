#include "grinwalletnodesyncservice.h"

#include "grinwalletcontroller.h"
#include "grinwalletnodesync.h"
#include "input.h"
#include "nodeforeignapi.h"
#include "transaction.h"
#include "transactionbody.h"
#include "walletscanner.h"

// -------------------------------------------------------------------------------------------------------
// Driving Node Polling, Scan Control, And Broadcast Preflight
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletNodeSyncService::connectNodeClient
 */
void GrinWalletNodeSyncService::connectNodeClient()
{
    if (m_controller->nodeApi()) {
        m_controller->nodeApi()->deleteLater();
        m_controller->setNodeApi(0);
    }
    if (m_controller->nodeUrl().trimmed().isEmpty()) {
        return;
    }

    NodeForeignApi *nodeApi = new NodeForeignApi(m_controller->nodeUrl(), QString());
    nodeApi->setParent(this);
    m_controller->setNodeApi(nodeApi);

    connect(nodeApi, &NodeForeignApi::getTipFinished, this, &GrinWalletNodeSyncService::onNodeTipFinished);
    connect(nodeApi, &NodeForeignApi::getVersionFinished, this, &GrinWalletNodeSyncService::onNodeVersionFinished);
    connect(nodeApi, &NodeForeignApi::getOutputsFinished, this, &GrinWalletNodeSyncService::onNodeOutputsFinished);
    connect(nodeApi, &NodeForeignApi::getOutputCommitmentsFinished, this, &GrinWalletNodeSyncService::onNodeOutputCommitmentsFinished);
    connect(nodeApi, &NodeForeignApi::getUnspentOutputsFinished, this, &GrinWalletNodeSyncService::onNodeUnspentOutputsFinished);
    connect(nodeApi, &NodeForeignApi::getUnconfirmedTransactionsFinished, this, &GrinWalletNodeSyncService::onNodeUnconfirmedTransactionsFinished);
    connect(nodeApi, &NodeForeignApi::getKernelFinished, this, &GrinWalletNodeSyncService::onNodeKernelFinished);
    connect(nodeApi, &NodeForeignApi::pushTransactionFinished, this, &GrinWalletNodeSyncService::onNodePushTransactionFinished);
}

/**
 * @brief GrinWalletNodeSyncService::requestWalletScan
 */
void GrinWalletNodeSyncService::requestWalletScan()
{
    if (!m_controller->hasUnlockedSession()) {
        m_controller->setLastError(QStringLiteral("Unlock the wallet before scanning outputs."));
        return;
    }

    if (m_controller->walletScanInFlight() || m_controller->seedScanActive()) {
        m_controller->setLastInfo(QStringLiteral("Wallet scan is already running."));
        return;
    }

    if (!m_controller->nodeApi()) {
        connectNodeClient();
    }
    if (!m_controller->nodeApi()) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();

    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (outputs.isEmpty()) {
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_controller->chainHeight()));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_controller->chainHeight()));
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        m_controller->saveDocumentForService(document);
        m_controller->refreshStateFromStorage();
        m_controller->setLastInfo(QStringLiteral("Wallet has no tracked outputs yet. Starting seed scan."));
        m_controller->setSyncStatusMessage(QStringLiteral("Scanning wallet outputs..."));
        m_controller->notifyStatusChanged();
        m_controller->setWalletScanInFlight(true);
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

    m_controller->setSyncStatusMessage(QStringLiteral("Scanning wallet outputs..."));
    m_controller->notifyStatusChanged();
    m_controller->setWalletScanInFlight(true);
    startSeedScan();
}

/**
 * @brief GrinWalletNodeSyncService::startSeedScan
 */
void GrinWalletNodeSyncService::startSeedScan()
{
    m_controller->setSeedScanActive(true);
    const GrinWalletNodeSync::SeedScanState seedScanState =
        GrinWalletNodeSync::buildSeedScanState(m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject());
    m_controller->setSeedScanNextIndex(seedScanState.nextIndex);
    m_controller->clearSeedScanDiscovered();
    m_controller->setSyncStatusMessage(seedScanState.syncStatus);
    m_controller->notifyStatusChanged();
    if (m_controller->fullRescanInFlight()) {
        requestNextFullRescanBatch();
        return;
    }

    m_controller->nodeApi()->getUnspentOutputsAsync(static_cast<int>(m_controller->seedScanNextIndex()), -1, kSeedScanBatchSize, true);
}

/**
 * @brief GrinWalletNodeSyncService::finishSeedScan
 * @param message
 */
void GrinWalletNodeSyncService::finishSeedScan(const QString &message)
{

    m_controller->setSeedScanActive(false);
    m_controller->setFullRescanInFlight(false);
    if (!message.isEmpty()) {
        m_controller->setLastInfo(message);
    }
}

/**
 * @brief GrinWalletNodeSyncService::recoverPendingBroadcasts
 */
void GrinWalletNodeSyncService::recoverPendingBroadcasts()
{
    if (GrinWalletNodeSync::hasRecoverableBroadcasts(m_controller->loadDocumentForService())) {
        refreshBroadcastStatuses();
    }
}

/**
 * @brief GrinWalletNodeSyncService::refreshBroadcastStatuses
 */
void GrinWalletNodeSyncService::refreshBroadcastStatuses()
{
    if (!m_controller->nodeApi() || m_controller->broadcastStatusRefreshInFlight() || m_controller->kernelStatusCheckInFlight()) {
        return;
    }

    if (!GrinWalletNodeSync::shouldRefreshBroadcastStatuses(m_controller->loadDocumentForService())) {
        return;
    }

    m_controller->setBroadcastStatusRefreshInFlight(true);
    m_controller->nodeApi()->getUnconfirmedTransactionsAsync();
}

/**
 * @brief GrinWalletNodeSyncService::startNextKernelStatusCheck
 */
void GrinWalletNodeSyncService::startNextKernelStatusCheck()
{
    if (!m_controller->nodeApi() || m_controller->kernelStatusCheckInFlight() || !m_controller->hasPendingKernelStatusChecks()) {
        return;
    }

    const QPair<QString, QString> next = m_controller->takeNextKernelStatusCheck();

    m_controller->setCurrentKernelCheck(next.first, next.second);
    if (m_controller->currentKernelExcessInternal().isEmpty()) {
        startNextKernelStatusCheck();
        return;
    }

    m_controller->setKernelStatusCheckInFlight(true);
    m_controller->nodeApi()->getKernelAsync(m_controller->currentKernelExcessInternal(),
                                            0,
                                            static_cast<int>(m_controller->chainHeight() > 0 ? m_controller->chainHeight() + 2 : 0));
}

/**
 * @brief GrinWalletNodeSyncService::beginBroadcastWithInputPreflight
 * @param workflowId
 * @param txSkeleton
 */
void GrinWalletNodeSyncService::beginBroadcastWithInputPreflight(const QString &workflowId,
                                                                 const QJsonObject &txSkeleton)
{
    if (!m_controller->nodeApi()) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    const Transaction tx = Transaction::fromJson(txSkeleton);

    const QVector<Input> inputs = tx.body().inputs();
    if (inputs.isEmpty()) {
        m_controller->setPendingBroadcastWorkflowId(QString());
        m_controller->markTransactionBroadcastFailed(workflowId, QStringLiteral("Transaction has no inputs."));
        m_controller->setLastError(QStringLiteral("Transaction has no inputs."));
        return;
    }

    QJsonArray commits;
    for (int i = 0; i < inputs.size(); ++i) {
        commits.append(inputs.at(i).commit().hex());
    }

    m_controller->setPendingBroadcastInputLookup(true);
    m_controller->setPendingBroadcastTxSkeleton(txSkeleton);
    m_controller->setPendingBroadcastInputCommits(commits);
    m_controller->nodeApi()->getOutputCommitmentsAsync(commits);
}
