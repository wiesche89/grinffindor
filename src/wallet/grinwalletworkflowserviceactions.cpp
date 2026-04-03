#include "grinwalletworkflowservice.h"

#include <QDateTime>
#include <QJsonDocument>

#include "grinwalletcontroller.h"
#include "walletscanner.h"

void GrinWalletWorkflowService::clearWorkflow()
{
    m_controller->setWorkflow(QString(), QString(), QString(), QString(), QString());
    m_controller->setLastInfo(QStringLiteral("Workflow state cleared."));
}

void GrinWalletWorkflowService::cleanupLocalAndCancelledItems()
{
    m_controller->touchWalletSession();

    if (!m_controller->m_walletUnlocked || m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        m_controller->setLastError(QStringLiteral("Unlock the wallet before cleaning up."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    int localCount = 0;
    for (int i = outputs.size() - 1; i >= 0; --i) {
        if (!outputs.at(i).onChain) {
            outputs.removeAt(i);
            ++localCount;
        }
    }

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    int cancelledCount = 0;
    for (int i = transactions.size() - 1; i >= 0; --i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("status")).toString() == QStringLiteral("cancelled")) {
            transactions.removeAt(i);
            ++cancelledCount;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"),
                       WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    m_controller->setLastInfo(
        QStringLiteral("Cleanup completed: %1 local output(s) and %2 cancelled transaction(s) removed.")
            .arg(QString::number(localCount))
            .arg(QString::number(cancelledCount)));
}

void GrinWalletWorkflowService::broadcastCurrentWorkflowTransaction()
{
    m_controller->touchWalletSession();
    const QJsonDocument workflowDoc = QJsonDocument::fromJson(m_controller->m_workflowDecoded.toUtf8());
    if (!workflowDoc.isObject()) {
        m_controller->setLastError(QStringLiteral("Current workflow does not contain decodable transaction data."));
        return;
    }

    const QJsonObject txSkeleton = workflowDoc.object().value(QStringLiteral("tx_skeleton")).toObject();
    if (txSkeleton.isEmpty()) {
        m_controller->setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
        return;
    }

    if (!m_controller->m_nodeApi) {
        m_controller->connectNodeClient();
    }
    if (!m_controller->m_nodeApi) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    if (!m_controller->m_pendingBroadcastWorkflowId.isEmpty()) {
        m_controller->setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
        return;
    }

    const SlateV4 slate = SlateV4::fromJson(workflowDoc.object());
    m_controller->persistWorkflowTransaction(slate, false);
    m_controller->markTransactionBroadcastPending(slate.workflowId());
    m_controller->m_pendingBroadcastWorkflowId = slate.workflowId();
    m_controller->beginBroadcastWithInputPreflight(slate.workflowId(), txSkeleton);
}

void GrinWalletWorkflowService::broadcastTransaction(const QString &workflowId)
{
    m_controller->touchWalletSession();
    const QJsonArray transactions = m_controller->loadDocumentForService()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        const QJsonObject txSkeleton = tx.value(QStringLiteral("tx_skeleton")).toObject();
        if (txSkeleton.isEmpty()) {
            m_controller->setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
            return;
        }
        if (!m_controller->m_nodeApi) {
            m_controller->connectNodeClient();
        }
        if (!m_controller->m_nodeApi) {
            m_controller->setLastError(QStringLiteral("Node client is not configured."));
            return;
        }
        if (!m_controller->m_pendingBroadcastWorkflowId.isEmpty()) {
            m_controller->setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
            return;
        }

        SlateV4 slate;
        slate.id = tx.value(QStringLiteral("slate_id")).toString();
        slate.amount = tx.value(QStringLiteral("amount")).toString();
        slate.fee = tx.value(QStringLiteral("fee")).toString();
        slate.offset = tx.value(QStringLiteral("offset")).toString();
        slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        slate.setStateFromCode(tx.value(QStringLiteral("state")).toString());
        m_controller->persistWorkflowTransaction(slate, false);
        m_controller->markTransactionBroadcastPending(workflowId);
        m_controller->m_pendingBroadcastWorkflowId = workflowId;
        m_controller->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
        return;
    }

    m_controller->setLastError(QStringLiteral("Transaction not found in wallet history."));
}

void GrinWalletWorkflowService::cancelTransaction(const QString &workflowId)
{
    m_controller->touchWalletSession();
    if (workflowId.trimmed().isEmpty()) {
        m_controller->setLastError(QStringLiteral("Workflow id is required for cancel."));
        return;
    }

    const QJsonArray existingTransactions = m_controller->loadDocumentForService()
                                                .value(QStringLiteral("wallet_state"))
                                                .toObject()
                                                .value(QStringLiteral("transactions"))
                                                .toArray();
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject tx = existingTransactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        if (tx.value(QStringLiteral("confirmations")).toInt() > 0
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("confirmed")) {
            m_controller->setLastError(QStringLiteral("Confirmed transactions can no longer be cancelled."));
            return;
        }
        if (tx.value(QStringLiteral("broadcasted")).toBool()
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("broadcasted")
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("in_mempool")) {
            m_controller->setLastError(QStringLiteral("Broadcasted transactions can no longer be cancelled locally."));
            return;
        }
        break;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonObject localContext = m_controller->workflowContext(workflowId);
    const QJsonArray selectedInputCommits = localContext.value(QStringLiteral("selected_input_commits")).toArray();

    QStringList selectedInputCommitments;
    for (int j = 0; j < selectedInputCommits.size(); ++j) {
        selectedInputCommitments.append(selectedInputCommits.at(j).toString());
    }

    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (outputs[i].spent) {
            continue;
        }

        if (selectedInputCommitments.contains(outputs[i].commitment)) {
            outputs[i].locked = false;
            outputs[i].pending = false;
        } else {
            outputs.removeAt(i);
            --i;
        }
    }

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() == workflowId) {
            tx.insert(QStringLiteral("status"), QStringLiteral("cancelled"));
            tx.insert(QStringLiteral("cancelled_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            transactions.replace(i, tx);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"),
                       WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.remove(workflowId);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    if (m_controller->m_workflowId == workflowId) {
        clearWorkflow();
    }
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Transaction %1 cancelled and UTXOs released.").arg(workflowId));
}
