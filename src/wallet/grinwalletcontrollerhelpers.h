#ifndef GRINWALLETCONTROLLERHELPERS_H
#define GRINWALLETCONTROLLERHELPERS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "walletkeychain.h"
#include "walletoutput.h"

class GrinWalletControllerHelpers
{
public:
/**
 * @brief Processes default network name.
 */
    static QString defaultNetworkName();
/**
 * @brief Returns whether accepted network name.
 */
    static bool isAcceptedNetworkName(const QString &networkName);
/**
 * @brief Returns default node url for network.
 */
    static QString defaultNodeUrlForNetwork(const QString &networkName);
/**
 * @brief Returns infer network name.
 */
    static QString inferNetworkName(const QString &networkName, const QString &nodeUrl);
/**
 * @brief Returns whether node url accepted.
 */
    static bool isNodeUrlAccepted(const QString &nodeUrl);
/**
 * @brief Returns whether final transaction status.
 */
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
