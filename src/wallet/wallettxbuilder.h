#ifndef WALLETTXBUILDER_H
#define WALLETTXBUILDER_H

#include <QList>
#include <QString>

#include "transaction.h"
#include "walletoutput.h"
#include "slatev4.h"

class WalletTxBuilder
{
public:
    struct BuildResult {
        Transaction transaction;
        QString transactionJson;
        bool success = false;
        QString error;
    };

    static BuildResult buildTransactionSkeleton(const SlateV4 &slate,
                                                const QList<WalletOutput> &selectedInputs,
                                                const WalletOutput *receiverOutput,
                                                const WalletOutput *changeOutput);
    static BuildResult buildTransactionSkeletonFromCommitments(const SlateV4 &slate,
                                                               const WalletOutput *receiverOutput);
};

#endif // WALLETTXBUILDER_H
