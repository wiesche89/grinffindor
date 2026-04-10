#include "walletcryptohelpers.h"

#include "walletblake2b.h"

#include "../submodules/grin-node-api/src/attributes/transaction.h"

#include <QCryptographicHash>
#include <cstring>

namespace
{

const size_t kBulletproofScratchSpaceSize = 8 * 1024 * 1024;

}

namespace WalletCryptoHelpers
{

/**
 * @brief Parses commitment hex.
 * @param hex
 * @param commitmentOut
 * @return
 */
bool parseCommitmentHex(const QString &hex, secp256k1_pedersen_commitment *commitmentOut)
{
    if (!commitmentOut) {
        return false;
    }

    const QByteArray bytes = fromHex(hex.trimmed());
    if (bytes.size() != 33) {
        return false;
    }

    return secp256k1_pedersen_commitment_parse(walletSecpContext(),
                                               commitmentOut,
                                               reinterpret_cast<const unsigned char *>(bytes.constData())) == 1;
}

/**
 * @brief Builds zero value commitment.
 * @param blindHex
 * @param commitmentOut
 * @return
 */
bool buildZeroValueCommitment(const QString &blindHex, secp256k1_pedersen_commitment *commitmentOut)
{
    if (!commitmentOut) {
        return false;
    }

    const QByteArray blind = fromHex(blindHex.trimmed());
    if (blind.size() != 32) {
        return false;
    }

    if (secp256k1_ec_seckey_verify(walletSecpContext(),
                                   reinterpret_cast<const unsigned char *>(blind.constData())) != 1) {
        return false;
    }

    return secp256k1_pedersen_commit(walletSecpContext(),
                                     commitmentOut,
                                     reinterpret_cast<const unsigned char *>(blind.constData()),
                                     0,
                                     &secp256k1_generator_const_h,
                                     &secp256k1_generator_const_g) == 1;
}

/**
 * @brief Builds value only commitment.
 * @param value
 * @param commitmentOut
 * @return
 */
bool buildValueOnlyCommitment(quint64 value, secp256k1_pedersen_commitment *commitmentOut)
{
    if (!commitmentOut) {
        return false;
    }

    unsigned char zeroBlind[32];
    std::memset(zeroBlind, 0, sizeof(zeroBlind));
    return secp256k1_pedersen_commit(walletSecpContext(),
                                     commitmentOut,
                                     zeroBlind,
                                     value,
                                     &secp256k1_generator_const_h,
                                     &secp256k1_generator_const_g) == 1;
}

/**
 * @brief Builds commitment.
 * @param commitment
 * @return
 */
QString serializeCommitment(const secp256k1_pedersen_commitment &commitment)
{
    unsigned char serialized[33];
    if (secp256k1_pedersen_commitment_serialize(walletSecpContext(), serialized, &commitment) != 1) {
        return QString();
    }
    return toHex(serialized, sizeof(serialized));
}

/**
 * @brief Appends fixed hex bytes to the target buffer.
 * @param serialized
 * @param hex
 * @param expectedSize
 * @return
 */
bool appendFixedHexBytes(QByteArray *serialized, const QString &hex, int expectedSize)
{
    if (!serialized) {
        return false;
    }

    const QByteArray bytes = fromHex(hex.trimmed());
    if (bytes.size() != expectedSize) {
        return false;
    }

    serialized->append(bytes);
    return true;
}

/**
 * @brief Appends output feature for ordering to the target buffer.
 * @param serialized
 * @param feature
 * @return
 */
bool appendOutputFeatureForOrdering(QByteArray *serialized, const QString &feature)
{
    if (!serialized) {
        return false;
    }

    const QString trimmed = feature.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("Plain")) {
        appendU8(*serialized, 0);
        return true;
    }
    if (trimmed == QStringLiteral("Coinbase")) {
        appendU8(*serialized, 1);
        return true;
    }

    return false;
}

/**
 * @brief Appends kernel features for ordering to the target buffer.
 * @param serialized
 * @param kernel
 * @return
 */
