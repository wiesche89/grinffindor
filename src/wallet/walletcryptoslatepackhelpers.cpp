#include "walletcryptoslatepackhelpers.h"

#include "walletcryptobasehelpers.h"

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
extern "C" {
#include "monocypher.h"
}
#endif

namespace WalletCryptoSlatepackHelpers
{

QString slatepackAddress(const WalletKeychain &keychain, const QString &networkName)
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
    return WalletCryptoHelpers::bech32Encode(hrp, publicKey);
#else
    Q_UNUSED(keychain);
    Q_UNUSED(networkName);
    return QString();
#endif
}

QString paymentProofAddress(const WalletKeychain &keychain)
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

bool signPaymentProof(SlateV4 *slate, const WalletKeychain &keychain, QString *errorOut)
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

    const QByteArray message = WalletCryptoHelpers::paymentProofMessage(*slate);
    unsigned char signature[64];
    crypto_eddsa_sign(signature,
                      reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(message.constData()),
                      static_cast<size_t>(message.size()));
    slate->paymentProof.receiverAddress = receiverAddress;
    slate->paymentProof.receiverSignature = WalletCryptoHelpers::toHex(signature, sizeof(signature));
    return true;
#else
    Q_UNUSED(keychain);
    if (errorOut) {
        *errorOut = QStringLiteral("This build does not support Slatepack payment proof signatures.");
    }
    return false;
#endif
}

bool verifyPaymentProof(const SlateV4 &slate, QString *errorOut)
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

    const QByteArray message = WalletCryptoHelpers::paymentProofMessage(slate);
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

}
