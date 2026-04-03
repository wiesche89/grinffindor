#include "walletcryptokernelhelpers.h"

#include "walletblake2b.h"
#include "walletcryptoaggsighelpers.h"
#include "walletcryptobasehelpers.h"
#include "walletcryptocommitmenthelpers.h"
#include "walletcryptosecphelpers.h"

#include "../submodules/grin-node-api/src/attributes/transaction.h"

#include <QStringList>
#include <algorithm>
#include <limits>

namespace
{

QString inputSortKey(const Input &input)
{
    return WalletCryptoKernelHelpers::inputOrderHash(input);
}

QString outputSortKey(const Output &output)
{
    return WalletCryptoKernelHelpers::outputOrderHash(output);
}

QString kernelSortKey(const TxKernel &kernel)
{
    return WalletCryptoKernelHelpers::kernelOrderHash(kernel);
}

template <typename Item>
bool verifySortedAndUnique(const QVector<Item> &items,
                           QString (*keyFn)(const Item &),
                           const QString &label,
                           QString *errorOut)
{
    QString previousKey;
    for (int i = 0; i < items.size(); ++i) {
        const QString key = keyFn(items.at(i));
        if (key.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: empty %1 entry at index %2.")
                                .arg(label, QString::number(i));
            }
            return false;
        }
        if (i > 0) {
            if (key < previousKey) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Transaction body validation failed: %1s are not sorted at index %2.")
                                    .arg(label, QString::number(i));
                }
                return false;
            }
            if (key == previousKey) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Transaction body validation failed: duplicate %1 at index %2.")
                                    .arg(label, QString::number(i));
                }
                return false;
            }
        }
        previousKey = key;
    }
    return true;
}

}

