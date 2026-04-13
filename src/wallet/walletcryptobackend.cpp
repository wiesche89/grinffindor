#include "walletcryptobackend.h"

#include "walletcryptoaggsighelpers.h"
#include "walletcryptobasehelpers.h"
#include "walletcryptocommitmenthelpers.h"
#include "walletcryptosecphelpers.h"
#include "walletcryptokernelhelpers.h"
#include "walletcryptosignaturehelpers.h"
#include "walletcryptoslatepackhelpers.h"
#include "walletsecurerandom.h"

#include <QCryptographicHash>
#include <QStringList>

// -------------------------------------------------------------------------------------------------------
// Backend Capability And Participant Context
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Indicates whether the backend supports real Grin transaction primitives.
 * @return
 */
bool WalletCryptoBackend::supportsRealGrinTransactions()
{
    return true;
}

/**
 * @brief Creates participant.
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @return
 */
WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createParticipant(const QString &walletFingerprint,
                                                                              const QString &workflowId,
                                                                              const QString &roleTag)
{
    ParticipantContext context;
    const QString entropy = walletFingerprint + QLatin1Char(':') + workflowId + QLatin1Char(':') + roleTag;
    const QByteArray blindSecret = WalletCryptoHelpers::deriveSigningBaseSecret(walletFingerprint, workflowId, roleTag);
    context.nonceEntropy = randomHex(16);

    QByteArray nonceSecret =
        WalletCryptoHelpers::deriveAggsigSecnonce(walletFingerprint, workflowId, roleTag, context.nonceEntropy);
    if (nonceSecret.size() != 32) {
        nonceSecret = WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("nonce"),
                                                                 walletFingerprint,
                                                                 entropy + QLatin1Char(':') + context.nonceEntropy);
    }
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

/**
 * @brief Creates participant from blind secret.
 * @param blindSecretHex
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @return
 */
WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createParticipantFromBlindSecret(
    const QString &blindSecretHex,
    const QString &walletFingerprint,
    const QString &workflowId,
    const QString &roleTag,
    const QString &nonceEntropy)
{
    ParticipantContext context;
    const QByteArray blindSecret = WalletCryptoHelpers::fromHex(blindSecretHex.trimmed());
    if (blindSecret.size() != 32

        || secp256k1_ec_seckey_verify(WalletCryptoHelpers::walletSecpContext(),
                                      reinterpret_cast<const unsigned char *>(blindSecret.constData())) != 1) {
        return context;
    }

    const QString entropy = walletFingerprint + QLatin1Char(':') + workflowId + QLatin1Char(':') + roleTag;
    context.nonceEntropy = nonceEntropy.trimmed();

    QByteArray nonceSecret =
        WalletCryptoHelpers::deriveAggsigSecnonce(walletFingerprint, workflowId, roleTag, context.nonceEntropy);
    if (nonceSecret.size() != 32) {
        nonceSecret = WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("nonce"),
                                                                 walletFingerprint,
                                                                 context.nonceEntropy.isEmpty()
                                                                     ? entropy
                                                                     : (entropy + QLatin1Char(':') + context.nonceEntropy));
    }

    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

/**
 * @brief Creates random participant.
 * @param roleTag
 * @return
 */
WalletCryptoBackend::ParticipantContext WalletCryptoBackend::createRandomParticipant(const QString &roleTag)
{
    const QString entropy = QStringLiteral("%1:%2").arg(roleTag, randomHex(32));
    const QByteArray blindSecret =
        WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("random-blind"), entropy, QStringLiteral("blind"));

    const QString nonceEntropy = randomHex(16);
    QByteArray nonceSecret =
        WalletCryptoHelpers::deriveAggsigSecnonce(QStringLiteral("random"), entropy, roleTag, nonceEntropy);
    if (nonceSecret.size() != 32) {
        nonceSecret =
            WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("random-nonce"),
                                                        entropy,
                                                        QStringLiteral("nonce:") + nonceEntropy);
    }

    ParticipantContext context;
    context.role = roleTag;
    context.blindSecret = QString::fromUtf8(blindSecret.toHex());
    context.nonceSecret = QString::fromUtf8(nonceSecret.toHex());
    context.nonceEntropy = nonceEntropy;
    context.blindPublic = WalletCryptoHelpers::createCompressedPubkeyHex(blindSecret);
    context.noncePublic = WalletCryptoHelpers::createCompressedPubkeyHex(nonceSecret);
    context.address = hashHex(QStringLiteral("addr:") + entropy);
    return context;
}

