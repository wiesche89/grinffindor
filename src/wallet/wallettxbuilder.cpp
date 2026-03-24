#include "wallettxbuilder.h"

#include <QJsonDocument>
#include <QStringList>

#include "blindingfactor.h"
#include "input.h"
#include "output.h"
#include "outputfeatures.h"
#include "transactionbody.h"
#include "txkernel.h"

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

    QVector<TxKernel> kernels;
    TxKernel kernel;
    kernel.setFeatures(QStringLiteral("Plain"));
    kernel.setFee(amountToNanogrin(slate.fee));
    kernel.setExcess(slate.metadata.value(QStringLiteral("pubkey_total")).toString());
    kernel.setExcessSig(slate.metadata.value(QStringLiteral("final_sig")).toString());
    kernels.append(kernel);
    body.setKernels(kernels);

    Transaction tx;
    tx.setTxId(slate.id);
    BlindingFactor offset;
    offset.setHex(slate.offset);
    tx.setOffset(offset);
    tx.setBody(body);

    result.transaction = tx;
    result.transactionJson = QString::fromUtf8(QJsonDocument(tx.toJson()).toJson(QJsonDocument::Indented));
    result.success = true;
    return result;
}