bool appendKernelFeaturesForOrdering(QByteArray *serialized, const TxKernel &kernel)
{
    if (!serialized) {
        return false;
    }

    const QString feature = kernel.features().trimmed();
    if (feature.isEmpty() || feature == QStringLiteral("Plain")) {
        appendU8(*serialized, 0);
        appendU64(*serialized, static_cast<quint64>(kernel.fee()));
        return true;
    }
    if (feature == QStringLiteral("Coinbase")) {
        appendU8(*serialized, 1);
        return true;
    }

    return false;
}

/**
 * @brief Validates outputs batch rangeproofs.
 * @param outputs
 * @param errorOut
 * @return
 */
bool verifyOutputsBatchRangeproofs(const QVector<Output> &outputs, QString *errorOut)
{
    if (outputs.isEmpty()) {
        return true;
    }

    secp256k1_scratch_space *scratch = secp256k1_scratch_space_create(walletSecpContext(), kBulletproofScratchSpaceSize);
    if (!scratch) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction body validation failed: could not allocate secp scratch space.");
        }
        return false;
    }

    QVector<secp256k1_pedersen_commitment> commitments;
    QVector<QByteArray> proofs;
    commitments.reserve(outputs.size());
    proofs.reserve(outputs.size());

    size_t proofLen = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const Output &output = outputs.at(i);
        secp256k1_pedersen_commitment commitment;
        if (!parseCommitmentHex(output.commit(), &commitment)) {
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: invalid output commitment at index %1.")
                                .arg(i);
            }
            return false;
        }

        const QByteArray proof = QByteArray::fromHex(output.proof().trimmed().toUtf8());
        if (proof.isEmpty()) {
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: missing output proof at index %1.")
                                .arg(i);
            }
            return false;
        }
        if (i == 0) {
            proofLen = static_cast<size_t>(proof.size());
        } else if (proof.size() != static_cast<int>(proofLen)) {
            secp256k1_scratch_space_destroy(scratch);
            if (errorOut) {
                *errorOut = QStringLiteral("Transaction body validation failed: inconsistent proof length at index %1.")
                                .arg(i);
            }
            return false;
        }

        commitments.append(commitment);
        proofs.append(proof);
    }

    QVector<const unsigned char *> proofPointers;
    QVector<const secp256k1_pedersen_commitment *> commitmentPointers;
    QVector<secp256k1_generator> valueGenerators;
    proofPointers.reserve(proofs.size());
    commitmentPointers.reserve(commitments.size());
    valueGenerators.reserve(proofs.size());
    for (int i = 0; i < proofs.size(); ++i) {
        proofPointers.append(reinterpret_cast<const unsigned char *>(proofs.at(i).constData()));
        commitmentPointers.append(&commitments[i]);
        valueGenerators.append(secp256k1_generator_const_h);
    }

    const int verifyOk = secp256k1_bulletproof_rangeproof_verify_multi(
        walletSecpContext(),
        scratch,
        walletBulletproofGenerators(),
        proofPointers.constData(),
        static_cast<size_t>(proofPointers.size()),
        proofLen,
        0,
        commitmentPointers.constData(),
        1,
        64,
        valueGenerators.constData(),
        0,
        0);

    secp256k1_scratch_space_destroy(scratch);

    if (verifyOk != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Transaction body validation failed: batch rangeproof verification failed.");
        }
        return false;
    }
    return true;
}

/**
 * @brief Validates output rangeproof.
 * @param output
 * @param scratch
 * @param errorOut
 * @return
 */
bool verifyOutputRangeproof(const Output &output,
                            secp256k1_scratch_space *scratch,
                            QString *errorOut)
{
    secp256k1_pedersen_commitment commitment;
    if (!parseCommitmentHex(output.commit(), &commitment)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to parse output commitment.");
        }
        return false;
    }

    const QByteArray proof = QByteArray::fromHex(output.proof().trimmed().toUtf8());
    if (proof.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output proof is missing.");
        }
        return false;
    }

    if (secp256k1_bulletproof_rangeproof_verify(walletSecpContext(),
                                                scratch,
                                                walletBulletproofGenerators(),
                                                reinterpret_cast<const unsigned char *>(proof.constData()),
                                                static_cast<size_t>(proof.size()),
                                                0,
                                                &commitment,
                                                1,
                                                64,
                                                &secp256k1_generator_const_h,
                                                0,
                                                0) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Rangeproof verification failed.");
        }
        return false;
    }

    return true;
}

