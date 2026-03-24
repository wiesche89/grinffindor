#ifndef WALLETTSCANNER_H
#define WALLETTSCANNER_H

#include <QJsonArray>
#include <QJsonObject>
#include <QList>

#include "walletoutput.h"

class OutputPrintable;
class WalletKeychain;

class WalletScanner
{
public:
    static QList<WalletOutput> outputsFromState(const QJsonObject &walletState);
    static QJsonArray outputsToJson(const QList<WalletOutput> &outputs);
    static QJsonArray commitmentsToJson(const QList<WalletOutput> &outputs);
    static QList<WalletOutput> reconcileTrackedOutputs(const QList<WalletOutput> &trackedOutputs,
                                                       const QList<OutputPrintable> &chainOutputs);
    static QList<WalletOutput> discoverOwnedOutputs(const QList<OutputPrintable> &chainOutputs,
                                                    const WalletKeychain &keychain);
    static QJsonObject balancesFromOutputs(const QList<WalletOutput> &outputs, qulonglong chainHeight);
};

#endif // WALLETTSCANNER_H