/**
 * @brief Creates offset.
 * @param walletFingerprint
 * @param workflowId
 * @return
 */
QString WalletCryptoBackend::createOffset(const QString &walletFingerprint, const QString &workflowId)
{
    return QString::fromUtf8(
        WalletCryptoHelpers::deriveValidSecretBytes(QStringLiteral("offset"), walletFingerprint, workflowId).toHex());
}

/**
 * @brief Adds two transaction offset scalars.
 * @param leftOffset
 * @param rightOffset
 * @param errorOut
 * @return
 */
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

/**
 * @brief Negates a scalar value modulo secp256k1 curve order.
 * @param value
 * @param errorOut
 * @return
 */
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

/**
 * @brief Combines blinding factors.
 * @param positiveBlinds
 * @param negativeBlinds
 * @param errorOut
 * @return
 */
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

// -------------------------------------------------------------------------------------------------------
// Commitment, Signature, And Slate Helpers
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Creates commitment.
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @param amount
 * @return
 */
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

/**
 * @brief Creates owned commitment.
 * @param keychain
 * @param childIndex
 * @param amount
 * @return
 */
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

/**
 * @brief Derives the slatepack address for the provided keychain and network.
 * @param keychain
 * @param networkName
 * @return
 */
QString WalletCryptoBackend::slatepackAddress(const WalletKeychain &keychain, const QString &networkName)
{
    return WalletCryptoSlatepackHelpers::slatepackAddress(keychain, networkName);
}

/**
 * @brief Derives the payment-proof address for the provided keychain.
 * @param keychain
 * @return
 */
QString WalletCryptoBackend::paymentProofAddress(const WalletKeychain &keychain)
{
    return WalletCryptoSlatepackHelpers::paymentProofAddress(keychain);
}

/**
 * @brief Creates participant data.
 * @param context
 * @return
 */
SlateV4::ParticipantData WalletCryptoBackend::createParticipantData(const ParticipantContext &context)
{
    SlateV4::ParticipantData data;
    data.xs = context.blindPublic;
    data.nonce = context.noncePublic;
    return data;
}

/**
 * @brief Creates payment proof.
 * @param sender
 * @param receiver
 * @return
 */
SlateV4::PaymentProof WalletCryptoBackend::createPaymentProof(const ParticipantContext &sender,
                                                              const ParticipantContext &receiver)
{
    SlateV4::PaymentProof proof;
    proof.senderAddress = sender.address;
    proof.receiverAddress = receiver.address;
    return proof;
}

/**
 * @brief Signs payment proof fields for the provided slate.
 * @param slate
 * @param keychain
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::signPaymentProof(SlateV4 *slate,
                                           const WalletKeychain &keychain,
                                           QString *errorOut)
{
    return WalletCryptoSlatepackHelpers::signPaymentProof(slate, keychain, errorOut);
}

/**
 * @brief Verifies payment proof.
 * @param slate
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::verifyPaymentProof(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSlatepackHelpers::verifyPaymentProof(slate, errorOut);
}

/**
 * @brief Applies round-2 aggsig partial signature to a slate.
 * @param slate
 * @param walletFingerprint
 * @param roleTag
 * @param overrideContext
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::applyRound2Signature(SlateV4 *slate,
                                               const QString &walletFingerprint,
                                               const QString &roleTag,
                                               const ParticipantContext *overrideContext,
                                               QString *errorOut)
{
    return WalletCryptoSignatureHelpers::applyRound2Signature(
        slate, walletFingerprint, roleTag, overrideContext, errorOut);
}

/**
 * @brief Verifies partial signatures.
 * @param slate
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::verifyPartialSignatures(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::verifyPartialSignatures(slate, errorOut);
}

/**
 * @brief Calculates excess commitment.
 * @param slate
 * @param errorOut
 * @return
 */
