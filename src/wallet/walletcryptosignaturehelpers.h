#ifndef WALLETCRYPTOSIGNATUREHELPERS_H
#define WALLETCRYPTOSIGNATUREHELPERS_H

#include <QString>

#include "slatev4.h"
#include "walletcryptobackend.h"

namespace WalletCryptoSignatureHelpers
{
bool applyRound2Signature(SlateV4 *slate,
                          const QString &walletFingerprint,
                          const QString &roleTag,
                          const WalletCryptoBackend::ParticipantContext *overrideContext,
                          QString *errorOut);
bool finalizeSlate(SlateV4 *slate, QString *errorOut);
bool verifyPartialSignatures(const SlateV4 &slate, QString *errorOut);
QString calculateExcessCommitment(const SlateV4 &slate, QString *errorOut);
QString kernelSignatureMessageHex(const SlateV4 &slate);
QString combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut);
QString combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut);
bool buildFinalSignature(const SlateV4 &slate, QString *finalSignatureOut, QString *errorOut);
}

#endif // WALLETCRYPTOSIGNATUREHELPERS_H
