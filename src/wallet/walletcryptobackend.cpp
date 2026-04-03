#include "walletcryptobackend.h"

#include "walletcryptoaggsighelpers.h"
#include "walletcryptobasehelpers.h"
#include "walletcryptocommitmenthelpers.h"
#include "walletcryptosecphelpers.h"
#include "walletcryptokernelhelpers.h"
#include "walletcryptosignaturehelpers.h"
#include "walletcryptoslatepackhelpers.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QStringList>

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
    const QByteArray blindSecret = WalletCryptoHelpers::deriveSigningBaseSecret(walletFingerprint, workflowId, roleTag);
    QByteArray nonceSecret = WalletCryptoHelpers::deriveAggsigSecnonce(walletFingerprint, workflowId, roleTag);
    if (nonceSecret.size() != 32) {
        nonceSecret = WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("nonce"), walletFingerprint, entropy);
    }
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createParticipantFromBlindSecret(
    const QString &blindSecretHex,
    const QString &walletFingerprint,
    const QString &workflowId,
    const QString &roleTag)
{
    ParticipantContext context;
    const QByteArray blindSecret = WalletCryptoHelpers::fromHex(blindSecretHex.trimmed());
    if (blindSecret.size() != 32
        || secp256k1_ec_seckey_verify(WalletCryptoHelpers::walletSecpContext(),
                                      reinterpret_cast<const unsigned char *>(blindSecret.constData())) != 1) {
        return context;
    }

    const QString entropy = walletFingerprint + QLatin1Char(':') + workflowId + QLatin1Char(':') + roleTag;
    QByteArray nonceSecret = WalletCryptoHelpers::deriveAggsigSecnonce(walletFingerprint, workflowId, roleTag);
    if (nonceSecret.size() != 32) {
        nonceSecret = WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("nonce"), walletFingerprint, entropy);
    }

    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createRandomParticipant(const QString &roleTag)
{
    const QString entropy = QStringLiteral("%1:%2").arg(roleTag, randomHex(32));
    const QByteArray blindSecret =
        WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("random-blind"), entropy, QStringLiteral("blind"));
    QByteArray nonceSecret = WalletCryptoHelpers::deriveAggsigSecnonce(QStringLiteral("random"), entropy, roleTag);
    if (nonceSecret.size() != 32) {
        nonceSecret =
            WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("random-nonce"), entropy, QStringLiteral("nonce"));
    }

    ParticipantContext context;
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

QString WalletCryptoBackend::createOffset(const QString &walletFingerprint, const QString &workflowId)
{
    return QString::fromUtf8(
        WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("offset"), walletFingerprint, workflowId).toHex());
}

QString WalletCryptoBackend::addOffsets(const QString &leftOffset, const QString &rightOffset, QString *errorOut)
{
    const QByteArray left = WalletCryptoHelpers::fromHex(leftOffset);
    const QByteArray right = WalletCryptoHelpers::fromHex(rightOffset);
    QByteArray sum;
    if (!WalletCryptoHelpers::addScalars(left, right, &sum)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to add transaction offsets.");
        }
        return QString();
    }
    return QString::fromUtf8(sum.toHex());
}

