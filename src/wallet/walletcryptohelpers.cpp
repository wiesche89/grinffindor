#include "walletcryptohelpers.h"

#include "walletblake2b.h"

#include "../submodules/grin-node-api/src/attributes/transaction.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>
#include <cstring>

namespace
{

const size_t kBulletproofGeneratorCount = 256;
const size_t kBulletproofScratchSpaceSize = 8 * 1024 * 1024;
const char kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

class SecpContextHolder
{
public:
    SecpContextHolder()
    {
        m_context = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        if (m_context) {
            unsigned char seed[32];
            for (int i = 0; i < 32; ++i) {
                seed[i] = static_cast<unsigned char>(QRandomGenerator::global()->bounded(256));
            }
            secp256k1_context_randomize(m_context, seed);
            m_generators = secp256k1_bulletproof_generators_create(
                m_context,
                &secp256k1_generator_const_g,
                kBulletproofGeneratorCount);
        }
    }

    ~SecpContextHolder()
    {
        if (m_generators) {
            secp256k1_bulletproof_generators_destroy(m_context, m_generators);
        }
        if (m_context) {
            secp256k1_context_destroy(m_context);
        }
    }

    secp256k1_context *context() const
    {
        return m_context;
    }

    secp256k1_bulletproof_generators *generators() const
    {
        return m_generators;
    }

private:
    secp256k1_context *m_context = 0;
    secp256k1_bulletproof_generators *m_generators = 0;
};

SecpContextHolder &walletSecpHolder()
{
    static SecpContextHolder holder;
    return holder;
}

QVector<int> convertBits(const QByteArray &data, int fromBits, int toBits, bool pad)
{
    QVector<int> output;
    int accumulator = 0;
    int bitCount = 0;
    const int maxValue = (1 << toBits) - 1;
    const int maxAccumulator = (1 << (fromBits + toBits - 1)) - 1;

    for (int i = 0; i < data.size(); ++i) {
        const int value = static_cast<unsigned char>(data.at(i));
        if ((value >> fromBits) != 0) {
            return QVector<int>();
        }
        accumulator = ((accumulator << fromBits) | value) & maxAccumulator;
        bitCount += fromBits;
        while (bitCount >= toBits) {
            bitCount -= toBits;
            output.append((accumulator >> bitCount) & maxValue);
        }
    }

    if (pad) {
        if (bitCount > 0) {
            output.append((accumulator << (toBits - bitCount)) & maxValue);
        }
    } else if (bitCount >= fromBits || ((accumulator << (toBits - bitCount)) & maxValue) != 0) {
        return QVector<int>();
    }

    return output;
}

QVector<int> hrpExpand(const QString &hrp)
{
    QVector<int> expanded;
    expanded.reserve(hrp.size() * 2 + 1);
    for (int i = 0; i < hrp.size(); ++i) {
        expanded.append(hrp.at(i).unicode() >> 5);
    }
    expanded.append(0);
    for (int i = 0; i < hrp.size(); ++i) {
        expanded.append(hrp.at(i).unicode() & 31);
    }
    return expanded;
}

quint32 bech32Polymod(const QVector<int> &values)
{
    static const quint32 generators[5] = {
        0x3b6a57b2U, 0x26508e6dU, 0x1ea119faU, 0x3d4233ddU, 0x2a1462b3U
    };

    quint32 checksum = 1;
    for (int i = 0; i < values.size(); ++i) {
        const quint32 top = checksum >> 25;
        checksum = ((checksum & 0x1ffffffU) << 5) ^ static_cast<quint32>(values.at(i));
        for (int j = 0; j < 5; ++j) {
            if (((top >> j) & 1U) != 0U) {
                checksum ^= generators[j];
            }
        }
    }
    return checksum;
}

QVector<int> bech32CreateChecksum(const QString &hrp, const QVector<int> &data)
{
    QVector<int> values = hrpExpand(hrp);
    values += data;
    values += QVector<int>(6, 0);
    const quint32 polymod = bech32Polymod(values) ^ 1U;

    QVector<int> checksum;
    checksum.reserve(6);
    for (int i = 0; i < 6; ++i) {
        checksum.append((polymod >> (5 * (5 - i))) & 31U);
    }
    return checksum;
}

}

