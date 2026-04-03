#include "walletcryptosignaturehelpers.h"

#include "walletcryptoaggsighelpers.h"
#include "walletcryptobasehelpers.h"
#include "walletcryptocommitmenthelpers.h"
#include "walletcryptosecphelpers.h"

namespace WalletCryptoSignatureHelpers
{

/**
 * @brief applyRound2Signature
 * @param slate
 * @param walletFingerprint
 * @param roleTag
 * @param overrideContext
 * @param errorOut
 * @return
 */
bool applyRound2Signature(SlateV4 *slate,
                          const QString &walletFingerprint,
                          const QString &roleTag,
                          const WalletCryptoBackend::ParticipantContext *overrideContext,
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

    WalletCryptoBackend::ParticipantContext context =
        WalletCryptoBackend::createParticipant(walletFingerprint, slate->workflowId(), roleTag);
    if (overrideContext) {
        context = *overrideContext;
    }
    const int existingIndex = WalletCryptoHelpers::findParticipantIndex(*slate, context.blindPublic);
    if (existingIndex < 0) {
        slate->signatures.append(WalletCryptoBackend::createParticipantData(context));
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
    if (!WalletCryptoHelpers::combinePubkeys(blindPubkeys, &totalBlind)
        || !WalletCryptoHelpers::combinePubkeys(noncePubkeys, &totalNonce)
        || !WalletCryptoHelpers::parsePubkey(context.blindPublic, &ownBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to build aggregated public keys.");
        }
        return false;
    }

    const QByteArray messageHash = WalletCryptoHelpers::buildKernelSignatureMessage(*slate);
    QByteArray partialSignature;
    if (!WalletCryptoHelpers::createPartialSignature(messageHash,
                                                     WalletCryptoHelpers::fromHex(context.blindSecret),
                                                     WalletCryptoHelpers::fromHex(context.nonceSecret),
                                                     totalNonce,
                                                     totalBlind,
                                                     &partialSignature)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to create aggsig partial.");
        }
        return false;
    }

    if (!WalletCryptoHelpers::verifyPartialSignature(partialSignature,
                                                     messageHash,
                                                     totalNonce,
                                                     ownBlind,
                                                     totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Aggsig partial verification failed.");
        }
        return false;
    }

    const int participantIndex = WalletCryptoHelpers::findParticipantIndex(*slate, context.blindPublic);
    if (participantIndex < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Participant entry could not be located.");
        }
        return false;
    }

    QByteArray compactPartialSignature;
    if (!WalletCryptoHelpers::aggsigRawToCompact(partialSignature, &compactPartialSignature)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to serialize aggsig partial.");
        }
        return false;
    }

    slate->signatures[participantIndex].part = QString::fromUtf8(compactPartialSignature.toHex());
    slate->metadata.insert(QStringLiteral("message_hash"), QString::fromUtf8(messageHash.toHex()));
    slate->metadata.insert(QStringLiteral("pubkey_total"), WalletCryptoHelpers::serializePubkey(totalBlind));
    slate->metadata.insert(QStringLiteral("pubnonce_total"), WalletCryptoHelpers::serializePubkey(totalNonce));
    slate->metadata.insert(QStringLiteral("signature_status"), QStringLiteral("partial"));
    return true;
}

/**
 * @brief verifyPartialSignatures
 * @param slate
 * @param errorOut
 * @return
 */
bool verifyPartialSignatures(const SlateV4 &slate, QString *errorOut)
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
    if (!WalletCryptoHelpers::combinePubkeys(blindPubkeys, &totalBlind)
        || !WalletCryptoHelpers::combinePubkeys(noncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant public keys.");
        }
        return false;
    }

    const QByteArray messageHash = WalletCryptoHelpers::buildKernelSignatureMessage(slate);
    int verifiedCount = 0;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (participant.part.isEmpty()) {
            continue;
        }

        const QByteArray compactPartialBytes = WalletCryptoHelpers::fromHex(participant.part);
        if (compactPartialBytes.size() != 64) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid partial signature length.");
            }
            return false;
        }
        QByteArray partialBytes;
        if (!WalletCryptoHelpers::aggsigCompactToRaw(compactPartialBytes, &partialBytes)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid partial signature format.");
            }
            return false;
        }

        secp256k1_pubkey participantPubkey;
        if (!WalletCryptoHelpers::parsePubkey(participant.xs, &participantPubkey)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to parse participant public key.");
            }
            return false;
        }

        if (!WalletCryptoHelpers::verifyPartialSignature(partialBytes,
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

    return true;
}

/**
 * @brief calculateExcessCommitment
 * @param slate
 * @param errorOut
 * @return
 */
QString calculateExcessCommitment(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> blindPubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (!participant.xs.isEmpty()) {
            blindPubkeys.append(participant.xs);
        }
    }

    secp256k1_pubkey totalBlind;
    if (!WalletCryptoHelpers::combinePubkeys(blindPubkeys, &totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant excess keys.");
        }
        return QString();
    }

    secp256k1_pedersen_commitment commitment;
    if (secp256k1_pubkey_to_pedersen_commitment(WalletCryptoHelpers::walletSecpContext(), &commitment, &totalBlind) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to convert public blind sum into excess commitment.");
        }
        return QString();
    }

    unsigned char serialized[33];
    if (secp256k1_pedersen_commitment_serialize(WalletCryptoHelpers::walletSecpContext(), serialized, &commitment) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to serialize excess commitment.");
        }
        return QString();
    }

    return WalletCryptoHelpers::toHex(serialized, sizeof(serialized));
}

