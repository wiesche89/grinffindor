#include "wallettxbuilder.h"

#include <QJsonDocument>
#include <QStringList>

#include "blindingfactor.h"
#include "input.h"
#include "output.h"
#include "outputfeatures.h"
#include "transactionbody.h"
#include "txkernel.h"
#include "walletcryptobackend.h"

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

QString outputFeatureString(bool coinbase)
{
    return coinbase ? QStringLiteral("Coinbase") : QStringLiteral("Plain");
}

bool containsOutputCommitment(const QVector<Output> &outputs, const QString &commitment)
{
    for (const Output &output : outputs) {
        if (output.commit() == commitment) {
            return true;
        }
    }
    return false;
}

WalletTxBuilder::BuildResult finalizeBuildResult(const SlateV4 &slate, Transaction tx)
{
    WalletTxBuilder::BuildResult result;

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

    result.transaction = tx;
    result.transactionJson = QString::fromUtf8(QJsonDocument(tx.toJson()).toJson(QJsonDocument::Indented));
    result.success = true;
    return result;
}

}

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

    Transaction tx;
    tx.setTxId(slate.id);
    BlindingFactor offset;
    offset.setHex(slate.offset);
    tx.setOffset(offset);
    tx.setBody(body);

    return finalizeBuildResult(slate, tx);
}

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

    Transaction tx;
    tx.setTxId(slate.id);
    BlindingFactor offset;
    offset.setHex(slate.offset);
    tx.setOffset(offset);
    tx.setBody(body);

    return finalizeBuildResult(slate, tx);
}
