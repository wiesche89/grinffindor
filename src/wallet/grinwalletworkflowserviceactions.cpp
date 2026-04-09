#include "grinwalletworkflowservice.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QSet>

#include "grinwalletcontroller.h"
#include "walletscanner.h"

namespace {

QString inputCommitmentFromTxInput(const QJsonObject &input)
{
    QString commitment = input.value(QStringLiteral("commit")).toString();
    if (!commitment.isEmpty()) {
        return commitment;
    }
    return input.value(QStringLiteral("commit")).toObject().value(QStringLiteral("hex")).toString();
}

}

// -------------------------------------------------------------------------------------------------------
// Clearing, Broadcasting, And Cancelling Workflows
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletWorkflowService::clearWorkflow
 */
void GrinWalletWorkflowService::clearWorkflow()
{
    m_controller->setWorkflow(QString(), QString(), QString(), QString(), QString());
    m_controller->setLastInfo(QStringLiteral("Workflow state cleared."));
}

/**
 * @brief GrinWalletWorkflowService::cleanupLocalAndCancelledItems
 */
void GrinWalletWorkflowService::cleanupLocalAndCancelledItems()
{

    m_controller->touchWalletSession();

    if (!m_controller->hasUnlockedSession()) {
        m_controller->setLastError(QStringLiteral("Unlock the wallet before cleaning up."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    const QJsonArray existingTransactions = walletState.value(QStringLiteral("transactions")).toArray();
    QSet<QString> cancelledWorkflowIds;
    QSet<QString> cancelledInputCommitments;
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject tx = existingTransactions.at(i).toObject();
        if (tx.value(QStringLiteral("status")).toString() != QStringLiteral("cancelled")) {
            continue;
        }

        const QString workflowId = tx.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty()) {
            cancelledWorkflowIds.insert(workflowId);
        }

        const QJsonArray inputs = tx.value(QStringLiteral("tx_skeleton"))
                                  .toObject()
                                  .value(QStringLiteral("body"))
                                  .toObject()
                                  .value(QStringLiteral("inputs"))
                                  .toArray();
        for (int j = 0; j < inputs.size(); ++j) {
            const QString commitment = inputCommitmentFromTxInput(inputs.at(j).toObject());
            if (!commitment.isEmpty()) {
                cancelledInputCommitments.insert(commitment);
            }
        }
    }

    int localCount = 0;
    for (int i = outputs.size() - 1; i >= 0; --i) {
        if (cancelledInputCommitments.contains(outputs.at(i).commitment)) {
            outputs[i].locked = false;
            outputs[i].pending = false;
            continue;
        }

        if (cancelledWorkflowIds.contains(outputs.at(i).workflowId) && !outputs.at(i).onChain) {
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
                       WalletScanner::balancesFromOutputs(outputs, m_controller->chainHeight(), static_cast<qulonglong>(m_controller->minimumConfirmations())));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    m_controller->setLastInfo(
        QStringLiteral("Cleanup completed: %1 local output(s) and %2 cancelled transaction(s) removed.")
            .arg(QString::number(localCount))
            .arg(QString::number(cancelledCount)));
}

/**
 * @brief GrinWalletWorkflowService::broadcastCurrentWorkflowTransaction
 */
void GrinWalletWorkflowService::broadcastCurrentWorkflowTransaction()
{
    m_controller->touchWalletSession();

    const QJsonDocument workflowDoc = QJsonDocument::fromJson(m_controller->workflowDecoded().toUtf8());
    if (!workflowDoc.isObject()) {
        m_controller->setLastError(QStringLiteral("Current workflow does not contain decodable transaction data."));
        return;
    }

    const QJsonObject txSkeleton = workflowDoc.object().value(QStringLiteral("tx_skeleton")).toObject();
    if (txSkeleton.isEmpty()) {
        m_controller->setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
        return;
    }

    if (!m_controller->nodeApi()) {
        m_controller->connectNodeClient();
    }
    if (!m_controller->nodeApi()) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    if (m_controller->hasPendingBroadcastWorkflow()) {
        m_controller->setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
        return;
    }

    const SlateV4 slate = SlateV4::fromJson(workflowDoc.object());
    m_controller->persistWorkflowTransaction(slate, false);
    m_controller->markTransactionBroadcastPending(slate.workflowId());
    m_controller->setPendingBroadcastWorkflowId(slate.workflowId());
    m_controller->beginBroadcastWithInputPreflight(slate.workflowId(), txSkeleton);
}

/**
 * @brief GrinWalletWorkflowService::broadcastTransaction
 * @param workflowId
 */
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
        if (!m_controller->nodeApi()) {
            m_controller->connectNodeClient();
        }
        if (!m_controller->nodeApi()) {
            m_controller->setLastError(QStringLiteral("Node client is not configured."));
            return;
        }
        if (m_controller->hasPendingBroadcastWorkflow()) {
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
        m_controller->setPendingBroadcastWorkflowId(workflowId);
        m_controller->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
        return;
    }

    m_controller->setLastError(QStringLiteral("Transaction not found in wallet history."));
}

/**
 * @brief GrinWalletWorkflowService::cancelTransaction
 * @param workflowId
 */
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

    for (int i = outputs.size() - 1; i >= 0; --i) {
        if (outputs[i].spent) {
            continue;
        }

        if (selectedInputCommitments.contains(outputs[i].commitment)) {
            outputs[i].locked = false;
            outputs[i].pending = false;
            continue;
        }

        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (!outputs[i].onChain) {
            outputs.removeAt(i);
            continue;
        }

        outputs[i].locked = false;
        outputs[i].pending = false;
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
                       WalletScanner::balancesFromOutputs(outputs, m_controller->chainHeight(), static_cast<qulonglong>(m_controller->minimumConfirmations())));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.remove(workflowId);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    m_controller->saveDocumentForService(document);

    m_controller->refreshStateFromStorage();

    if (m_controller->workflowId() == workflowId) {
        clearWorkflow();
    }
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Transaction %1 cancelled and UTXOs released.").arg(workflowId));
}
