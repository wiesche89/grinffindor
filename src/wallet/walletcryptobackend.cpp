#include "walletcryptobackend.h"
#include "walletblake2b.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDebug>
#include <QStringList>
#include <QVector>
#include <cstring>

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
extern "C" {
#include "monocypher.h"
}
#endif

extern "C" {
#include "secp256k1.h"
#include "secp256k1_aggsig.h"
#include "secp256k1_bulletproofs.h"
#include "secp256k1_commitment.h"
}

namespace {

const size_t kBulletproofGeneratorCount = 256;
const size_t kBulletproofScratchSpaceSize = 8 * 1024 * 1024;

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

const char kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

QVector<int> bech32CharsetReverse()
{
    QVector<int> reverse(128, -1);
    for (int i = 0; kBech32Charset[i] != '\0'; ++i) {
        reverse[static_cast<int>(kBech32Charset[i])] = i;
    }
    return reverse;
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

QByteArray deriveSigningBaseSecret(const QString &walletFingerprint, const QString &workflowId, const QString &roleTag)
{
    return deriveValidSecretBytes(QStringLiteral("blind-base"), walletFingerprint, workflowId + QLatin1Char(':') + roleTag);
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

quint64 amountToNanogrin(const QString &amount);

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
    const QByteArray privateNonce = deriveValidSecretBytes(QStringLiteral("proof-private"), walletFingerprint, workflowId + QLatin1Char(':') + roleTag);
    const QByteArray rewindNonce = deriveValidSecretBytes(QStringLiteral("proof-rewind"), walletFingerprint, workflowId + QLatin1Char(':') + roleTag);
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

bool WalletCryptoBackend::supportsRealGrinTransactions()
{
    return true;
}

WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createParticipant(const QString &walletFingerprint,
                                                                              const QString &workflowId,
                                                                              const QString &roleTag)
{
    ParticipantContext context;
    const QString entropy = walletFingerprint + QLatin1Char(':') + workflowId + QLatin1Char(':') + roleTag;
    const QByteArray blindSecret = deriveSigningBaseSecret(walletFingerprint, workflowId, roleTag);
    const QByteArray nonceSecret = deriveValidSecretBytes(QStringLiteral("nonce"), walletFingerprint, entropy);
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = createCompressedPubkeyHex(blindSecret);
    context.noncePublic = createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createRandomParticipant(const QString &roleTag)
{
    const QString entropy = QStringLiteral("%1:%2")
        .arg(roleTag, randomHex(32));
    const QByteArray blindSecret = deriveValidSecretBytes(QStringLiteral("random-blind"), entropy, QStringLiteral("blind"));
    const QByteArray nonceSecret = deriveValidSecretBytes(QStringLiteral("random-nonce"), entropy, QStringLiteral("nonce"));

    ParticipantContext context;
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = createCompressedPubkeyHex(blindSecret);
    context.noncePublic = createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

QString WalletCryptoBackend::createOffset(const QString &walletFingerprint, const QString &workflowId)
{
    return QString::fromUtf8(deriveValidSecretBytes(QStringLiteral("offset"), walletFingerprint, workflowId).toHex());
}

QString WalletCryptoBackend::addOffsets(const QString &leftOffset, const QString &rightOffset, QString *errorOut)
{
    const QByteArray left = fromHex(leftOffset);
    const QByteArray right = fromHex(rightOffset);
    QByteArray sum;
    if (!addScalars(left, right, &sum)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to add transaction offsets.");
        }
        return QString();
    }
    return QString::fromUtf8(sum.toHex());
}

QString WalletCryptoBackend::negateScalar(const QString &value, QString *errorOut)
{
    QByteArray scalar = fromHex(value);
    if (scalar.size() != 32) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid scalar length.");
        }
        return QString();
    }
    if (secp256k1_ec_privkey_negate(walletSecpContext(),
                                    reinterpret_cast<unsigned char *>(scalar.data())) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to negate scalar.");
        }
        return QString();
    }
    return QString::fromUtf8(scalar.toHex());
}

QString WalletCryptoBackend::combineBlindingFactors(const QStringList &positiveBlinds,
                                                    const QStringList &negativeBlinds,
                                                    QString *errorOut)
{
    QByteArray combined;
    bool initialized = false;

    for (int i = 0; i < positiveBlinds.size(); ++i) {
        const QByteArray blind = fromHex(positiveBlinds.at(i));
        if (blind.size() != 32) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid positive blinding factor.");
            }
            return QString();
        }
        if (!initialized) {
            combined = blind;
            initialized = true;
            continue;
        }
        if (!addScalars(combined, blind, &combined)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to combine positive blinding factors.");
            }
            return QString();
        }
    }

    if (!initialized) {
        if (errorOut) {
            *errorOut = QStringLiteral("No positive blinding factors were available.");
        }
        return QString();
    }

    for (int i = 0; i < negativeBlinds.size(); ++i) {
        const QByteArray blind = fromHex(negativeBlinds.at(i));
        if (blind.size() != 32) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid negative blinding factor.");
            }
            return QString();
        }
        if (!subtractScalars(combined, blind, &combined)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to subtract negative blinding factors.");
            }
            return QString();
        }
    }

    return QString::fromUtf8(combined.toHex());
}