namespace WalletCryptoHelpers
{

secp256k1_context *walletSecpContext()
{
    return walletSecpHolder().context();
}

secp256k1_bulletproof_generators *walletBulletproofGenerators()
{
    return walletSecpHolder().generators();
}

QByteArray hashBytes(const QByteArray &input)
{
    return QCryptographicHash::hash(input, QCryptographicHash::Sha256);
}

QString toHex(const unsigned char *data, int size)
{
    return QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(data), size).toHex());
}

QString bech32Encode(const QString &hrp, const QByteArray &payload)
{
    const QVector<int> data = convertBits(payload, 8, 5, true);
    if (data.isEmpty() && !payload.isEmpty()) {
        return QString();
    }

    const QVector<int> checksum = bech32CreateChecksum(hrp, data);
    QString encoded = hrp + QLatin1Char('1');
    encoded.reserve(hrp.size() + 1 + data.size() + checksum.size());
    for (int i = 0; i < data.size(); ++i) {
        encoded.append(QLatin1Char(kBech32Charset[data.at(i)]));
    }
    for (int i = 0; i < checksum.size(); ++i) {
        encoded.append(QLatin1Char(kBech32Charset[checksum.at(i)]));
    }
    return encoded;
}

void appendU8(QByteArray &out, quint8 value)
{
    out.append(static_cast<char>(value));
}

void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>(value & 0xff));
}

void appendU64(QByteArray &out, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

QByteArray fromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toUtf8());
}

QByteArray deriveValidSecretBytes(const QString &domain, const QString &left, const QString &right)
{
    secp256k1_context *context = walletSecpContext();
    for (int counter = 0; counter < 1024; ++counter) {
        QByteArray candidate = hashBytes(QStringLiteral("%1:%2:%3:%4")
                                             .arg(domain, left, right, QString::number(counter))
                                             .toUtf8());
        if (candidate.size() == 32
            && context
            && secp256k1_ec_seckey_verify(context, reinterpret_cast<const unsigned char *>(candidate.constData())) == 1) {
            return candidate;
        }
    }

    QByteArray fallback(32, Qt::Uninitialized);
    do {
        for (int i = 0; i < fallback.size(); ++i) {
            fallback[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
        }
    } while (!context
             || secp256k1_ec_seckey_verify(context, reinterpret_cast<const unsigned char *>(fallback.constData())) != 1);
    return fallback;
}

QByteArray deriveSigningBaseSecret(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag)
{
    return deriveValidSecretBytes(QStringLiteral("blind-base"),
                                  walletFingerprint,
                                  workflowId + QLatin1Char(':') + roleTag);
}

QByteArray deriveAggsigSecnonce(const QString &walletFingerprint,
                                const QString &workflowId,
                                const QString &roleTag)
{
    const QByteArray seed = hashBytes(
        QStringLiteral("nonce-seed:%1:%2:%3")
            .arg(walletFingerprint, workflowId, roleTag)
            .toUtf8());
    if (seed.size() != 32) {
        return QByteArray();
    }

    unsigned char secnonce[32];
    if (secp256k1_aggsig_export_secnonce_single(
            walletSecpContext(),
            secnonce,
            reinterpret_cast<const unsigned char *>(seed.constData())) != 1) {
        return QByteArray();
    }
    return QByteArray(reinterpret_cast<const char *>(secnonce), sizeof(secnonce));
}

QByteArray paymentProofMessage(const SlateV4 &slate)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(slate.id,
             slate.amount,
             slate.fee,
             slate.paymentProof.senderAddress,
             slate.paymentProof.receiverAddress)
        .toUtf8();
}

QString createCompressedPubkeyHex(const QByteArray &secretKey)
{
    secp256k1_context *context = walletSecpContext();
    if (!context || secretKey.size() != 32) {
        return QString();
    }

    secp256k1_pubkey pubkey;
    if (secp256k1_ec_pubkey_create(context, &pubkey,
                                   reinterpret_cast<const unsigned char *>(secretKey.constData())) != 1) {
        return QString();
    }

    unsigned char serialized[33];
    size_t serializedSize = sizeof(serialized);
    if (secp256k1_ec_pubkey_serialize(context, serialized, &serializedSize, &pubkey, SECP256K1_EC_COMPRESSED) != 1) {
        return QString();
    }

    return toHex(serialized, static_cast<int>(serializedSize));
}

