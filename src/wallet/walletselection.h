#ifndef WALLETSELECTION_H
#define WALLETSELECTION_H

#include <QList>
#include <QString>

#include "walletoutput.h"

/**
 * @brief Provides output selection and fee estimation for wallet send workflows.
 */
class WalletSelection
{
public:
    /**
     * @brief Selection result including chosen outputs, fee, and change.
     */
    struct Result {
        QList<WalletOutput> selectedOutputs;
        quint64 amount = 0;
        quint64 fee = 0;
        quint64 totalSelected = 0;
        quint64 change = 0;
        bool success = false;
        QString error;
    };

    /**
     * @brief Selects spendable outputs to satisfy amount plus fee requirements.
     * @param outputs Available wallet outputs.
     * @param amount Requested send amount in nanogrin.
     * @param chainHeight Current chain height used for maturity checks.
     * @return Selection result with the best candidate set.
     */
    static Result selectSpendableOutputs(const QList<WalletOutput> &outputs,
                                         quint64 amount,
                                         qulonglong chainHeight);

    /**
     * @brief Estimates transaction fee from input/output/kernel counts.
     * @param numInputs Number of transaction inputs.
     * @param numOutputs Number of transaction outputs.
     * @param numKernels Number of transaction kernels.
     * @return Estimated fee in nanogrin.
     */
    static quint64 estimateFee(int numInputs, int numOutputs, int numKernels = 1);
};

#endif // WALLETSELECTION_H