namespace WalletCryptoKernelHelpers
{

QString inputOrderHash(const Input &input)
{
    QByteArray serialized;
    WalletCryptoHelpers::appendU8(serialized, static_cast<quint8>(input.features()));
    if (!WalletCryptoHelpers::appendFixedHexBytes(&serialized, input.commit().hex(), 33)) {
        return QString();
    }

    return QString::fromUtf8(WalletBlake2b::hash256(serialized).toHex());
}

QString outputOrderHash(const Output &output)
{
    QByteArray serialized;
    if (!WalletCryptoHelpers::appendOutputFeatureForOrdering(&serialized, output.features())) {
        return QString();
    }
    if (!WalletCryptoHelpers::appendFixedHexBytes(&serialized, output.commit(), 33)) {
        return QString();
    }

    return QString::fromUtf8(WalletBlake2b::hash256(serialized).toHex());
}

QString kernelOrderHash(const TxKernel &kernel)
{
    QByteArray serialized;
    if (!WalletCryptoHelpers::appendKernelFeaturesForOrdering(&serialized, kernel)) {
        return QString();
    }
    if (!WalletCryptoHelpers::appendFixedHexBytes(&serialized, kernel.excess(), 33)) {
        return QString();
    }
    if (!WalletCryptoHelpers::appendFixedHexBytes(&serialized, kernel.excessSig(), 64)) {
        return QString();
    }

    return QString::fromUtf8(WalletBlake2b::hash256(serialized).toHex());
}

bool validateTransactionKernelSums(const Transaction &tx, QString *errorOut)
{
    const TransactionBody body = tx.body();
    const QVector<Input> inputs = body.inputs();
    const QVector<Output> outputs = body.outputs();
    const QVector<TxKernel> kernels = body.kernels();

    if (inputs.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction kernel validation failed: no inputs.");
        }
        return false;
    }
    if (outputs.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction kernel validation failed: no outputs.");
        }
        return false;
    }
    if (kernels.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction kernel validation failed: no kernels.");
        }
        return false;
    }

    QVector<secp256k1_pedersen_commitment> positiveCommitments;
    QVector<secp256k1_pedersen_commitment> negativeCommitments;
    positiveCommitments.reserve(outputs.size());
    negativeCommitments.reserve(inputs.size() + kernels.size() + 1);

    secp256k1_scratch_space *scratch =
        secp256k1_scratch_space_create(WalletCryptoHelpers::walletSecpContext(), 8 * 1024 * 1024);
    if (!scratch) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction kernel validation failed: could not allocate secp scratch space.");
        }
        return false;
    }

    QString detailError;
    for (int i = 0; i < outputs.size(); ++i) {
        const Output &output = outputs.at(i);
        secp256k1_pedersen_commitment commitment;
        if (!WalletCryptoHelpers::parseCommitmentHex(output.commit(), &commitment)) {
            detailError = QStringLiteral("Transaction kernel validation failed: invalid output commitment %1.")
                              .arg(output.commit().left(16));
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        QString proofError;
        if (!WalletCryptoHelpers::verifyOutputRangeproof(output, scratch, &proofError)) {
            detailError = QStringLiteral("Transaction kernel validation failed for output %1: %2")
                              .arg(output.commit().left(16), proofError);
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        positiveCommitments.append(commitment);
    }

    for (int i = 0; i < inputs.size(); ++i) {
        secp256k1_pedersen_commitment commitment;
        if (!WalletCryptoHelpers::parseCommitmentHex(inputs.at(i).commit().hex(), &commitment)) {
            detailError = QStringLiteral("Transaction kernel validation failed: invalid input commitment %1.")
                              .arg(inputs.at(i).commit().hex().left(16));
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        negativeCommitments.append(commitment);
    }

    for (int i = 0; i < kernels.size(); ++i) {
        secp256k1_pedersen_commitment commitment;
        if (!WalletCryptoHelpers::parseCommitmentHex(kernels.at(i).excess(), &commitment)) {
            detailError = QStringLiteral("Transaction kernel validation failed: invalid kernel excess %1.")
                              .arg(kernels.at(i).excess().left(16));
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        negativeCommitments.append(commitment);
    }

    quint64 totalFee = 0;
    for (int i = 0; i < kernels.size(); ++i) {
        const quint64 fee = static_cast<quint64>(kernels.at(i).fee());
        if (fee > 0 && totalFee > std::numeric_limits<quint64>::max() - fee) {
            detailError = QStringLiteral("Transaction kernel validation failed: kernel fee overflow.");
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        totalFee += fee;
    }
    if (totalFee > 0) {
        secp256k1_pedersen_commitment feeCommitment;
        if (!WalletCryptoHelpers::buildValueOnlyCommitment(totalFee, &feeCommitment)) {
            detailError = QStringLiteral("Transaction kernel validation failed: could not build fee commitment.");
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        positiveCommitments.append(feeCommitment);
    }

    const QString offsetHex = tx.offset().hex().trimmed();
    if (!offsetHex.isEmpty() && offsetHex != QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000")) {
        secp256k1_pedersen_commitment offsetCommitment;
        if (!WalletCryptoHelpers::buildZeroValueCommitment(offsetHex, &offsetCommitment)) {
            detailError = QStringLiteral("Transaction kernel validation failed: invalid offset %1.")
                              .arg(offsetHex.left(16));
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = detailError;
            }
            return false;
        }
        negativeCommitments.append(offsetCommitment);
    }

    QVector<const secp256k1_pedersen_commitment *> positivePointers;
    QVector<const secp256k1_pedersen_commitment *> negativePointers;
    positivePointers.reserve(positiveCommitments.size());
    negativePointers.reserve(negativeCommitments.size());
    for (int i = 0; i < positiveCommitments.size(); ++i) {
        positivePointers.append(&positiveCommitments[i]);
    }
    for (int i = 0; i < negativeCommitments.size(); ++i) {
        negativePointers.append(&negativeCommitments[i]);
    }

    secp256k1_pedersen_commitment emptyCommitment;
    const secp256k1_pedersen_commitment *emptyPointer = &emptyCommitment;
    const secp256k1_pedersen_commitment * const *positivePointerData =
        positivePointers.isEmpty() ? &emptyPointer : positivePointers.constData();
    const secp256k1_pedersen_commitment * const *negativePointerData =
        negativePointers.isEmpty() ? &emptyPointer : negativePointers.constData();

    const int tallyOk = secp256k1_pedersen_verify_tally(
        WalletCryptoHelpers::walletSecpContext(),
        positivePointerData,
        static_cast<size_t>(positivePointers.size()),
        negativePointerData,
        static_cast<size_t>(negativePointers.size()));

    if (tallyOk != 1) {
        secp256k1_pedersen_commitment positiveSum;
        secp256k1_pedersen_commitment negativeSum;
        QString positiveHex;
        QString negativeHex;
        if (secp256k1_pedersen_commit_sum(WalletCryptoHelpers::walletSecpContext(),
                                          &positiveSum,
                                          positivePointerData,
                                          static_cast<size_t>(positivePointers.size()),
                                          &emptyPointer,
                                          0) == 1) {
            positiveHex = WalletCryptoHelpers::serializeCommitment(positiveSum);
        }
        if (secp256k1_pedersen_commit_sum(WalletCryptoHelpers::walletSecpContext(),
                                          &negativeSum,
                                          negativePointerData,
                                          static_cast<size_t>(negativePointers.size()),
                                          &emptyPointer,
                                          0) == 1) {
            negativeHex = WalletCryptoHelpers::serializeCommitment(negativeSum);
        }

        detailError = QStringLiteral("Transaction kernel validation failed: outputs (+fee) do not balance inputs + kernels + offset. outputs=%1 inputs=%2 kernels=%3 totalFee=%4 offset=%5 posSum=%6 negSum=%7")
                          .arg(QString::number(outputs.size()),
                               QString::number(inputs.size()),
                               QString::number(kernels.size()),
                               QString::number(totalFee),
                               offsetHex.left(16),
                               positiveHex.left(16),
                               negativeHex.left(16));
        secp256k1_scratch_space_destroy(scratch);
        if (errorOut) {
            *errorOut = detailError;
        }
        return false;
    }

    secp256k1_scratch_space_destroy(scratch);
    return true;
}

bool validateTransactionBody(const Transaction &tx, QString *errorOut)
{
    const TransactionBody body = tx.body();
    const QVector<Input> inputs = body.inputs();
    const QVector<Output> outputs = body.outputs();
    const QVector<TxKernel> kernels = body.kernels();

    if (inputs.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction body validation failed: no inputs.");
        }
        return false;
    }
    if (outputs.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction body validation failed: no outputs.");
        }
        return false;
    }
    if (kernels.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction body validation failed: no kernels.");
        }
        return false;
    }

    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).features().trimmed() == QStringLiteral("Coinbase")) {
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: coinbase output at index %1.")
                                .arg(i);
            }
            return false;
        }
    }

    for (int i = 0; i < kernels.size(); ++i) {
        if (kernels.at(i).features().trimmed() == QStringLiteral("Coinbase")) {
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: coinbase kernel at index %1.")
                                .arg(i);
            }
            return false;
        }
    }

    if (!verifySortedAndUnique(inputs, inputSortKey, QStringLiteral("input"), errorOut)) {
        return false;
    }
    if (!verifySortedAndUnique(outputs, outputSortKey, QStringLiteral("output"), errorOut)) {
        return false;
    }
    if (!verifySortedAndUnique(kernels, kernelSortKey, QStringLiteral("kernel"), errorOut)) {
        return false;
    }

    QStringList commitments;
    commitments.reserve(inputs.size() + outputs.size());
    for (int i = 0; i < inputs.size(); ++i) {
        commitments.append(inputs.at(i).commit().hex());
    }
    for (int i = 0; i < outputs.size(); ++i) {
        commitments.append(outputs.at(i).commit().trimmed());
    }
    std::sort(commitments.begin(), commitments.end());
    for (int i = 1; i < commitments.size(); ++i) {
        if (!commitments.at(i).isEmpty() && commitments.at(i) == commitments.at(i - 1)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: cut-through detected for commitment %1.")
                                .arg(commitments.at(i).left(16));
            }
            return false;
        }
    }

    if (!WalletCryptoHelpers::verifyOutputsBatchRangeproofs(outputs, errorOut)) {
        return false;
    }

    return true;
}

