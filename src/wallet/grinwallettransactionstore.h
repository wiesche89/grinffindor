#ifndef GRINWALLETTRANSACTIONSTORE_H
#define GRINWALLETTRANSACTIONSTORE_H

#include <QJsonObject>
#include <QString>

class GrinWalletTransactionStore
{
public:
    static bool markBroadcastPending(QJsonObject *document, const QString &workflowId);
    static bool markBroadcastFailed(QJsonObject *document, const QString &workflowId, const QString &message);
    static bool markKernelConfirmed(QJsonObject *document,
                                    const QString &workflowId,
                                    qulonglong chainHeight,
                                    qulonglong confirmedHeight);
    static bool markKernelBroadcasted(QJsonObject *document, const QString &workflowId);
    static bool markBroadcastSucceeded(QJsonObject *document, const QString &workflowId);

private:
    static bool updateTransaction(QJsonObject *document,
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
                                  bool keepMempoolStatus);
};

#endif
