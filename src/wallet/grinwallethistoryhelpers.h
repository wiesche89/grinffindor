#ifndef GRINWALLETHISTORYHELPERS_H
#define GRINWALLETHISTORYHELPERS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "walletoutput.h"

class GrinWalletHistoryHelpers
{
public:
    static QString syntheticWorkflowIdForCommitment(const QString &commitment);
    static QStringList transactionOutputCommitments(const QJsonObject &entry);
    static qint64 inferredConfirmedHeightForTransactionEntry(const QJsonObject &entry,
                                                             const QList<WalletOutput> &outputs);
    static qint64 transactionSortKey(const QJsonObject &entry);
    static QString modeFromOutputs(const QList<WalletOutput> &outputs, const QString &fallbackMode);
    static QJsonArray rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                           const QJsonArray &existingTransactions,
                                                           qulonglong chainHeight);
    static bool transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right);
};

#endif