WalletCryptoBackend::CommitmentResult WalletCryptoBackend::createCommitment(const QString &walletFingerprint,
                                                                            const QString &workflowId,
                                                                            const QString &roleTag,
                                                                            const QString &amount)
{
    CommitmentResult result;
    if (!createCommitmentAndRangeproof(walletFingerprint, workflowId, roleTag, amount, &result.commit)) {
        result.error = QStringLiteral("Failed to create commitment and rangeproof.");
        return result;
    }
    result.success = true;
    return result;
}

WalletCryptoBackend::OwnedCommitment WalletCryptoBackend::createOwnedCommitment(const WalletKeychain &keychain,
                                                                                quint32 childIndex,
                                                                                const QString &amount)
{
    OwnedCommitment owned;
    const quint64 value = amountToNanogrin(amount);
    const WalletKeychain::OutputSecrets secrets = keychain.deriveOutputSecrets(childIndex, value);
    if (!secrets.success) {
        return owned;
    }

    if (!createCommitmentAndRangeproofFromSecrets(
            secrets.blindingFactor,
            secrets.privateNonceHash,
            secrets.rewindNonceHash,
            secrets.proofMessage,
            value,
            &owned.commit)) {
        return owned;
    }

    owned.success = true;
    owned.blindingFactor = QString::fromUtf8(secrets.blindingFactor.toHex());
    owned.keyPath = secrets.keyPath;
    owned.childIndex = secrets.childIndex;
    return owned;
}

QString WalletCryptoBackend::slatepackAddress(const WalletKeychain &keychain,
                                              const QString &networkName)
{
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    const QByteArray seed = keychain.slatepackSecretKey();
    if (seed.size() != 32) {
        return QString();
    }

    QByteArray seedCopy = seed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));

    const QString hrp = networkName.trimmed().compare(QStringLiteral("mainnet"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("grin")
        : QStringLiteral("tgrin");
    return bech32Encode(hrp, publicKey);
#else
    Q_UNUSED(keychain);
    Q_UNUSED(networkName);
    return QString();
#endif
}

QString WalletCryptoBackend::paymentProofAddress(const WalletKeychain &keychain)
{
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    const QByteArray seed = keychain.slatepackSecretKey();
    if (seed.size() != 32) {
        return QString();
    }

    QByteArray seedCopy = seed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    return QString::fromUtf8(publicKey.toHex());
#else
    Q_UNUSED(keychain);
    return QString();
#endif
}

SlateV4::ParticipantData WalletCryptoBackend::createParticipantData(const ParticipantContext &context)
{
    SlateV4::ParticipantData data;
    data.xs = context.blindPublic;
    data.nonce = context.noncePublic;
    return data;
}

SlateV4::PaymentProof WalletCryptoBackend::createPaymentProof(const ParticipantContext &sender, const ParticipantContext &receiver)
{
    SlateV4::PaymentProof proof;
    proof.senderAddress = sender.address;
    proof.receiverAddress = receiver.address;
    return proof;
}

bool WalletCryptoBackend::signPaymentProof(SlateV4 *slate,
                                           const WalletKeychain &keychain,
                                           QString *errorOut)
{
    if (!slate) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate is missing.");
        }
        return false;
    }
    if (!slate->hasPaymentProof) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof is missing.");
        }
        return false;
    }
    if (slate->paymentProof.senderAddress.trimmed().isEmpty()
        || slate->paymentProof.receiverAddress.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof addresses are incomplete.");
        }
        return false;
    }

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    const QByteArray seed = keychain.slatepackSecretKey();
    if (seed.size() != 32) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet keychain does not expose a Slatepack signing seed.");
        }
        return false;
    }

    QByteArray seedCopy = seed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    const QString receiverAddress = paymentProofAddress(keychain);
    if (!slate->paymentProof.receiverAddress.isEmpty()
        && slate->paymentProof.receiverAddress != receiverAddress) {
        if (errorOut) {
            *errorOut = QStringLiteral("Current wallet does not match the payment proof receiver address.");
        }
        return false;
    }

    const QByteArray message = paymentProofMessage(*slate);
    unsigned char signature[64];
    crypto_eddsa_sign(signature,
                      reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(message.constData()),
                      static_cast<size_t>(message.size()));
    slate->paymentProof.receiverAddress = receiverAddress;
    slate->paymentProof.receiverSignature = toHex(signature, sizeof(signature));
    return true;
