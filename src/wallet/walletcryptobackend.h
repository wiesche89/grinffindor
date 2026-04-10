#ifndef WALLETCRYPTOBACKEND_H
#define WALLETCRYPTOBACKEND_H

#include <QString>

#include "slatev4.h"
#include "walletkeychain.h"

class Transaction;
class Input;
class Output;
class TxKernel;

class WalletCryptoBackend
{
public:
    struct ParticipantContext {
        QString role;
        QString blindSecret;
        QString nonceSecret;
        QString blindPublic;
        QString noncePublic;
        QString address;
    };

    struct OwnedCommitment {
        bool success = false;
        SlateV4::Commit commit;
        QString blindingFactor;
        QString keyPath;
        quint32 childIndex = 0;
    };

    struct CommitmentResult {
        bool success = false;
        SlateV4::Commit commit;
        QString error;
    };

/**
 * @brief Returns whether real grin transactions.
 */
    static bool supportsRealGrinTransactions();
    static ParticipantContext createParticipant(const QString &walletFingerprint,
                                                const QString &workflowId,
                                                const QString &roleTag);
    static ParticipantContext createParticipantFromBlindSecret(const QString &blindSecretHex,
                                                               const QString &walletFingerprint,
                                                               const QString &workflowId,
                                                               const QString &roleTag);
/**
 * @brief Creates create random participant.
 */
    static ParticipantContext createRandomParticipant(const QString &roleTag);
/**
 * @brief Creates create offset.
 */
    static QString createOffset(const QString &walletFingerprint, const QString &workflowId);
    static QString addOffsets(const QString &leftOffset,
                              const QString &rightOffset,
                              QString *errorOut = 0);
/**
 * @brief Returns negate scalar.
 */
    static QString negateScalar(const QString &value, QString *errorOut = 0);
    static QString combineBlindingFactors(const QStringList &positiveBlinds,
                                          const QStringList &negativeBlinds = QStringList(),
                                          QString *errorOut = 0);
    static CommitmentResult createCommitment(const QString &walletFingerprint,
                                             const QString &workflowId,
                                             const QString &roleTag,
                                             const QString &amount);
    static OwnedCommitment createOwnedCommitment(const WalletKeychain &keychain,
                                                 quint32 childIndex,
                                                 const QString &amount);
    static QString slatepackAddress(const WalletKeychain &keychain,
                                    const QString &networkName = QString());
/**
 * @brief Returns payment proof address.
 */
    static QString paymentProofAddress(const WalletKeychain &keychain);
/**
 * @brief Creates create participant data.
 */
    static SlateV4::ParticipantData createParticipantData(const ParticipantContext &context);
/**
 * @brief Creates create payment proof.
 */
    static SlateV4::PaymentProof createPaymentProof(const ParticipantContext &sender, const ParticipantContext &receiver);
    static bool signPaymentProof(SlateV4 *slate,
                                 const WalletKeychain &keychain,
                                 QString *errorOut = 0);
/**
 * @brief Validates verify payment proof.
 */
    static bool verifyPaymentProof(const SlateV4 &slate, QString *errorOut = 0);
    static bool applyRound2Signature(SlateV4 *slate,
                                     const QString &walletFingerprint,
                                     const QString &roleTag,
                                     const ParticipantContext *overrideContext,
                                     QString *errorOut = 0);
/**
 * @brief Validates verify partial signatures.
 */
    static bool verifyPartialSignatures(const SlateV4 &slate, QString *errorOut = 0);
/**
 * @brief Computes calculate excess commitment.
 */
    static QString calculateExcessCommitment(const SlateV4 &slate, QString *errorOut = 0);
/**
 * @brief Returns kernel signature message hex.
 */
    static QString kernelSignatureMessageHex(const SlateV4 &slate);
/**
 * @brief Returns combined blind public key hex.
 */
    static QString combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut = 0);
/**
 * @brief Returns combined nonce public key hex.
 */
    static QString combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut = 0);
    static bool buildFinalSignature(const SlateV4 &slate,
                                    QString *finalSignatureOut,
                                    QString *errorOut = 0);
/**
 * @brief Finalizes slate.
 */
    static bool finalizeSlate(SlateV4 *slate, QString *errorOut = 0);
/**
 * @brief Returns input order hash.
 */
    static QString inputOrderHash(const Input &input);
/**
 * @brief Returns output order hash.
 */
    static QString outputOrderHash(const Output &output);
/**
 * @brief Returns kernel order hash.
 */
    static QString kernelOrderHash(const TxKernel &kernel);
/**
 * @brief Validates validate transaction body.
 */
    static bool validateTransactionBody(const Transaction &tx, QString *errorOut = 0);
/**
 * @brief Validates validate transaction kernel sums.
 */
    static bool validateTransactionKernelSums(const Transaction &tx, QString *errorOut = 0);
/**
 * @brief Validates validate transaction kernel signatures.
 */
    static bool validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut = 0);
/**
 * @brief Processes describe backend.
 */
    static QString describeBackend();

private:
/**
 * @brief Processes random hex.
 */
    static QString randomHex(int bytes);
/**
 * @brief Returns whether h hex.
 */
    static QString hashHex(const QString &input);
};

#endif // WALLETCRYPTOBACKEND_H