/**
 * @brief Builds commitment and rangeproof.
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @param amount
 * @param commitOut
 * @return
 */
bool createCommitmentAndRangeproof(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag,
                                   const QString &amount,
                                   SlateV4::Commit *commitOut)
{
    if (!commitOut) {
        return false;
    }

    secp256k1_context *context = walletSecpContext();
    secp256k1_bulletproof_generators *generators = walletBulletproofGenerators();
    if (!context || !generators) {
        return false;
    }

    const quint64 value = amountToNanogrin(amount);
    const QByteArray blind = deriveSigningBaseSecret(walletFingerprint, workflowId, roleTag);
    const QByteArray privateNonce = deriveValidSecretBytes(QStringLiteral("proof-private"),
                                                           walletFingerprint,
                                                           workflowId + QLatin1Char(':') + roleTag);
    const QByteArray rewindNonce = deriveValidSecretBytes(QStringLiteral("proof-rewind"),
                                                          walletFingerprint,
                                                          workflowId + QLatin1Char(':') + roleTag);
    const QByteArray proofMessage = QCryptographicHash::hash(
        QStringLiteral("%1:%2:%3:%4").arg(walletFingerprint, workflowId, roleTag, amount).toUtf8(),
        QCryptographicHash::Blake2b_256).left(20);

    if (blind.size() != 32 || privateNonce.size() != 32 || rewindNonce.size() != 32 || proofMessage.size() != 20) {
        return false;
    }

    secp256k1_pedersen_commitment commitment;
    if (secp256k1_pedersen_commit(context,
                                  &commitment,
                                  reinterpret_cast<const unsigned char *>(blind.constData()),
                                  value,
                                  &secp256k1_generator_const_h,
                                  &secp256k1_generator_const_g) != 1) {
        return false;
    }

    unsigned char serializedCommitment[33];
    if (secp256k1_pedersen_commitment_serialize(context, serializedCommitment, &commitment) != 1) {
        return false;
    }

    QByteArray proof(SECP256K1_BULLETPROOF_MAX_PROOF, Qt::Uninitialized);
    size_t proofLen = static_cast<size_t>(proof.size());
    const uint64_t values[1] = { value };
    const unsigned char *blinds[1] = {
        reinterpret_cast<const unsigned char *>(blind.constData())
    };

    secp256k1_scratch_space *scratch = secp256k1_scratch_space_create(context, kBulletproofScratchSpaceSize);
    if (!scratch) {
        return false;
    }

    const int proved = secp256k1_bulletproof_rangeproof_prove(
        context,
        scratch,
        generators,
        reinterpret_cast<unsigned char *>(proof.data()),
        &proofLen,
        0,
        0,
        0,
        values,
        0,
        blinds,
        0,
        1,
        &secp256k1_generator_const_h,
        64,
        reinterpret_cast<const unsigned char *>(rewindNonce.constData()),
        reinterpret_cast<const unsigned char *>(privateNonce.constData()),
        0,
        0,
        reinterpret_cast<const unsigned char *>(proofMessage.constData()));

    if (proved != 1) {
        secp256k1_scratch_space_destroy(scratch);
        return false;
    }

    const int verified = secp256k1_bulletproof_rangeproof_verify(
        context,
        scratch,
        generators,
        reinterpret_cast<const unsigned char *>(proof.constData()),
        proofLen,
        0,
        &commitment,
        1,
        64,
        &secp256k1_generator_const_h,
        0,
        0);

    secp256k1_scratch_space_destroy(scratch);
    if (verified != 1) {
        return false;
    }

    proof.resize(static_cast<int>(proofLen));
    commitOut->feature = 0;
    commitOut->commitment = toHex(serializedCommitment, sizeof(serializedCommitment));
    commitOut->proof = QString::fromUtf8(proof.toHex());
    return true;
}

