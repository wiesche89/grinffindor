#ifndef WALLETSELECTION_H
#define WALLETSELECTION_H

#include <QList>
#include <QString>

#include "walletoutput.h"

class WalletSelection
{
public:
    struct Result {
        QList<WalletOutput> selectedOutputs;
        quint64 amount = 0;
        quint64 fee = 0;
        quint64 totalSelected = 0;
        quint64 change = 0;
        bool success = false;
        QString error;
    };

    static Result selectSpendableOutputs(const QList<WalletOutput> &outputs,
                                         quint64 amount,
                                         qulonglong chainHeight);
    static quint64 estimateFee(int numInputs, int numOutputs, int numKernels = 1);
};

#endif // WALLETSELECTION_H
