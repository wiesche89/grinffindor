#ifndef GRINWALLETHISTORYHELPERS_H
#define GRINWALLETHISTORYHELPERS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "walletoutput.h"

class GrinWalletHistoryHelpers
{
public:
/**
 * @brief Returns synthetic workflow id for commitment.
 */
    static QString syntheticWorkflowIdForCommitment(const QString &commitment);
/**
 * @brief Returns transaction output commitments.
 */
    static QStringList transactionOutputCommitments(const QJsonObject &entry);
    static qint64 inferredConfirmedHeightForTransactionEntry(const QJsonObject &entry,
                                                             const QList<WalletOutput> &outputs);
/**
 * @brief Returns transaction sort key.
 */
    static qint64 transactionSortKey(const QJsonObject &entry);
/**
 * @brief Returns mode from outputs.
 */
    static QString modeFromOutputs(const QList<WalletOutput> &outputs, const QString &fallbackMode);
    static QJsonArray rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                           const QJsonArray &existingTransactions,
                                                           qulonglong chainHeight);
/**
 * @brief Returns transaction entry less than.
 */
    static bool transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right);
};

#endif