#else
    Q_UNUSED(keychain);
    if (errorOut) {
        *errorOut = QStringLiteral("This build does not support Slatepack payment proof signatures.");
    }
    return false;
#endif
}

bool WalletCryptoBackend::verifyPaymentProof(const SlateV4 &slate, QString *errorOut)
{
    if (!slate.hasPaymentProof) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof is missing.");
        }
        return false;
    }
    if (slate.paymentProof.senderAddress.trimmed().isEmpty()
        || slate.paymentProof.receiverAddress.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof addresses are incomplete.");
        }
        return false;
    }
    if (slate.paymentProof.receiverSignature.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof receiver signature is missing.");
        }
        return false;
    }

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    const QByteArray publicKey = QByteArray::fromHex(slate.paymentProof.receiverAddress.trimmed().toUtf8());
    const QByteArray signature = QByteArray::fromHex(slate.paymentProof.receiverSignature.trimmed().toUtf8());
    if (publicKey.size() != 32 || signature.size() != 64) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof key material is malformed.");
        }
        return false;
    }

    const QByteArray message = paymentProofMessage(slate);
    const int ok = crypto_eddsa_check(reinterpret_cast<const uint8_t *>(signature.constData()),
                                      reinterpret_cast<const uint8_t *>(publicKey.constData()),
                                      reinterpret_cast<const uint8_t *>(message.constData()),
                                      static_cast<size_t>(message.size()));
    if (ok != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Payment proof signature verification failed.");
        }
        return false;
    }
    return true;
#else
    if (errorOut) {
        *errorOut = QStringLiteral("This build does not support Slatepack payment proof verification.");
    }
    return false;
#endif
}

