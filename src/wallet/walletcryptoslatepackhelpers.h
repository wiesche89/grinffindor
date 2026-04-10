#ifndef WALLETCRYPTOSLATEPACKHELPERS_H
#define WALLETCRYPTOSLATEPACKHELPERS_H

#include <QString>

#include "slatev4.h"
#include "walletkeychain.h"

namespace WalletCryptoSlatepackHelpers
{
/**
 * @brief Returns slatepack address.
 */
QString slatepackAddress(const WalletKeychain &keychain, const QString &networkName);
/**
 * @brief Returns payment proof address.
 */
QString paymentProofAddress(const WalletKeychain &keychain);
/**
 * @brief Signs sign payment proof.
 */
bool signPaymentProof(SlateV4 *slate, const WalletKeychain &keychain, QString *errorOut);
/**
 * @brief Validates verify payment proof.
 */
bool verifyPaymentProof(const SlateV4 &slate, QString *errorOut);
}

#endif // WALLETCRYPTOSLATEPACKHELPERS_H
