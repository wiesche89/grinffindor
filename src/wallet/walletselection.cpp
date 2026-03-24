#include "walletselection.h"

#include <algorithm>
#include <QStringList>

namespace {

quint64 amountToNanogrin(const QString &amount)
{
    const QString trimmed = amount.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    const QStringList parts = trimmed.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = parts.size() == 2 ? parts.at(1) : QString();
    if (fractional.size() > 9) {
        fractional = fractional.left(9);
    }
    while (fractional.size() < 9) {
        fractional.append(QLatin1Char('0'));
    }

    bool fracOk = false;
    const quint64 frac = fractional.isEmpty() ? 0 : fractional.toULongLong(&fracOk);
    if (!fractional.isEmpty() && !fracOk) {
        return 0;
    }

    return whole * 1000000000ULL + frac;
}

bool isSpendable(const WalletOutput &output, qulonglong chainHeight)
{
    if (output.spent || output.locked || !output.onChain || output.height == 0) {
        return false;
    }
    if (chainHeight < output.height + 10) {
        return false;
    }
    if (output.coinbase && chainHeight < output.height + 1000) {
        return false;
    }
    return true;
}

}

quint64 WalletSelection::estimateFee(int numInputs, int numOutputs, int numKernels)
{
    const quint64 weight = static_cast<quint64>(numInputs) * 4ULL
                         + static_cast<quint64>(numOutputs) * 21ULL
                         + static_cast<quint64>(numKernels) * 3ULL;
    return weight * 1000000ULL;
}

WalletSelection::Result WalletSelection::selectSpendableOutputs(const QList<WalletOutput> &outputs,
                                                                quint64 amount,
                                                                qulonglong chainHeight)
{
    Result result;
    result.amount = amount;

    QList<WalletOutput> candidates;
    for (int i = 0; i < outputs.size(); ++i) {
        if (isSpendable(outputs.at(i), chainHeight)) {
            candidates.append(outputs.at(i));
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const WalletOutput &left, const WalletOutput &right) {
        return amountToNanogrin(left.amount) < amountToNanogrin(right.amount);
    });

    quint64 runningTotal = 0;
    for (int i = candidates.size() - 1; i >= 0; --i) {
        result.selectedOutputs.prepend(candidates.at(i));
        runningTotal += amountToNanogrin(candidates.at(i).amount);
        result.fee = estimateFee(result.selectedOutputs.size(), 2, 1);
        if (runningTotal >= amount + result.fee) {
            result.totalSelected = runningTotal;
            result.change = runningTotal - amount - result.fee;
            result.success = true;
            return result;
        }
    }

    result.error = QStringLiteral("Insufficient spendable outputs.");
    return result;
}
