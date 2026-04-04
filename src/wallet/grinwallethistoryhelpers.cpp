#include "grinwallethistoryhelpers.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QStringList>

#include "grinwalletcontrollerhelpers.h"
#include "grinwalletworkflowhelpers.h"

/**
 * @brief GrinWalletHistoryHelpers::syntheticWorkflowIdForCommitment
 * @param commitment
 * @return
 */
QString GrinWalletHistoryHelpers::syntheticWorkflowIdForCommitment(const QString &commitment)
{
    return QStringLiteral("rescan-%1").arg(commitment.left(24));
}

/**
 * @brief GrinWalletHistoryHelpers::transactionOutputCommitments
 * @param entry
 * @return
 */
QStringList GrinWalletHistoryHelpers::transactionOutputCommitments(const QJsonObject &entry)
{
    QStringList commitments;

    const QString directCommitment = entry.value(QStringLiteral("commitment")).toString();
    if (!directCommitment.isEmpty()) {
        commitments.append(directCommitment);
    }

    const QJsonArray outputs = entry.value(QStringLiteral("tx_skeleton"))
                               .toObject()
                               .value(QStringLiteral("body"))
                               .toObject()
                               .value(QStringLiteral("outputs"))

                               .toArray();
    for (int i = 0; i < outputs.size(); ++i) {
        const QString commitment = outputs.at(i).toObject().value(QStringLiteral("commit")).toString();
        if (!commitment.isEmpty() && !commitments.contains(commitment)) {
            commitments.append(commitment);
        }
    }

    return commitments;
}

/**
 * @brief GrinWalletHistoryHelpers::inferredConfirmedHeightForTransactionEntry
 * @param entry
 * @param outputs
 * @return
 */
qint64 GrinWalletHistoryHelpers::inferredConfirmedHeightForTransactionEntry(const QJsonObject &entry,
                                                                            const QList<WalletOutput> &outputs)
{
    const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
    const QString mode = entry.value(QStringLiteral("mode")).toString();
    const bool isReceiveSide = (mode == QStringLiteral("receive") || mode == QStringLiteral("invoice"));
    quint64 inferredHeight = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.height == 0) {
            continue;
        }
        if (!isReceiveSide && output.spent) {
            continue;
        }
        if (!workflowId.isEmpty() && output.workflowId == workflowId) {
            if (inferredHeight == 0 || output.height < inferredHeight) {
                inferredHeight = output.height;
            }
        }
    }

    if (inferredHeight == 0) {
        const QStringList commitments = transactionOutputCommitments(entry);
        for (int i = 0; i < commitments.size(); ++i) {
            const WalletOutput output =
                GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, commitments.at(i));
            if (!output.onChain || output.height == 0) {
                continue;
            }
            if (!isReceiveSide && output.spent) {
                continue;
            }
            if (inferredHeight == 0 || output.height < inferredHeight) {
                inferredHeight = output.height;
            }
        }
    }

    if (inferredHeight > 0) {
        return static_cast<qint64>(inferredHeight);
    }

    return entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
}

/**
 * @brief GrinWalletHistoryHelpers::transactionSortKey
 * @param entry
 * @return
 */
qint64 GrinWalletHistoryHelpers::transactionSortKey(const QJsonObject &entry)
{
    const QStringList timeFields = QStringList()
        << QStringLiteral("cancelled_at")
        << QStringLiteral("broadcast_at")
        << QStringLiteral("last_broadcast_attempt")

        << QStringLiteral("timestamp");

    for (const QString &field : timeFields) {
        const QString value = entry.value(field).toString().trimmed();
        if (value.isEmpty()) {
            continue;
        }
        const QDateTime parsed = QDateTime::fromString(value, Qt::ISODate);
        if (parsed.isValid()) {
            return parsed.toUTC().toMSecsSinceEpoch();
        }
    }

    const qint64 confirmedHeight = entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
    if (confirmedHeight > 0) {
        return confirmedHeight;
    }

    return 0;
}

/**
 * @brief GrinWalletHistoryHelpers::modeFromOutputs
 * @param outputs
 * @param fallbackMode
 * @return
 */
QString GrinWalletHistoryHelpers::modeFromOutputs(const QList<WalletOutput> &outputs,
                                                  const QString &fallbackMode)
{
    if (!fallbackMode.trimmed().isEmpty()) {
        return fallbackMode;
    }

    bool hasChange = false;
    bool hasReceiveLike = false;
    for (int i = 0; i < outputs.size(); ++i) {
        const QString source = outputs.at(i).source;
        if (source == QStringLiteral("change")) {
            hasChange = true;
        }
        if (source == QStringLiteral("receive") || source == QStringLiteral("invoice")) {
            hasReceiveLike = true;
        }
    }

    if (hasChange) {
        return QStringLiteral("send");
    }
    if (hasReceiveLike) {
        return QStringLiteral("receive");
    }
    return QStringLiteral("receive");
}

/**
 * @brief GrinWalletHistoryHelpers::rebuildTransactionHistoryFromOutputs
 * @param outputs
 * @param existingTransactions
 * @param chainHeight
 * @return
 */
