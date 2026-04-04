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
/**
 * @brief Finalizes slate.
 */
bool finalizeSlate(SlateV4 *slate, QString *errorOut);
/**
 * @brief Validates verify partial signatures.
 */
bool verifyPartialSignatures(const SlateV4 &slate, QString *errorOut);
/**
 * @brief Computes calculate excess commitment.
 */
QString calculateExcessCommitment(const SlateV4 &slate, QString *errorOut);
/**
 * @brief Returns kernel signature message hex.
 */
QString kernelSignatureMessageHex(const SlateV4 &slate);
/**
 * @brief Returns combined blind public key hex.
 */
QString combinedBlindPublicKeyHex(const SlateV4 &slate, QString *errorOut);
/**
 * @brief Returns combined nonce public key hex.
 */
QString combinedNoncePublicKeyHex(const SlateV4 &slate, QString *errorOut);
/**
 * @brief Builds final signature.
 */
bool buildFinalSignature(const SlateV4 &slate, QString *finalSignatureOut, QString *errorOut);
}

#endif // WALLETCRYPTOSIGNATUREHELPERS_H