QString WalletCryptoBackend::negateScalar(const QString &value, QString *errorOut)
{
    QByteArray scalar = WalletCryptoHelpers::fromHex(value);
    if (scalar.size() != 32) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid scalar length.");
        }
        return QString();
    }
    if (secp256k1_ec_privkey_negate(WalletCryptoHelpers::walletSecpContext(),
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
        const QByteArray blind = WalletCryptoHelpers::fromHex(positiveBlinds.at(i));
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
        if (!WalletCryptoHelpers::addScalars(combined, blind, &combined)) {
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
        const QByteArray blind = WalletCryptoHelpers::fromHex(negativeBlinds.at(i));
        if (blind.size() != 32) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid negative blinding factor.");
            }
            return QString();
        }
        if (!WalletCryptoHelpers::subtractScalars(combined, blind, &combined)) {
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
    if (!WalletCryptoHelpers::createCommitmentAndRangeproof(
            walletFingerprint, workflowId, roleTag, amount, &result.commit)) {
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
    const quint64 value = WalletCryptoHelpers::amountToNanogrin(amount);
    const WalletKeychain::OutputSecrets secrets = keychain.deriveOutputSecrets(childIndex, value);
    if (!secrets.success) {
        return owned;
    }

    if (!WalletCryptoHelpers::createCommitmentAndRangeproofFromSecrets(secrets.blindingFactor,
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

QString WalletCryptoBackend::slatepackAddress(const WalletKeychain &keychain, const QString &networkName)
{
    return WalletCryptoSlatepackHelpers::slatepackAddress(keychain, networkName);
}

QString WalletCryptoBackend::paymentProofAddress(const WalletKeychain &keychain)
{
    return WalletCryptoSlatepackHelpers::paymentProofAddress(keychain);
}

SlateV4::ParticipantData WalletCryptoBackend::createParticipantData(const ParticipantContext &context)
{
    SlateV4::ParticipantData data;
    data.xs = context.blindPublic;
    data.nonce = context.noncePublic;
    return data;
}

SlateV4::PaymentProof WalletCryptoBackend::createPaymentProof(const ParticipantContext &sender,
                                                              const ParticipantContext &receiver)
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
    return WalletCryptoSlatepackHelpers::signPaymentProof(slate, keychain, errorOut);
}

bool WalletCryptoBackend::verifyPaymentProof(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSlatepackHelpers::verifyPaymentProof(slate, errorOut);
}

bool WalletCryptoBackend::applyRound2Signature(SlateV4 *slate,
                                               const QString &walletFingerprint,
                                               const QString &roleTag,
                                               const ParticipantContext *overrideContext,
                                               QString *errorOut)
{
    return WalletCryptoSignatureHelpers::applyRound2Signature(
        slate, walletFingerprint, roleTag, overrideContext, errorOut);
}

bool WalletCryptoBackend::verifyPartialSignatures(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::verifyPartialSignatures(slate, errorOut);
}

QString WalletCryptoBackend::calculateExcessCommitment(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::calculateExcessCommitment(slate, errorOut);
}

QString WalletCryptoBackend::kernelSignatureMessageHex(const SlateV4 &slate)
{
    return WalletCryptoSignatureHelpers::kernelSignatureMessageHex(slate);
}

QString WalletCryptoBackend::combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::combinedBlindPublicKeyHex(slate, errorOut);
}

QString WalletCryptoBackend::combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::combinedNoncePublicKeyHex(slate, errorOut);
}

bool WalletCryptoBackend::buildFinalSignature(const SlateV4 &slate,
                                              QString *finalSignatureOut,
                                              QString *errorOut)
{
    return WalletCryptoSignatureHelpers::buildFinalSignature(slate, finalSignatureOut, errorOut);
}

bool WalletCryptoBackend::finalizeSlate(SlateV4 *slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::finalizeSlate(slate, errorOut);
}

QString WalletCryptoBackend::inputOrderHash(const Input &input)
{
    return WalletCryptoKernelHelpers::inputOrderHash(input);
}

QString WalletCryptoBackend::outputOrderHash(const Output &output)
{
    return WalletCryptoKernelHelpers::outputOrderHash(output);
}

QString WalletCryptoBackend::kernelOrderHash(const TxKernel &kernel)
{
    return WalletCryptoKernelHelpers::kernelOrderHash(kernel);
}

bool WalletCryptoBackend::validateTransactionBody(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionBody(tx, errorOut);
}

bool WalletCryptoBackend::validateTransactionKernelSums(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionKernelSums(tx, errorOut);
}

bool WalletCryptoBackend::validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionKernelSignatures(tx, errorOut);
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