QJsonArray GrinWalletHistoryHelpers::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                                          const QJsonArray &existingTransactions,
                                                                          qulonglong chainHeight)
{
    QHash<QString, QJsonObject> knownTransactionsByWorkflow;
    QHash<QString, QString> workflowByCommitment;
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty()) {
            knownTransactionsByWorkflow.insert(workflowId, entry);
        }

        const QStringList commitments = transactionOutputCommitments(entry);
        for (int j = 0; j < commitments.size(); ++j) {
            workflowByCommitment.insert(
                commitments.at(j),
                workflowId.isEmpty() ? syntheticWorkflowIdForCommitment(commitments.at(j)) : workflowId);
        }
    }

    QHash<QString, QList<WalletOutput> > groupedOutputs;
    QStringList orderedWorkflowIds;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.commitment.isEmpty()) {
            continue;
        }

        QString workflowId = output.workflowId.trimmed();
        if (workflowId.isEmpty()) {
            workflowId = workflowByCommitment.value(output.commitment);
        }
        if (workflowId.isEmpty()) {
            workflowId = syntheticWorkflowIdForCommitment(output.commitment);
        }

        if (!groupedOutputs.contains(workflowId)) {
            orderedWorkflowIds.append(workflowId);
        }
        groupedOutputs[workflowId].append(output);
    }

    QJsonArray transactions;
    QSet<QString> appendedExistingWorkflows;
    for (int i = 0; i < orderedWorkflowIds.size(); ++i) {
        const QString workflowId = orderedWorkflowIds.at(i);
        const QList<WalletOutput> grouped = groupedOutputs.value(workflowId);
        if (grouped.isEmpty()) {
            continue;
        }

        QJsonObject entry = knownTransactionsByWorkflow.value(workflowId);
        appendedExistingWorkflows.insert(workflowId);

        quint64 confirmedHeight = 0;
        bool anySpent = false;
        quint64 displayAmount = 0;
        QString primaryCommitment;
        QJsonArray outputCommitments;

        for (int j = 0; j < grouped.size(); ++j) {
            const WalletOutput &output = grouped.at(j);
            outputCommitments.append(output.commitment);
            if (primaryCommitment.isEmpty()) {
                primaryCommitment = output.commitment;
            }
            if (!output.spent && output.height > 0) {
                if (confirmedHeight == 0 || output.height < confirmedHeight) {
                    confirmedHeight = output.height;
                }
            }
            anySpent = anySpent || output.spent;
            if (displayAmount == 0 && output.source != QStringLiteral("change")) {
                displayAmount = GrinWalletWorkflowHelpers::amountToNanogrin(output.amount);
            }
        }

        if (displayAmount == 0) {
            displayAmount = GrinWalletWorkflowHelpers::amountToNanogrin(grouped.first().amount);
        }

        const QString mode = modeFromOutputs(grouped, entry.value(QStringLiteral("mode")).toString());
        const QString existingStatus = entry.value(QStringLiteral("status")).toString();
        const bool existingBroadcasted = entry.value(QStringLiteral("broadcasted")).toBool();
        QString status = existingStatus;
        if (status.isEmpty() || status == QStringLiteral("cancelled")) {
            status = anySpent && mode == QStringLiteral("send")
                ? QStringLiteral("spent")
                : QStringLiteral("confirmed");
        }
        const bool broadcasted = existingBroadcasted
            || (mode == QStringLiteral("send") && status != QStringLiteral("cancelled"));

        entry.insert(QStringLiteral("workflow_id"), workflowId);
        entry.insert(QStringLiteral("mode"), mode);
        entry.insert(QStringLiteral("state"),
                     entry.value(QStringLiteral("state")).toString().isEmpty()
                         ? QStringLiteral("chain")
                         : entry.value(QStringLiteral("state")).toString());
        if (entry.value(QStringLiteral("amount")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("amount"), GrinWalletWorkflowHelpers::formatNanogrin(displayAmount));
        }
        if (entry.value(QStringLiteral("fee")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("fee"), QStringLiteral("0.000000000"));
        }
        entry.insert(QStringLiteral("broadcasted"), broadcasted);
        entry.insert(QStringLiteral("tx_ready"), true);
        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("commitment"), primaryCommitment);
        entry.insert(QStringLiteral("output_commitments"), outputCommitments);
        entry.insert(QStringLiteral("source"),
                     grouped.first().source.isEmpty() ? QStringLiteral("scan") : grouped.first().source);
        entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(confirmedHeight));
        entry.insert(QStringLiteral("confirmations"),
                     confirmedHeight > 0 && chainHeight >= confirmedHeight
                         ? static_cast<qint64>(chainHeight - confirmedHeight + 1)
                         : 0);
        entry.insert(QStringLiteral("rescan_rebuilt"), true);
        if (entry.value(QStringLiteral("timestamp")).toString().isEmpty()) {
            entry.insert(QStringLiteral("timestamp"), QString());
        }
        transactions.append(entry);
    }

    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (entry.value(QStringLiteral("rescan_rebuilt")).toBool()
            && !workflowId.isEmpty()
            && !appendedExistingWorkflows.contains(workflowId)) {
            continue;
        }
        if (!workflowId.isEmpty() && !appendedExistingWorkflows.contains(workflowId)) {
            transactions.append(entry);
        }
    }

    return transactions;
}

/**
 * @brief GrinWalletHistoryHelpers::transactionEntryLessThan
 * @param left
 * @param right
 * @return
 */
bool GrinWalletHistoryHelpers::transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right)
{
    const qint64 leftKey = transactionSortKey(left);

    const qint64 rightKey = transactionSortKey(right);
    if (leftKey != rightKey) {
        return leftKey > rightKey;
    }

    return left.value(QStringLiteral("workflow_id")).toString()
        > right.value(QStringLiteral("workflow_id")).toString();
}
