#ifndef GRINWALLETCONTROLLERHELPERS_H
#define GRINWALLETCONTROLLERHELPERS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "wallet/walletkeychain.h"
#include "wallet/walletoutput.h"

class GrinWalletControllerHelpers
{
public:
    static QString defaultNetworkName();
    static bool isAcceptedNetworkName(const QString &networkName);
    static QString defaultNodeUrlForNetwork(const QString &networkName);
    static QString inferNetworkName(const QString &networkName, const QString &nodeUrl);
    static bool isNodeUrlAccepted(const QString &nodeUrl);
    static bool isFinalTransactionStatus(const QString &status);
    static WalletOutput findTrackedOutputByCommitment(const QList<WalletOutput> &outputs,
                                                      const QString &commitment);
    static WalletOutput normalizedTrackedOutput(const WalletOutput &output,
                                                const WalletKeychain &keychain);
    static QString displayAmountForTransactionEntry(const QJsonObject &entry,
                                                    const QList<WalletOutput> &outputs);
    static QJsonObject filterWorkflowContextsForTransactions(const QJsonObject &contexts,
                                                             const QJsonArray &transactions);
};

#endif
