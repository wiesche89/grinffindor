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

bool isBetterCandidate(const WalletSelection::Result &candidate,
                       const WalletSelection::Result &best)
{
    if (!candidate.success) {
        return false;
    }
    if (!best.success) {
        return true;
    }
    if (candidate.change == 0 && best.change != 0) {
        return true;
    }
    if (candidate.change != 0 && best.change == 0) {
        return false;
    }
    if (candidate.selectedOutputs.size() != best.selectedOutputs.size()) {
        return candidate.selectedOutputs.size() < best.selectedOutputs.size();
    }
    if (candidate.change != best.change) {
        return candidate.change < best.change;
    }
    if (candidate.totalSelected != best.totalSelected) {
        return candidate.totalSelected < best.totalSelected;
    }
    return candidate.fee < best.fee;
}

WalletSelection::Result buildCandidate(const QList<WalletOutput> &selectedOutputs,
                                       quint64 amount)
{
    WalletSelection::Result result;
    result.selectedOutputs = selectedOutputs;
    result.amount = amount;

    quint64 totalSelected = 0;
    for (int i = 0; i < selectedOutputs.size(); ++i) {
        totalSelected += amountToNanogrin(selectedOutputs.at(i).amount);
    }
    result.totalSelected = totalSelected;

    if (selectedOutputs.isEmpty()) {
        result.error = QStringLiteral("No spendable outputs selected.");
        return result;
    }

    const quint64 feeWithoutChange = WalletSelection::estimateFee(selectedOutputs.size(), 1, 1);
    if (totalSelected == amount + feeWithoutChange) {
        result.fee = feeWithoutChange;
        result.change = 0;
        result.success = true;
        return result;
    }

    const quint64 feeWithChange = WalletSelection::estimateFee(selectedOutputs.size(), 2, 1);
    if (totalSelected >= amount + feeWithChange) {
        result.fee = feeWithChange;
        result.change = totalSelected - amount - feeWithChange;
        result.success = true;
        return result;
    }

    result.error = QStringLiteral("Insufficient spendable outputs.");
    return result;
}

void considerAccumulatedCandidates(const QList<WalletOutput> &orderedCandidates,
                                   quint64 amount,
                                   WalletSelection::Result *bestResult)
{
    if (!bestResult) {
        return;
    }

    QList<WalletOutput> currentSelection;
    quint64 runningTotal = 0;
    for (int i = 0; i < orderedCandidates.size(); ++i) {
        currentSelection.append(orderedCandidates.at(i));
        runningTotal += amountToNanogrin(orderedCandidates.at(i).amount);

        const quint64 minimumPossibleFee =
            WalletSelection::estimateFee(currentSelection.size(), 1, 1);
        if (runningTotal < amount + minimumPossibleFee) {
            continue;
        }

        const WalletSelection::Result candidate = buildCandidate(currentSelection, amount);
        if (isBetterCandidate(candidate, *bestResult)) {
            *bestResult = candidate;
        }
    }
}

}

quint64 WalletSelection::estimateFee(int numInputs, int numOutputs, int numKernels)
{
    const quint64 weight = static_cast<quint64>(numInputs) * 1ULL
                         + static_cast<quint64>(numOutputs) * 21ULL
                         + static_cast<quint64>(numKernels) * 3ULL;
    return weight * 500000ULL;
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

    for (int i = 0; i < candidates.size(); ++i) {
        const QList<WalletOutput> singleCandidate(1, candidates.at(i));
        const Result candidate = buildCandidate(singleCandidate, amount);
        if (isBetterCandidate(candidate, result)) {
            result = candidate;
        }
    }

    QList<WalletOutput> descendingCandidates = candidates;
    std::reverse(descendingCandidates.begin(), descendingCandidates.end());
    considerAccumulatedCandidates(descendingCandidates, amount, &result);
    considerAccumulatedCandidates(candidates, amount, &result);

    if (result.success) {
        return result;
    }

    result.error = QStringLiteral("Insufficient spendable outputs.");
    return result;
}