QString WalletCryptoBackend::calculateExcessCommitment(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::calculateExcessCommitment(slate, errorOut);
}

/**
 * @brief Returns the kernel-signature message digest as hex.
 * @param slate
 * @return
 */
QString WalletCryptoBackend::kernelSignatureMessageHex(const SlateV4 &slate)
{
    return WalletCryptoSignatureHelpers::kernelSignatureMessageHex(slate);
}

/**
 * @brief Returns the combined participant blind public key as hex.
 * @param slate
 * @param errorOut
 * @return
 */
QString WalletCryptoBackend::combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::combinedBlindPublicKeyHex(slate, errorOut);
}

/**
 * @brief Returns the combined participant nonce public key as hex.
 * @param slate
 * @param errorOut
 * @return
 */
QString WalletCryptoBackend::combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::combinedNoncePublicKeyHex(slate, errorOut);
}

/**
 * @brief Builds final signature.
 * @param slate
 * @param finalSignatureOut
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::buildFinalSignature(const SlateV4 &slate,
                                              QString *finalSignatureOut,
                                              QString *errorOut)
{
    return WalletCryptoSignatureHelpers::buildFinalSignature(slate, finalSignatureOut, errorOut);
}

/**
 * @brief Finalizes slate.
 * @param slate
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::finalizeSlate(SlateV4 *slate, QString *errorOut)
{
    return WalletCryptoSignatureHelpers::finalizeSlate(slate, errorOut);
}

// -------------------------------------------------------------------------------------------------------
// Transaction Ordering And Validation
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Returns the deterministic ordering hash for a transaction input.
 * @param input
 * @return
 */
QString WalletCryptoBackend::inputOrderHash(const Input &input)
{
    return WalletCryptoKernelHelpers::inputOrderHash(input);
}

/**
 * @brief Returns the deterministic ordering hash for a transaction output.
 * @param output
 * @return
 */
QString WalletCryptoBackend::outputOrderHash(const Output &output)
{
    return WalletCryptoKernelHelpers::outputOrderHash(output);
}

/**
 * @brief Returns the deterministic ordering hash for a transaction kernel.
 * @param kernel
 * @return
 */
QString WalletCryptoBackend::kernelOrderHash(const TxKernel &kernel)
{
    return WalletCryptoKernelHelpers::kernelOrderHash(kernel);
}

/**
 * @brief Validates transaction body.
 * @param tx
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::validateTransactionBody(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionBody(tx, errorOut);
}

/**
 * @brief Validates transaction kernel sums.
 * @param tx
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::validateTransactionKernelSums(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionKernelSums(tx, errorOut);
}

/**
 * @brief Validates transaction kernel signatures.
 * @param tx
 * @param errorOut
 * @return
 */
bool WalletCryptoBackend::validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut)
{
    return WalletCryptoKernelHelpers::validateTransactionKernelSignatures(tx, errorOut);
}

/**
 * @brief Returns the backend implementation identifier.
 * @return
 */
QString WalletCryptoBackend::describeBackend()
{
    return QStringLiteral("local-secp256k1-zkp-aggsig");
}

/**
 * @brief Generates random bytes and returns them as hex text.
 * @param bytes
 * @return
 */
QString WalletCryptoBackend::randomHex(int bytes)
{
    const QByteArray data = WalletSecureRandom::bytes(bytes);
    return QString::fromUtf8(data.toHex());
}

/**
 * @brief Computes SHA-256 hash of input text and returns hex encoding.
 * @param input
 * @return
 */
QString WalletCryptoBackend::hashHex(const QString &input)
{
    return QString::fromUtf8(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256).toHex());
}
