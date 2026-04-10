#include "wallettxbuilder.h"

#include <QJsonDocument>
#include <QStringList>
#include <algorithm>

#include "blindingfactor.h"
#include "input.h"
#include "output.h"
#include "outputfeatures.h"
#include "transactionbody.h"
#include "txkernel.h"
#include "walletcryptobackend.h"

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
 * @brief Maps an output coinbase flag to the canonical feature name.
 * @param coinbase True when the output uses coinbase features.
 * @return Feature string expected by serialization helpers.
 */
QString outputFeatureString(bool coinbase)
{
    return coinbase ? QStringLiteral("Coinbase") : QStringLiteral("Plain");
}

/**
 * @brief Checks whether a list of outputs already contains a commitment.
 * @param outputs Outputs to inspect.
 * @param commitment Commitment to locate.
 * @return True when the commitment exists in the list.
 */
bool containsOutputCommitment(const QVector<Output> &outputs, const QString &commitment)
{
    for (const Output &output : outputs) {
        if (output.commit() == commitment) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks whether a list of inputs already contains a commitment.
 * @param inputs Inputs to inspect.
 * @param commitment Commitment to locate.
 * @return True when the commitment exists in the list.
 */
bool containsInputCommitment(const QVector<Input> &inputs, const QString &commitment)
{
    for (const Input &input : inputs) {
        if (input.commit().hex() == commitment) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Produces a deterministic sort key for output canonicalization.
 * @param output Output instance to hash.
 * @return Canonical output ordering key.
 */
QString outputSortKeyForCanonicalization(const Output &output)
{
    return WalletCryptoBackend::outputOrderHash(output);
}

/**
 * @brief Compares two inputs by canonical wallet ordering.
 * @param left Left input.
 * @param right Right input.
 * @return True when the left input must appear before the right input.
 */
bool inputCanonicalLessThan(const Input &left, const Input &right)
{
    return WalletCryptoBackend::inputOrderHash(left) < WalletCryptoBackend::inputOrderHash(right);
}

/**
 * @brief Compares two outputs by canonical wallet ordering.
 * @param left Left output.
 * @param right Right output.
 * @return True when the left output must appear before the right output.
 */
bool outputCanonicalLessThan(const Output &left, const Output &right)
{
    return outputSortKeyForCanonicalization(left) < outputSortKeyForCanonicalization(right);
}

/**
 * @brief Deduplicates and canonicalizes transaction body inputs and outputs.
 * @param body Transaction body to normalize in place.
 */
void canonicalizeBody(TransactionBody *body)
{
    if (!body) {
        return;
    }

    QVector<Input> inputs = body->inputs();
    QVector<Output> outputs = body->outputs();

    // -------------------------------------------------------------------------------------------------------
    // Deduplicating Inputs And Outputs By Commitment
    // -------------------------------------------------------------------------------------------------------
    QVector<Input> uniqueInputs;
    for (const Input &input : inputs) {
        const QString commitHex = input.commit().hex();
        if (commitHex.isEmpty() || containsInputCommitment(uniqueInputs, commitHex)) {
            continue;
        }
        uniqueInputs.append(input);
    }

    QVector<Output> uniqueOutputs;
    for (const Output &output : outputs) {
        const QString commitHex = output.commit().trimmed();
        if (commitHex.isEmpty() || containsOutputCommitment(uniqueOutputs, commitHex)) {
            continue;
        }
        uniqueOutputs.append(output);
    }

    std::sort(uniqueInputs.begin(), uniqueInputs.end(), inputCanonicalLessThan);
    std::sort(uniqueOutputs.begin(), uniqueOutputs.end(), outputCanonicalLessThan);

    body->setInputs(uniqueInputs);
    body->setOutputs(uniqueOutputs);
}

/**
 * @brief Finalizes a transaction skeleton by adding kernel data and validating sums/signatures.
 * @param slate Slate V4 payload used for fee and signature metadata.
 * @param tx Partially assembled transaction.
 * @return Final build result with validation status.
 */
WalletTxBuilder::BuildResult finalizeBuildResult(const SlateV4 &slate, Transaction tx)
{
    WalletTxBuilder::BuildResult result;

    // -------------------------------------------------------------------------------------------------------
    // Building Kernel Data
    // -------------------------------------------------------------------------------------------------------
    QString excessError;
    const QString excessCommitment = WalletCryptoBackend::calculateExcessCommitment(slate, &excessError);
    if (excessCommitment.isEmpty()) {
        result.error = excessError.isEmpty()
            ? QStringLiteral("Failed to calculate kernel excess commitment.")
            : excessError;
        return result;
    }

    QVector<TxKernel> kernels;
    TxKernel kernel;
    kernel.setFeatures(QStringLiteral("Plain"));
    kernel.setFee(amountToNanogrin(slate.fee));
    kernel.setExcess(excessCommitment);
    kernel.setExcessSig(slate.metadata.value(QStringLiteral("final_sig")).toString());
    kernels.append(kernel);

    TransactionBody body = tx.body();
    body.setKernels(kernels);
    tx.setBody(body);

    // -------------------------------------------------------------------------------------------------------
    // Validating Transaction Kernels
    // -------------------------------------------------------------------------------------------------------
    QString validationError;
    if (!WalletCryptoBackend::validateTransactionKernelSums(tx, &validationError)) {
        result.error = validationError.isEmpty()
            ? QStringLiteral("Transaction kernel validation failed.")
            : validationError;
        return result;
    }
    if (!WalletCryptoBackend::validateTransactionKernelSignatures(tx, &validationError)) {
        result.error = validationError.isEmpty()
            ? QStringLiteral("Transaction kernel signature validation failed.")
            : validationError;
        return result;
    }

    result.transaction = tx;
    result.transactionJson = QString::fromUtf8(QJsonDocument(tx.toJson()).toJson(QJsonDocument::Indented));
    result.success = true;
    return result;
}

}

/**
 * @brief Builds a transaction skeleton from selected wallet outputs.
 * @param slate Slate V4 payload containing transaction metadata.
 * @param selectedInputs Selected spendable outputs used as inputs.
 * @param receiverOutput Receiver output candidate.
 * @param changeOutput Optional change output candidate.
 * @return Build result containing a canonicalized transaction skeleton.
 */
WalletTxBuilder::BuildResult WalletTxBuilder::buildTransactionSkeleton(const SlateV4 &slate,
                                                                       const QList<WalletOutput> &selectedInputs,
                                                                       const WalletOutput *receiverOutput,
                                                                       const WalletOutput *changeOutput)
{
    BuildResult result;

    if (selectedInputs.isEmpty()) {
        result.error = QStringLiteral("No selected inputs available for transaction build.");
        return result;
    }

    if (!receiverOutput || receiverOutput->commitment.isEmpty()) {
        result.error = QStringLiteral("Receiver output is missing for transaction build.");
        return result;
    }

    // -------------------------------------------------------------------------------------------------------
    // Building Inputs And Outputs
    // -------------------------------------------------------------------------------------------------------
    TransactionBody body;

    QVector<Input> inputs;
    for (int i = 0; i < selectedInputs.size(); ++i) {
        Commitment commit;
        commit.setHex(selectedInputs.at(i).commitment);
        Input input(selectedInputs.at(i).coinbase ? OutputFeatures::Coinbase : OutputFeatures::Plain, commit);
        inputs.append(input);
    }
    body.setInputs(inputs);

    QVector<Output> outputs;
    outputs.append(Output(receiverOutput->commitment,
                          outputFeatureString(receiverOutput->coinbase),
                          receiverOutput->proof));
    if (changeOutput && !changeOutput->commitment.isEmpty()) {
        outputs.append(Output(changeOutput->commitment,
                              outputFeatureString(changeOutput->coinbase),
                              changeOutput->proof));
    }
    body.setOutputs(outputs);
    canonicalizeBody(&body);

    // -------------------------------------------------------------------------------------------------------
    // Assembling Transaction Envelope
    // -------------------------------------------------------------------------------------------------------
    Transaction tx;
    tx.setTxId(slate.id);
    BlindingFactor offset;
    offset.setHex(slate.offset);
    tx.setOffset(offset);
    tx.setBody(body);

    return finalizeBuildResult(slate, tx);
}

/**
 * @brief Builds a transaction skeleton from compact slate commitments.
 * @param slate Slate V4 payload containing commitment entries.
 * @param receiverOutput Optional receiver output appended when not present.
 * @return Build result containing a canonicalized transaction skeleton.
 */
WalletTxBuilder::BuildResult WalletTxBuilder::buildTransactionSkeletonFromCommitments(const SlateV4 &slate,
                                                                                      const WalletOutput *receiverOutput)
{
    BuildResult result;

    if (slate.commitments.isEmpty()) {
        result.error = QStringLiteral("No commitments available for transaction build.");
        return result;
    }

    TransactionBody body;
    QVector<Input> inputs;
    QVector<Output> outputs;

    // -------------------------------------------------------------------------------------------------------
    // Mapping Compact Commitments To Inputs And Outputs
    // -------------------------------------------------------------------------------------------------------
    for (int i = 0; i < slate.commitments.size(); ++i) {
        const SlateV4::Commit &commit = slate.commitments.at(i);
        if (commit.commitment.trimmed().isEmpty()) {
            continue;
        }

        if (commit.proof.trimmed().isEmpty()) {
            Commitment inputCommit;
            inputCommit.setHex(commit.commitment);
            inputs.append(Input(commit.feature == 1 ? OutputFeatures::Coinbase : OutputFeatures::Plain,
                                inputCommit));
        } else {
            outputs.append(Output(commit.commitment,
                                  commit.feature == 1 ? QStringLiteral("Coinbase") : QStringLiteral("Plain"),
                                  commit.proof));
        }
    }

    if (receiverOutput

        && !receiverOutput->commitment.isEmpty()
        && !containsOutputCommitment(outputs, receiverOutput->commitment)) {
        outputs.append(Output(receiverOutput->commitment,
                              outputFeatureString(receiverOutput->coinbase),
                              receiverOutput->proof));
    }

    if (inputs.isEmpty()) {
        result.error = QStringLiteral("No inputs available in compact slate commitments.");
        return result;
    }
    if (outputs.isEmpty()) {
        result.error = QStringLiteral("No outputs available in compact slate commitments.");
        return result;
    }

    body.setInputs(inputs);
    body.setOutputs(outputs);
    canonicalizeBody(&body);

    // -------------------------------------------------------------------------------------------------------
    // Assembling Transaction Envelope
    // -------------------------------------------------------------------------------------------------------
    Transaction tx;
    tx.setTxId(slate.id);
    BlindingFactor offset;
    offset.setHex(slate.offset);
    tx.setOffset(offset);
    tx.setBody(body);

    return finalizeBuildResult(slate, tx);
}
