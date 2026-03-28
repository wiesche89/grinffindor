#ifndef WALLETCRYPTOBACKEND_H
#define WALLETCRYPTOBACKEND_H

#include <QString>

#include "slatev4.h"
#include "walletkeychain.h"

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

    static bool supportsRealGrinTransactions();
    static ParticipantContext createParticipant(const QString &walletFingerprint,
                                                const QString &workflowId,
                                                const QString &roleTag);
    static QString createOffset(const QString &walletFingerprint, const QString &workflowId);
    static QString addOffsets(const QString &leftOffset,
                              const QString &rightOffset,
                              QString *errorOut = 0);
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
    static QString slatepackAddress(const WalletKeychain &keychain);
    static QString paymentProofAddress(const WalletKeychain &keychain);
    static SlateV4::ParticipantData createParticipantData(const ParticipantContext &context);
    static SlateV4::PaymentProof createPaymentProof(const ParticipantContext &sender, const ParticipantContext &receiver);
    static bool signPaymentProof(SlateV4 *slate,
                                 const WalletKeychain &keychain,
                                 QString *errorOut = 0);
    static bool verifyPaymentProof(const SlateV4 &slate, QString *errorOut = 0);
    static bool applyRound2Signature(SlateV4 *slate,
                                     const QString &walletFingerprint,
                                     const QString &roleTag,
                                     QString *errorOut = 0);
    static bool finalizeSlate(SlateV4 *slate, QString *errorOut = 0);
    static QString describeBackend();

private:
    static QString randomHex(int bytes);
    static QString hashHex(const QString &input);
};

#endif // WALLETCRYPTOBACKEND_H
