#include "grinwallethistoryhelpers.h"

#include <QDateTime>
#include <QStringList>

#include "grinwalletcontrollerhelpers.h"

QString GrinWalletHistoryHelpers::syntheticWorkflowIdForCommitment(const QString &commitment)
{
    return QStringLiteral("rescan-%1").arg(commitment.left(24));
}

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
