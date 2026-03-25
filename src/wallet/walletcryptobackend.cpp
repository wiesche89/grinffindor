#include "walletcryptobackend.h"
#include "walletblake2b.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QStringList>
#include <cstring>

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

bool WalletCryptoBackend::applyRound2Signature(SlateV4 *slate,
                                               const QString &walletFingerprint,
                                               const QString &roleTag,
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
    if (roleTag == QStringLiteral("receiver")
        && slate->metadata.value(QStringLiteral("external_binary")).toBool()) {
        const QString receiverBlindHex = slate->metadata.value(QStringLiteral("receiver_blind")).toString();
        const QString receiverOffsetHex = slate->metadata.value(QStringLiteral("receiver_offset")).toString();
        const QByteArray baseBlind = receiverBlindHex.isEmpty()
            ? deriveSigningBaseSecret(walletFingerprint, slate->workflowId(), roleTag)
            : fromHex(receiverBlindHex);
        const QByteArray offset = receiverOffsetHex.isEmpty() ? fromHex(slate->offset) : fromHex(receiverOffsetHex);
        QByteArray signingSecret;
        if (!subtractScalars(baseBlind, offset, &signingSecret)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to derive receiver signing key from offset.");
            }
            return false;
        }
        context.blindSecret = QString::fromUtf8(signingSecret.toHex());
        context.blindPublic = createCompressedPubkeyHex(signingSecret);
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

    QList<QString> blindPubkeys;
    QList<QString> noncePubkeys;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate->signatures.at(i);
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
        blindPubkeys.append(participant.xs);
        noncePubkeys.append(participant.nonce);
    }

    if (blindPubkeys.size() < slate->numParticipants) {
        if (errorOut) {
            *errorOut = QStringLiteral("Not enough partial signatures to finalize.");
        }
        return false;
    }

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    if (!combinePubkeys(blindPubkeys, &totalBlind) || !combinePubkeys(noncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine final public keys.");
        }
        return false;
    }

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

    const QByteArray messageHash = buildKernelSignatureMessage(*slate);
    QVector<secp256k1_pubkey> pubkeys;
    for (int i = 0; i < blindPubkeys.size(); ++i) {
        secp256k1_pubkey pubkey;
        if (!parsePubkey(blindPubkeys.at(i), &pubkey)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to parse final participant key.");
            }
            return false;
        }
        pubkeys.append(pubkey);
    }

    if (secp256k1_aggsig_build_scratch_and_verify(walletSecpContext(),
                                                  finalSig,
                                                  reinterpret_cast<const unsigned char *>(messageHash.constData()),
                                                  pubkeys.constData(),
                                                  static_cast<size_t>(pubkeys.size())) != 1) {
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