bool validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut)
{
    const QVector<TxKernel> kernels = tx.body().kernels();
    if (kernels.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction kernel signature validation failed: no kernels.");
        }
        return false;
    }

    for (int i = 0; i < kernels.size(); ++i) {
        const TxKernel &kernel = kernels.at(i);

        secp256k1_pedersen_commitment excessCommitment;
        if (!WalletCryptoHelpers::parseCommitmentHex(kernel.excess(), &excessCommitment)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Kernel signature validation failed: invalid excess at index %1.").arg(i);
            }
            return false;
        }

        secp256k1_pubkey excessPubkey;
        if (secp256k1_pedersen_commitment_to_pubkey(WalletCryptoHelpers::walletSecpContext(),
                                                    &excessPubkey,
                                                    &excessCommitment) != 1) {
            if (errorOut) {
                *errorOut = QStringLiteral("Kernel signature validation failed: excess is not convertible to pubkey at index %1.").arg(i);
            }
            return false;
        }

        const QByteArray compactSignature = WalletCryptoHelpers::fromHex(kernel.excessSig().trimmed());
        if (compactSignature.size() != 64) {
            if (errorOut) {
                *errorOut = QStringLiteral("Kernel signature validation failed: invalid signature length at index %1.").arg(i);
            }
            return false;
        }
        QByteArray rawSignature;
        if (!WalletCryptoHelpers::kernelSigCompactToRaw(compactSignature, &rawSignature)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Kernel signature validation failed: invalid signature format at index %1.").arg(i);
            }
            return false;
        }

        QString messageError;
        const QByteArray messageHash = WalletCryptoHelpers::buildKernelSignatureMessageForFeature(
            kernel.features(),
            static_cast<quint64>(kernel.fee()),
            0,
            &messageError);
        if (messageHash.size() != 32) {
            if (errorOut) {
                *errorOut = messageError.isEmpty()
                    ? QStringLiteral("Kernel signature validation failed: invalid message at index %1.").arg(i)
                    : messageError;
            }
            return false;
        }

        if (secp256k1_aggsig_verify_single(WalletCryptoHelpers::walletSecpContext(),
                                           reinterpret_cast<const unsigned char *>(rawSignature.constData()),
                                           reinterpret_cast<const unsigned char *>(messageHash.constData()),
                                           0,
                                           &excessPubkey,
                                           &excessPubkey,
                                           0,
                                           0) != 1) {
            if (errorOut) {
                *errorOut = QStringLiteral("Kernel signature validation failed at index %1.").arg(i);
            }
            return false;
        }
    }

    return true;
}

}
