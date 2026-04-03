#ifndef WALLETCRYPTOSLATEPACKHELPERS_H
#define WALLETCRYPTOSLATEPACKHELPERS_H

#include <QString>

#include "slatev4.h"
#include "walletkeychain.h"

namespace WalletCryptoSlatepackHelpers
{
QString slatepackAddress(const WalletKeychain &keychain, const QString &networkName);
QString paymentProofAddress(const WalletKeychain &keychain);
bool signPaymentProof(SlateV4 *slate, const WalletKeychain &keychain, QString *errorOut);
bool verifyPaymentProof(const SlateV4 &slate, QString *errorOut);
}

#endif // WALLETCRYPTOSLATEPACKHELPERS_H