/**
 * @brief Builds commitment and rangeproof from secrets.
 * @param blind
 * @param privateNonceHash
 * @param rewindNonceHash
 * @param proofMessage
 * @param value
 * @param commitOut
 * @return
 */
bool createCommitmentAndRangeproofFromSecrets(const QByteArray &blind,
                                              const QByteArray &privateNonceHash,
                                              const QByteArray &rewindNonceHash,
                                              const QByteArray &proofMessage,
                                              quint64 value,
                                              SlateV4::Commit *commitOut)
{
    if (!commitOut) {
        return false;
    }

    secp256k1_context *context = walletSecpContext();
    secp256k1_bulletproof_generators *generators = walletBulletproofGenerators();
    if (!context || !generators) {
        return false;
    }
    if (blind.size() != 32 || privateNonceHash.size() != 32 || rewindNonceHash.size() != 32 || proofMessage.size() != 20) {
        return false;
    }

    secp256k1_pedersen_commitment commitment;
    if (secp256k1_pedersen_commit(context,
                                  &commitment,
                                  reinterpret_cast<const unsigned char *>(blind.constData()),
                                  value,
                                  &secp256k1_generator_const_h,
                                  &secp256k1_generator_const_g) != 1) {
        return false;
    }

    unsigned char serializedCommitment[33];
    if (secp256k1_pedersen_commitment_serialize(context, serializedCommitment, &commitment) != 1) {
        return false;
    }

    const QByteArray commitmentBytes(reinterpret_cast<const char *>(serializedCommitment), sizeof(serializedCommitment));
    const QByteArray rewindNonce = WalletBlake2b::hash256(commitmentBytes, rewindNonceHash);
    const QByteArray privateNonce = WalletBlake2b::hash256(commitmentBytes, privateNonceHash);
    if (rewindNonce.size() != 32 || privateNonce.size() != 32) {
        return false;
    }

    QByteArray proof(SECP256K1_BULLETPROOF_MAX_PROOF, Qt::Uninitialized);
    size_t proofLen = static_cast<size_t>(proof.size());
    const uint64_t values[1] = { value };
    const unsigned char *blinds[1] = {
        reinterpret_cast<const unsigned char *>(blind.constData())
    };

    secp256k1_scratch_space *scratch = secp256k1_scratch_space_create(context, kBulletproofScratchSpaceSize);
    if (!scratch) {
        return false;
    }

    const int proved = secp256k1_bulletproof_rangeproof_prove(
        context,
        scratch,
        generators,
        reinterpret_cast<unsigned char *>(proof.data()),
        &proofLen,
        0,
        0,
        0,
        values,
        0,
        blinds,
        0,
        1,
        &secp256k1_generator_const_h,
        64,
        reinterpret_cast<const unsigned char *>(rewindNonce.constData()),
        reinterpret_cast<const unsigned char *>(privateNonce.constData()),
        0,
        0,
        reinterpret_cast<const unsigned char *>(proofMessage.constData()));

    if (proved != 1) {
        secp256k1_scratch_space_destroy(scratch);
        return false;
    }

    const int verified = secp256k1_bulletproof_rangeproof_verify(
        context,
        scratch,
        generators,
        reinterpret_cast<const unsigned char *>(proof.constData()),
        proofLen,
        0,
        &commitment,
        1,
        64,
        &secp256k1_generator_const_h,
        0,
        0);

    secp256k1_scratch_space_destroy(scratch);
    if (verified != 1) {
        return false;
    }

    proof.resize(static_cast<int>(proofLen));
    commitOut->feature = 0;
    commitOut->commitment = toHex(serializedCommitment, sizeof(serializedCommitment));
    commitOut->proof = QString::fromUtf8(proof.toHex());
    return true;
}

}