bool WalletCryptoBackend::applyRound2Signature(SlateV4 *slate,
                                               const QString &walletFingerprint,
                                               const QString &roleTag,
                                               const ParticipantContext *overrideContext,
                                               QString *errorOut)
{
    if (!slate) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate is missing.");
        }
        return false;
    }
    if (slate->workflowId().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow id is missing.");
        }
        return false;
    }

    ParticipantContext context = createParticipant(walletFingerprint, slate->workflowId(), roleTag);
    if (overrideContext) {
        context = *overrideContext;
    }
    const int existingIndex = findParticipantIndex(*slate, context.blindPublic);
    if (existingIndex < 0) {
        slate->signatures.append(createParticipantData(context));
    }

    QList<QString> blindPubkeys;
    QList<QString> noncePubkeys;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        blindPubkeys.append(slate->signatures.at(i).xs);
        noncePubkeys.append(slate->signatures.at(i).nonce);
    }
    qDebug() << "[WalletRound2]"
             << "workflowId=" << slate->workflowId()
             << "state=" << slate->stateCode()
             << "role=" << roleTag
             << "sigCount=" << slate->signatures.size()
             << "blindPubkeys=" << blindPubkeys
             << "noncePubkeys=" << noncePubkeys;

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    secp256k1_pubkey ownBlind;
    if (!combinePubkeys(blindPubkeys, &totalBlind)
        || !combinePubkeys(noncePubkeys, &totalNonce)
        || !parsePubkey(context.blindPublic, &ownBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to build aggregated public keys.");
        }
        return false;
    }

    const QByteArray messageHash = buildKernelSignatureMessage(*slate);
    qDebug() << "[WalletRound2]"
             << "workflowId=" << slate->workflowId()
             << "messageHash=" << QString::fromUtf8(messageHash.toHex())
             << "contextBlindPublic=" << context.blindPublic
             << "contextNoncePublic=" << context.noncePublic
             << "override=" << (overrideContext != 0);
    QByteArray partialSignature;
    if (!createPartialSignature(messageHash,
                                fromHex(context.blindSecret),
                                fromHex(context.nonceSecret),
                                totalNonce,
                                totalBlind,
                                &partialSignature)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to create aggsig partial.");
        }
        return false;
    }

    if (!verifyPartialSignature(partialSignature, messageHash, totalNonce, ownBlind, totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Aggsig partial verification failed.");
        }
        return false;
    }

    const int participantIndex = findParticipantIndex(*slate, context.blindPublic);
    if (participantIndex < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Participant entry could not be located.");
        }
        return false;
    }

    slate->signatures[participantIndex].part = QString::fromUtf8(partialSignature.toHex());
    slate->metadata.insert(QStringLiteral("message_hash"), QString::fromUtf8(messageHash.toHex()));
    slate->metadata.insert(QStringLiteral("pubkey_total"), serializePubkey(totalBlind));
    slate->metadata.insert(QStringLiteral("pubnonce_total"), serializePubkey(totalNonce));
    slate->metadata.insert(QStringLiteral("signature_status"), QStringLiteral("partial"));
    qDebug() << "[WalletRound2]"
             << "workflowId=" << slate->workflowId()
             << "partialLen=" << partialSignature.size()
             << "pubkeyTotal=" << serializePubkey(totalBlind)
             << "pubnonceTotal=" << serializePubkey(totalNonce);
    return true;
}

