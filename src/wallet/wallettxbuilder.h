#ifndef WALLETTXBUILDER_H
#define WALLETTXBUILDER_H

#include <QList>
#include <QString>

#include "transaction.h"
#include "walletoutput.h"
#include "slatev4.h"

/**
 * @brief Builds transaction skeletons from wallet output data and Slate V4 payloads.
 */
class WalletTxBuilder
{
public:
    /**
     * @brief Result container for transaction skeleton construction.
     */
    struct BuildResult {
        Transaction transaction;
        QString transactionJson;
        bool success = false;
        QString error;
    };

    /**
     * @brief Builds a transaction skeleton from selected wallet inputs and output candidates.
     * @param slate Slate V4 payload containing identifiers, fee, and offset metadata.
     * @param selectedInputs Selected spendable wallet outputs to use as transaction inputs.
     * @param receiverOutput Output candidate representing the receiver side.
     * @param changeOutput Optional change output candidate.
     * @return Build result containing the assembled transaction or an error.
     */
    static BuildResult buildTransactionSkeleton(const SlateV4 &slate,
                                                const QList<WalletOutput> &selectedInputs,
                                                const WalletOutput *receiverOutput,
                                                const WalletOutput *changeOutput);

    /**
     * @brief Builds a transaction skeleton from compact slate commitments.
     * @param slate Slate V4 payload containing compact commitments.
     * @param receiverOutput Optional receiver output to append when missing in commitments.
     * @return Build result containing the assembled transaction or an error.
     */
    static BuildResult buildTransactionSkeletonFromCommitments(const SlateV4 &slate,
                                                               const WalletOutput *receiverOutput);
};

#endif // WALLETTXBUILDER_H
