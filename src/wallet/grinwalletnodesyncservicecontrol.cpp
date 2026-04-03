#include "grinwalletnodesyncservice.h"

#include "grinwalletcontroller.h"
#include "grinwalletnodesync.h"
#include "input.h"
#include "nodeforeignapi.h"
#include "transaction.h"
#include "transactionbody.h"
#include "walletscanner.h"

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