bool WalletCryptoBackend::finalizeSlate(SlateV4 *slate, QString *errorOut)
{
    if (!slate) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate is missing.");
        }
        return false;
    }

    QList<QString> allBlindPubkeys;
    QList<QString> allNoncePubkeys;
    QList<QString> sigBlindPubkeys;
    qDebug() << "[WalletFinalize] start"
             << "workflowId=" << slate->workflowId()
             << "state=" << slate->stateCode()
             << "numParticipants=" << slate->numParticipants
             << "sigCount=" << slate->signatures.size()
             << "amount=" << slate->amount
             << "fee=" << slate->fee
             << "offset=" << slate->offset;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate->signatures.at(i);
        qDebug() << "[WalletFinalize] participant"
                 << i
                 << "xs=" << participant.xs
                 << "nonce=" << participant.nonce
                 << "partLen=" << participant.part.length();
        allBlindPubkeys.append(participant.xs);
        allNoncePubkeys.append(participant.nonce);
        if (participant.part.isEmpty()) {
            continue;
        }
        const QByteArray partialBytes = fromHex(participant.part);
        if (partialBytes.size() != 64) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid partial signature length.");
            }
            return false;
        }
        sigBlindPubkeys.append(participant.xs);
    }
    qDebug() << "[WalletFinalize]"
             << "workflowId=" << slate->workflowId()
             << "usablePartialCount=" << sigBlindPubkeys.size()
             << "blindPubkeys=" << allBlindPubkeys
             << "noncePubkeys=" << allNoncePubkeys;

    if (sigBlindPubkeys.size() < slate->numParticipants) {
        if (errorOut) {
            *errorOut = QStringLiteral("Not enough partial signatures to finalize.");
        }
        return false;
    }

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    if (!combinePubkeys(allBlindPubkeys, &totalBlind) || !combinePubkeys(allNoncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine final public keys.");
        }
        return false;
    }
    qDebug() << "[WalletFinalize]"
             << "workflowId=" << slate->workflowId()
             << "pubkeyTotal=" << serializePubkey(totalBlind)
             << "pubnonceTotal=" << serializePubkey(totalNonce);

    QVector<unsigned char *> sigPointers;
    QVector<QByteArray> sigBuffers;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        if (!slate->signatures.at(i).part.isEmpty()) {
            sigBuffers.append(fromHex(slate->signatures.at(i).part));
        }
    }
    for (int i = 0; i < sigBuffers.size(); ++i) {
        sigPointers.append(reinterpret_cast<unsigned char *>(sigBuffers[i].data()));
    }

    unsigned char finalSig[64];
    if (secp256k1_aggsig_add_signatures_single(walletSecpContext(),
                                               finalSig,
                                               const_cast<const unsigned char **>(sigPointers.data()),
                                               static_cast<size_t>(sigPointers.size()),
                                               &totalNonce) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine aggsig partials.");
        }
        return false;
    }
    qDebug() << "[WalletFinalize]"
             << "workflowId=" << slate->workflowId()
             << "combinedFinalSig=" << toHex(finalSig, sizeof(finalSig));

    const QByteArray messageHash = buildKernelSignatureMessage(*slate);
    qDebug() << "[WalletFinalize]"
             << "workflowId=" << slate->workflowId()
             << "messageHash=" << QString::fromUtf8(messageHash.toHex());
    if (secp256k1_aggsig_verify_single(walletSecpContext(),
                                       finalSig,
                                       reinterpret_cast<const unsigned char *>(messageHash.constData()),
                                       0,
                                       &totalBlind,
                                       &totalBlind,
                                       0,
                                       1) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Final aggsig verification failed.");
        }
        return false;
    }

    slate->metadata.insert(QStringLiteral("message_hash"), QString::fromUtf8(messageHash.toHex()));
    slate->metadata.insert(QStringLiteral("pubkey_total"), serializePubkey(totalBlind));
    slate->metadata.insert(QStringLiteral("pubnonce_total"), serializePubkey(totalNonce));
    slate->metadata.insert(QStringLiteral("final_sig"), toHex(finalSig, sizeof(finalSig)));
    slate->metadata.insert(QStringLiteral("signature_status"), QStringLiteral("finalized"));
    qDebug() << "[WalletFinalize] success"
             << "workflowId=" << slate->workflowId()
             << "finalSig=" << toHex(finalSig, sizeof(finalSig));
    return true;
}

bool WalletCryptoBackend::verifyPartialSignatures(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> blindPubkeys;
    QList<QString> noncePubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (participant.xs.isEmpty() || participant.nonce.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Participant entry is missing public keys.");
            }
            return false;
        }
        blindPubkeys.append(participant.xs);
        noncePubkeys.append(participant.nonce);
    }

    if (blindPubkeys.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate does not contain participants.");
        }
        return false;
    }

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    if (!combinePubkeys(blindPubkeys, &totalBlind) || !combinePubkeys(noncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant public keys.");
        }
        return false;
    }

    const QByteArray messageHash = buildKernelSignatureMessage(slate);
    int verifiedCount = 0;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (participant.part.isEmpty()) {
            continue;
        }

        const QByteArray partialBytes = fromHex(participant.part);
        if (partialBytes.size() != 64) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid partial signature length.");
            }
            return false;
        }

        secp256k1_pubkey participantPubkey;
        if (!parsePubkey(participant.xs, &participantPubkey)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to parse participant public key.");
            }
            return false;
        }

        if (!verifyPartialSignature(partialBytes,
                                    messageHash,
                                    totalNonce,
                                    participantPubkey,
                                    totalBlind)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Partial signature verification failed.");
            }
            return false;
        }
        ++verifiedCount;
    }

    if (verifiedCount == 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate does not contain partial signatures.");
        }
        return false;
    }

    qDebug() << "[WalletVerifyPartial]"
             << "workflowId=" << slate.workflowId()
             << "state=" << slate.stateCode()
             << "participantCount=" << slate.signatures.size()
             << "verifiedCount=" << verifiedCount
             << "messageHash=" << QString::fromUtf8(messageHash.toHex())
             << "pubkeyTotal=" << serializePubkey(totalBlind)
             << "pubnonceTotal=" << serializePubkey(totalNonce);
    return true;
}

