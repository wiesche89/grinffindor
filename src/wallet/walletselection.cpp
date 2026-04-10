#include "walletselection.h"

#include <algorithm>
#include <QStringList>

namespace {

/**
 * @brief Converts a decimal GRIN amount string into nanogrin.
 * @param amount Decimal amount string.
 * @return Amount represented in nanogrin.
 */
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

/**
 * @brief Evaluates whether a wallet output can currently be spent.
 * @param output Output candidate.
 * @param chainHeight Current chain height.
 * @return True when the output is mature and not blocked by state flags.
 */
bool isSpendable(const WalletOutput &output, qulonglong chainHeight, qulonglong minimumConfirmations = 10)
{
    if (output.spent || output.locked || !output.onChain || output.pending || output.reverted) {
        return false;
    }

    if (output.height > 0) {
        const qulonglong minConf = minimumConfirmations > 0 ? minimumConfirmations : 10;
        if (chainHeight > 0 && chainHeight < output.height + minConf - 1) {
            return false;
        }
        if (output.coinbase && chainHeight > 0 && chainHeight < output.height + 1000) {
            return false;
        }
    } else {
        // Fallback for payloads missing height:
        // - coinbase without height is never considered spendable.
        // - non-coinbase on-chain outputs are treated as spendable.
        if (output.coinbase) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Compares two selection results and returns whether candidate is preferable.
 * @param candidate Candidate selection result.
 * @param best Current best selection result.
 * @return True when candidate is a better selection.
 */
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

/**
 * @brief Orders outputs by ascending amount.
 * @param left Left output.
 * @param right Right output.
 * @return True when left amount is smaller than right amount.
 */
bool amountAscending(const WalletOutput &left, const WalletOutput &right)
{
    return amountToNanogrin(left.amount) < amountToNanogrin(right.amount);
}

/**
 * @brief Builds a selection candidate from chosen outputs and target amount.
 * @param selectedOutputs Outputs included in this candidate.
 * @param amount Requested send amount in nanogrin.
 * @return Candidate result including fee and change details.
 */
WalletSelection::Result buildCandidate(const QList<WalletOutput> &selectedOutputs,
                                       quint64 amount)
{
    WalletSelection::Result result;
    result.selectedOutputs = selectedOutputs;
    result.amount = amount;

    // -------------------------------------------------------------------------------------------------------
    // Calculating Total Selected Amount
    // -------------------------------------------------------------------------------------------------------
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

/**
 * @brief Evaluates accumulated combinations and updates the best result.
 * @param orderedCandidates Candidates in accumulation order.
 * @param amount Requested send amount in nanogrin.
 * @param bestResult In/out pointer to the currently best selection result.
 */
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

/**
 * @brief Estimates transaction fee from weighted input/output/kernel costs.
 * @param numInputs Number of transaction inputs.
 * @param numOutputs Number of transaction outputs.
 * @param numKernels Number of transaction kernels.
 * @return Estimated fee in nanogrin.
 */
quint64 WalletSelection::estimateFee(int numInputs, int numOutputs, int numKernels)
{
    const quint64 weight = static_cast<quint64>(numInputs) * 1ULL
                         + static_cast<quint64>(numOutputs) * 21ULL
                         + static_cast<quint64>(numKernels) * 3ULL;
    return weight * 500000ULL;
}

/**
 * @brief Selects the best spendable outputs for a target amount.
 * @param outputs Available wallet outputs.
 * @param amount Requested send amount in nanogrin.
 * @param chainHeight Current chain height used for maturity checks.
 * @return Selection result containing outputs, fee, and change.
 */
WalletSelection::Result WalletSelection::selectSpendableOutputs(const QList<WalletOutput> &outputs,
                                                                quint64 amount,
                                                                qulonglong chainHeight,
                                                                qulonglong minimumConfirmations)
{
    Result result;
    result.amount = amount;

    // -------------------------------------------------------------------------------------------------------
    // Filtering Spendable Candidates
    // -------------------------------------------------------------------------------------------------------
    QList<WalletOutput> candidates;
    for (int i = 0; i < outputs.size(); ++i) {
        if (isSpendable(outputs.at(i), chainHeight, minimumConfirmations)) {
            candidates.append(outputs.at(i));
        }
    }

    std::sort(candidates.begin(), candidates.end(), amountAscending);

    // -------------------------------------------------------------------------------------------------------
    // Evaluating Single And Accumulated Strategies
    // -------------------------------------------------------------------------------------------------------
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