/**
 * @brief kernelSignatureMessageHex
 * @param slate
 * @return
 */
QString kernelSignatureMessageHex(const SlateV4 &slate)
{
    return QString::fromUtf8(WalletCryptoHelpers::buildKernelSignatureMessage(slate).toHex());
}

/**
 * @brief combinedBlindPublicKeyHex
 * @param slate
 * @param errorOut
 * @return
 */
QString combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> blindPubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (!participant.xs.isEmpty()) {
            blindPubkeys.append(participant.xs);
        }
    }

    secp256k1_pubkey totalBlind;
    if (!WalletCryptoHelpers::combinePubkeys(blindPubkeys, &totalBlind)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant blind public keys.");
        }
        return QString();
    }
    return WalletCryptoHelpers::serializePubkey(totalBlind);
}

/**
 * @brief combinedNoncePublicKeyHex
 * @param slate
 * @param errorOut
 * @return
 */
QString combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut)
{
    QList<QString> noncePubkeys;
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        if (!participant.nonce.isEmpty()) {
            noncePubkeys.append(participant.nonce);
        }
    }

    secp256k1_pubkey totalNonce;
    if (!WalletCryptoHelpers::combinePubkeys(noncePubkeys, &totalNonce)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine participant nonce public keys.");
        }
        return QString();
    }
    return WalletCryptoHelpers::serializePubkey(totalNonce);
}

/**
 * @brief buildFinalSignature
 * @param slate
 * @param finalSignatureOut
 * @param errorOut
 * @return
 */
bool buildFinalSignature(const SlateV4 &slate, QString *finalSignatureOut, QString *errorOut)
{
    QList<QString> allBlindPubkeys;
    QList<QString> allNoncePubkeys;
    int partialSignatureCount = 0;
    QVector<QByteArray> sigBuffers;

    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = slate.signatures.at(i);
        allBlindPubkeys.append(participant.xs);
        allNoncePubkeys.append(participant.nonce);
        if (!participant.part.isEmpty()) {
            ++partialSignatureCount;
            const QByteArray compactPartialBytes = WalletCryptoHelpers::fromHex(participant.part);
            if (compactPartialBytes.size() != 64) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Invalid partial signature length.");
                }
                return false;
            }
            QByteArray partialBytes;
            if (!WalletCryptoHelpers::aggsigCompactToRaw(compactPartialBytes, &partialBytes)) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Invalid partial signature format.");
                }
                return false;
            }
            sigBuffers.append(partialBytes);
        }
    }

    if (partialSignatureCount < slate.numParticipants) {
        if (errorOut) {
            *errorOut = QStringLiteral("Not enough partial signatures to finalize.");
        }
        return false;
    }

    if (sigBuffers.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate does not contain partial signatures.");
        }
        return false;
    }

    secp256k1_pubkey totalBlind;
    secp256k1_pubkey totalNonce;
    if (!WalletCryptoHelpers::combinePubkeys(allBlindPubkeys, &totalBlind)
        || !WalletCryptoHelpers::combinePubkeys(allNoncePubkeys, &totalNonce)) {
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
    if (secp256k1_aggsig_add_signatures_single(WalletCryptoHelpers::walletSecpContext(),
                                               finalSig,
                                               const_cast<const unsigned char **>(sigPointers.data()),
                                               static_cast<size_t>(sigPointers.size()),
                                               &totalNonce) != 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to combine aggsig partials.");
        }
        return false;
    }

    const QByteArray messageHash = WalletCryptoHelpers::buildKernelSignatureMessage(slate);
    if (secp256k1_aggsig_verify_single(WalletCryptoHelpers::walletSecpContext(),
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
        QByteArray compactFinalSignature;
        if (!WalletCryptoHelpers::kernelSigRawToCompact(
                QByteArray(reinterpret_cast<const char *>(finalSig), sizeof(finalSig)),
                &compactFinalSignature)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to serialize final aggsig signature.");
            }
            return false;
        }
        *finalSignatureOut = QString::fromUtf8(compactFinalSignature.toHex());
    }
    return true;
}

/**
 * @brief finalizeSlate
 * @param slate
 * @param errorOut
 * @return
 */
bool finalizeSlate(SlateV4 *slate, QString *errorOut)
{
    if (!slate) {
        if (errorOut) {
            *errorOut = QStringLiteral("Slate is missing.");
        }
        return false;
    }

    QString finalSignature;
    if (!buildFinalSignature(*slate, &finalSignature, errorOut)) {
        return false;
    }

    QString combineError;
    const QString blindTotal = combinedBlindPublicKeyHex(*slate, &combineError);
    if (blindTotal.isEmpty()) {
        if (errorOut && errorOut->isEmpty()) {
            *errorOut = combineError;
        }
        return false;
    }
    const QString nonceTotal = combinedNoncePublicKeyHex(*slate, &combineError);
    if (nonceTotal.isEmpty()) {
        if (errorOut && errorOut->isEmpty()) {
            *errorOut = combineError;
        }
        return false;
    }

    slate->metadata.insert(QStringLiteral("message_hash"),
                           QString::fromUtf8(WalletCryptoHelpers::buildKernelSignatureMessage(*slate).toHex()));
    slate->metadata.insert(QStringLiteral("pubkey_total"), blindTotal);
    slate->metadata.insert(QStringLiteral("pubnonce_total"), nonceTotal);
    slate->metadata.insert(QStringLiteral("final_sig"), finalSignature);
    slate->metadata.insert(QStringLiteral("signature_status"), QStringLiteral("finalized"));
    return true;
}

}
