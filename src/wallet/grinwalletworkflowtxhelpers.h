#ifndef GRINWALLETWORKFLOWTXHELPERS_H
#define GRINWALLETWORKFLOWTXHELPERS_H

#include <QJsonObject>
#include <QList>
#include <QString>

#include "slatev4.h"
#include "walletoutput.h"

class GrinWalletWorkflowTxHelpers
{
public:
    static QList<WalletOutput> collectSelectedInputs(const QJsonObject &localContext,
                                                     const QList<WalletOutput> &trackedOutputs);
    static WalletOutput resolveReceiverOutput(const SlateV4 &slate, const QString &knownChangeCommit);
    static WalletOutput resolveChangeOutput(const QJsonObject &localContext,
                                           const QList<WalletOutput> &trackedOutputs);
};

#endif