bool parsePubkey(const QString &hex, secp256k1_pubkey *pubkey)
{
    const QByteArray bytes = fromHex(hex);
    if (bytes.size() != 33) {
        return false;
    }
    return secp256k1_ec_pubkey_parse(walletSecpContext(),
                                     pubkey,
                                     reinterpret_cast<const unsigned char *>(bytes.constData()),
                                     static_cast<size_t>(bytes.size())) == 1;
}

QString serializePubkey(const secp256k1_pubkey &pubkey)
{
    unsigned char serialized[33];
    size_t serializedSize = sizeof(serialized);
    if (secp256k1_ec_pubkey_serialize(walletSecpContext(),
                                      serialized,
                                      &serializedSize,
                                      &pubkey,
                                      SECP256K1_EC_COMPRESSED) != 1) {
        return QString();
    }
    return toHex(serialized, static_cast<int>(serializedSize));
}

bool combinePubkeys(const QList<QString> &hexPubkeys, secp256k1_pubkey *combined)
{
    QVector<secp256k1_pubkey> parsed;
    QVector<const secp256k1_pubkey *> ptrs;
    for (int i = 0; i < hexPubkeys.size(); ++i) {
        secp256k1_pubkey pubkey;
        if (!parsePubkey(hexPubkeys.at(i), &pubkey)) {
            return false;
        }
        parsed.append(pubkey);
    }
    for (int i = 0; i < parsed.size(); ++i) {
        ptrs.append(&parsed[i]);
    }
    return !ptrs.isEmpty()
        && secp256k1_ec_pubkey_combine(walletSecpContext(),
                                       combined,
                                       ptrs.constData(),
                                       static_cast<size_t>(ptrs.size())) == 1;
}

