#include "walletcryptohelpers.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <cstring>

namespace WalletCryptoHelpers
{

/**
 * @brief buildKernelSignatureMessageForFeature
 * @param feature
 * @param fee
 * @param lockHeight
 * @param errorOut
 * @return
 */
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

/**
 * @brief buildKernelSignatureMessage
 * @param slate
 * @return
 */
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

/**
 * @brief findParticipantIndex
 * @param slate
 * @param publicBlind
 * @return
 */
int findParticipantIndex(const SlateV4 &slate, const QString &publicBlind)
{
    for (int i = 0; i < slate.signatures.size(); ++i) {
        if (slate.signatures.at(i).xs == publicBlind) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief createPartialSignature
 * @param messageHash
 * @param seckey
 * @param secnonce
 * @param pubnonceTotal
 * @param pubkeyTotal
 * @param signatureOut
 * @return
 */
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

/**
 * @brief aggsigRawToCompact
 * @param rawSignature
 * @param compactOut
 * @return
 */
bool aggsigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut)
{
    if (rawSignature.size() != 64 || !compactOut) {
        return false;
    }

    *compactOut = rawSignature;
    return true;
}

/**
 * @brief aggsigCompactToRaw
 * @param compactSignature
 * @param rawOut
 * @return
 */
bool aggsigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut)
{
    if (compactSignature.size() != 64 || !rawOut) {
        return false;
    }

    *rawOut = compactSignature;
    return true;
}

/**
 * @brief kernelSigRawToCompact
 * @param rawSignature
 * @param compactOut
 * @return
 */
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

/**
 * @brief kernelSigCompactToRaw
 * @param compactSignature
 * @param rawOut
 * @return
 */
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

/**
 * @brief verifyPartialSignature
 * @param signature
 * @param messageHash
 * @param pubnonceTotal
 * @param pubkey
 * @param pubkeyTotal
 * @return
 */
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
