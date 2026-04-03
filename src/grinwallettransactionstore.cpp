#include "grinwallettransactionstore.h"

#include <QDateTime>
#include <QJsonArray>

bool GrinWalletTransactionStore::markBroadcastPending(QJsonObject *document, const QString &workflowId)
{
    return updateTransaction(document,
                             workflowId,
                             QStringLiteral("broadcast_pending"),
                             false,
                             QString(),
                             true,
                             true,
                             false,
                             false,
                             0,
                             0,
                             false);
}

bool GrinWalletTransactionStore::markBroadcastFailed(QJsonObject *document,
                                                     const QString &workflowId,
                                                     const QString &message)
{
    return updateTransaction(document,
                             workflowId,
                             QStringLiteral("broadcast_failed"),
                             false,
                             message,
                             false,
                             false,
                             false,
                             false,
                             0,
                             0,
                             false);
}

bool GrinWalletTransactionStore::markKernelConfirmed(QJsonObject *document,
                                                     const QString &workflowId,
                                                     qulonglong chainHeight,
                                                     qulonglong confirmedHeight)
{
    return updateTransaction(document,
                             workflowId,
                             QStringLiteral("confirmed"),
                             true,
                             QString(),
                             false,
                             false,
                             false,
                             true,
                             chainHeight,
                             confirmedHeight,
                             false);
}

bool GrinWalletTransactionStore::markKernelBroadcasted(QJsonObject *document, const QString &workflowId)
{
    return updateTransaction(document,
                             workflowId,
                             QStringLiteral("broadcasted"),
                             true,
                             QString(),
                             false,
                             false,
                             false,
                             false,
                             0,
                             0,
                             true);
}

bool GrinWalletTransactionStore::markBroadcastSucceeded(QJsonObject *document, const QString &workflowId)
{
    return updateTransaction(document,
                             workflowId,
                             QStringLiteral("broadcasted"),
                             true,
                             QString(),
                             true,
                             false,
                             true,
                             false,
                             0,
                             0,
                             false);
}

bool GrinWalletTransactionStore::updateTransaction(QJsonObject *document,
                                                   const QString &workflowId,
                                                   const QString &status,
                                                   bool broadcasted,
                                                   const QString &errorMessage,
                                                   bool clearBroadcastError,
                                                   bool updateBroadcastAttempts,
                                                   bool setBroadcastAt,
                                                   bool setConfirmedHeight,
                                                   qulonglong chainHeight,
                                                   qulonglong confirmedHeight,
                                                   bool keepMempoolStatus)
{
    if (!document || workflowId.trimmed().isEmpty()) {
        return false;
    }

    QJsonObject walletState = document->value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        if (entry.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (keepMempoolStatus) {
            const QString currentStatus = entry.value(QStringLiteral("status")).toString();
            entry.insert(QStringLiteral("status"),
                         currentStatus == QStringLiteral("in_mempool")
                             ? QStringLiteral("in_mempool")
                             : status);
        } else {
            entry.insert(QStringLiteral("status"), status);
        }
        entry.insert(QStringLiteral("broadcasted"), broadcasted);
        entry.insert(QStringLiteral("last_node_check"), now);

        if (updateBroadcastAttempts) {
            entry.insert(QStringLiteral("last_broadcast_attempt"), now);
            entry.insert(QStringLiteral("broadcast_attempts"),
                         entry.value(QStringLiteral("broadcast_attempts")).toInt() + 1);
        }

        if (setBroadcastAt) {
            entry.insert(QStringLiteral("broadcast_at"), now);
        }

        if (setConfirmedHeight) {
            entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(confirmedHeight));
            entry.insert(QStringLiteral("confirmations"),
                         static_cast<qint64>(chainHeight >= confirmedHeight
                             ? (chainHeight - confirmedHeight + 1)
                             : 0));
        } else if (keepMempoolStatus) {
            entry.insert(QStringLiteral("confirmations"), 0);
        }

        if (clearBroadcastError) {
            entry.remove(QStringLiteral("broadcast_error"));
        } else if (!errorMessage.isEmpty()) {
            entry.insert(QStringLiteral("broadcast_error"), errorMessage);
        }

        transactions.replace(i, entry);
        walletState.insert(QStringLiteral("transactions"), transactions);
        document->insert(QStringLiteral("wallet_state"), walletState);
        return true;
    }

    return false;
}