QString WalletCryptoBackend::calculateExcessCommitment(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> blindPubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (participant.xs.isEmpty()) {
            continue;
        }
        blindPubkeys.append(participant.xs);
    }

    secp256k1_pubkey totalBlind;
    if (!combinePubkeys(blindPubkeys, &totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant excess keys.");
        }
        return QString();
    }

    secp256k1_pedersen_commitment commitment;
    if (secp256k1_pubkey_to_pedersen_commitment(walletSecpContext(), &commitment, &totalBlind) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to convert public blind sum into excess commitment.");
        }
        return QString();
    }

    unsigned char serialized[33];
    if (secp256k1_pedersen_commitment_serialize(walletSecpContext(), serialized, &commitment) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to serialize excess commitment.");
        }
        return QString();
    }

    return toHex(serialized, sizeof(serialized));
}

QString WalletCryptoBackend::kernelSignatureMessageHex(const SlateV4 &slate)
{
    return QString::fromUtf8(buildKernelSignatureMessage(slate).toHex());
}

QString WalletCryptoBackend::combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> blindPubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (!participant.xs.isEmpty()) {
            blindPubkeys.append(participant.xs);
        }
    }

    secp256k1_pubkey totalBlind;
    if (!combinePubkeys(blindPubkeys, &totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant blind public keys.");
        }
        return QString();
    }
    return serializePubkey(totalBlind);
}

QString WalletCryptoBackend::combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> noncePubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (!participant.nonce.isEmpty()) {
            noncePubkeys.append(participant.nonce);
        }
    }

    secp256k1_pubkey totalNonce;
    if (!combinePubkeys(noncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant nonce public keys.");
        }
        return QString();
    }
    return serializePubkey(totalNonce);
}

bool WalletCryptoBackend::buildFinalSignature(const SlateV4 &slate,
                                              QString *finalSignatureOut,
                                              QString *errorOut)
{
    QList<QString> allBlindPubkeys;
    QList<QString> allNoncePubkeys;
    QVector<QByteArray> sigBuffers;

    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        allBlindPubkeys.append(participant.xs);
        allNoncePubkeys.append(participant.nonce);
        if (!participant.part.isEmpty()) {
            const QByteArray partialBytes = fromHex(participant.part);
            if (partialBytes.size() != 64) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Invalid partial signature length.");
                }
                return false;
            }
            sigBuffers.append(partialBytes);
        }
    }

    if (sigBuffers.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate does not contain partial signatures.");
        }
        return false;
    }

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    if (!combinePubkeys(allBlindPubkeys, &totalBlind) || !combinePubkeys(allNoncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine final public keys.");
        }
        return false;
    }

    QVector<unsigned char *> sigPointers;
    for (int i = 0; i < sigBuffers.size(); ++i) {
        sigPointers.append(reinterpret_cast<unsigned char *>(sigBuffers[i].data()));
    }

    unsigned char finalSig[64];
    if (secp256k1_aggsig_add_signatures_single(walletSecpContext(),
                                               finalSig,
                                               const_cast<const unsigned char **>(sigPointers.data()),
                                               static_cast<size_t>(sigPointers.size()),
                                               &totalNonce) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine aggsig partials.");
        }
        return false;
    }

    const QByteArray messageHash = buildKernelSignatureMessage(slate);
    if (secp256k1_aggsig_verify_single(walletSecpContext(),
                                       finalSig,
                                       reinterpret_cast<const unsigned char *>(messageHash.constData()),
                                       0,
                                       &totalBlind,
                                       &totalBlind,
                                       0,
                                       1) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Final aggsig verification failed.");
        }
        return false;
    }

    if (finalSignatureOut) {
        *finalSignatureOut = toHex(finalSig, sizeof(finalSig));
    }
    return true;
}

QString WalletCryptoBackend::describeBackend()
{
    return QStringLiteral("local-secp256k1-zkp-aggsig");
}

QString WalletCryptoBackend::randomHex(int bytes)
{
    QByteArray data(bytes, Qt::Uninitialized);
    for (int i = 0; i < bytes; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return QString::fromUtf8(data.toHex());
}

QString WalletCryptoBackend::hashHex(const QString &input)
{
    return QString::fromUtf8(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256).toHex());
}