quint64 amountToNanogrin(const QString &amount)
{
    const QString simplified = amount.trimmed();
    if (simplified.isEmpty()) {
        return 0;
    }

    const QStringList parts = simplified.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = (parts.size() == 2) ? parts.at(1) : QString();
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

bool addScalars(const QByteArray &left, const QByteArray &right, QByteArray *sumOut)
{
    if (!sumOut || left.size() != 32 || right.size() != 32) {
        return false;
    }
    QByteArray sum = left;
    if (secp256k1_ec_privkey_tweak_add(walletSecpContext(),
                                       reinterpret_cast<unsigned char *>(sum.data()),
                                       reinterpret_cast<const unsigned char *>(right.constData())) != 1) {
        return false;
    }
    *sumOut = sum;
    return true;
}

bool subtractScalars(const QByteArray &left, const QByteArray &right, QByteArray *differenceOut)
{
    if (!differenceOut || left.size() != 32 || right.size() != 32) {
        return false;
    }
    QByteArray negated = right;
    if (secp256k1_ec_privkey_negate(walletSecpContext(),
                                    reinterpret_cast<unsigned char *>(negated.data())) != 1) {
        return false;
    }
    return addScalars(left, negated, differenceOut);
}

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

QString serializeCommitment(const secp256k1_pedersen_commitment &commitment)
{
    unsigned char serialized[33];
    if (secp256k1_pedersen_commitment_serialize(walletSecpContext(), serialized, &commitment) != 1) {
        return QString();
    }
    return toHex(serialized, sizeof(serialized));
}

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

QByteArray buildKernelSignatureMessageForFeature(const QString &feature,
                                                 quint64 fee,
                                                 quint64 lockHeight,
                                                 QString *errorOut)
{
    QByteArray serialized;
    if (feature.isEmpty() || feature == QStringLiteral("Plain")) {
        appendU8(serialized, 0);
        appendU64(serialized, fee);
    } else if (feature == QStringLiteral("Coinbase")) {
        appendU8(serialized, 1);
    } else if (feature == QStringLiteral("HeightLocked")) {
        appendU8(serialized, 2);
        appendU64(serialized, fee);
        appendU64(serialized, lockHeight);
    } else {
        if (errorOut) {
            *errorOut = QStringLiteral("Unsupported kernel feature for signature validation: %1").arg(feature);
        }
        return QByteArray();
    }

    return QCryptographicHash::hash(serialized, QCryptographicHash::Blake2b_256);
}

QByteArray buildKernelSignatureMessage(const SlateV4 &slate)
{
    QByteArray serialized;
    appendU8(serialized, static_cast<quint8>(slate.kernelFeatures));

    if (slate.kernelFeatures != 1) {
        appendU64(serialized, amountToNanogrin(slate.fee));
    }

    if (slate.kernelFeatures == 2) {
        appendU64(serialized, slate.metadata.value(QStringLiteral("lock_height")).toVariant().toULongLong());
    } else if (slate.kernelFeatures == 3) {
        appendU16(serialized,
                  static_cast<quint16>(slate.metadata.value(QStringLiteral("lock_height")).toVariant().toUInt()));
    }

    return QCryptographicHash::hash(serialized, QCryptographicHash::Blake2b_256);
}

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

int findParticipantIndex(const SlateV4 &slate, const QString &publicBlind)
{
    for (int i = 0; i < slate.signatures.size(); ++i) {
        if (slate.signatures.at(i).xs == publicBlind) {
            return i;
        }
    }
    return -1;
}

bool createPartialSignature(const QByteArray &messageHash,
                            const QByteArray &seckey,
                            const QByteArray &secnonce,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkeyTotal,
                            QByteArray *signatureOut)
{
    if (messageHash.size() != 32 || seckey.size() != 32 || secnonce.size() != 32 || !signatureOut) {
        return false;
    }

    unsigned char sig64[64];
    unsigned char seed[32];
    for (int i = 0; i < 32; ++i) {
        seed[i] = static_cast<unsigned char>(QRandomGenerator::global()->bounded(256));
    }

    const int ok = secp256k1_aggsig_sign_single(
        walletSecpContext(),
        sig64,
        reinterpret_cast<const unsigned char *>(messageHash.constData()),
        reinterpret_cast<const unsigned char *>(seckey.constData()),
        reinterpret_cast<const unsigned char *>(secnonce.constData()),
        0,
        &pubnonceTotal,
        &pubnonceTotal,
        &pubkeyTotal,
        seed);

    if (ok != 1) {
        return false;
    }

    *signatureOut = QByteArray(reinterpret_cast<const char *>(sig64), sizeof(sig64));
    return true;
}

bool aggsigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut)
{
    if (rawSignature.size() != 64 || !compactOut) {
        return false;
    }

    *compactOut = rawSignature;
    return true;
}

bool aggsigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut)
{
    if (compactSignature.size() != 64 || !rawOut) {
        return false;
    }

    *rawOut = compactSignature;
    return true;
}

bool kernelSigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut)
{
    if (rawSignature.size() != 64 || !compactOut) {
        return false;
    }

    secp256k1_ecdsa_signature signature;
    std::memcpy(signature.data, rawSignature.constData(), 64);

    unsigned char compact[64];
    if (secp256k1_ecdsa_signature_serialize_compact(walletSecpContext(), compact, &signature) != 1) {
        return false;
    }

    *compactOut = QByteArray(reinterpret_cast<const char *>(compact), sizeof(compact));
    return true;
}

bool kernelSigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut)
{
    if (compactSignature.size() != 64 || !rawOut) {
        return false;
    }

    secp256k1_ecdsa_signature signature;
    if (secp256k1_ecdsa_signature_parse_compact(
            walletSecpContext(),
            &signature,
            reinterpret_cast<const unsigned char *>(compactSignature.constData())) != 1) {
        return false;
    }

    *rawOut = QByteArray(reinterpret_cast<const char *>(signature.data), sizeof(signature.data));
    return true;
}

bool verifyPartialSignature(const QByteArray &signature,
                            const QByteArray &messageHash,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkey,
                            const secp256k1_pubkey &pubkeyTotal)
{
    return signature.size() == 64
        && messageHash.size() == 32
        && secp256k1_aggsig_verify_single(
               walletSecpContext(),
               reinterpret_cast<const unsigned char *>(signature.constData()),
               reinterpret_cast<const unsigned char *>(messageHash.constData()),
               &pubnonceTotal,
               &pubkey,
               &pubkeyTotal,
               0,
               1) == 1;
}

}
