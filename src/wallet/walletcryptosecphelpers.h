#ifndef WALLETCRYPTOSECPHELPERS_H
#define WALLETCRYPTOSECPHELPERS_H

#include <QByteArray>
#include <QList>
#include <QString>

extern "C" {
#include "secp256k1.h"
#include "secp256k1_aggsig.h"
#include "secp256k1_bulletproofs.h"
}

namespace WalletCryptoHelpers
{
/**
 * @brief Processes wallet secp context.
 */
secp256k1_context *walletSecpContext();
/**
 * @brief Processes wallet bulletproof generators.
 */
secp256k1_bulletproof_generators *walletBulletproofGenerators();

/**
 * @brief Computes derive valid secret bytes.
 */
QByteArray deriveValidSecretBytes(const QString &domain, const QString &left, const QString &right);
QByteArray deriveSigningBaseSecret(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag);
QByteArray deriveAggsigSecnonce(const QString &walletFingerprint,
                                const QString &workflowId,
                                const QString &roleTag,
                                const QString &nonceEntropy = QString());

/**
 * @brief Creates create compressed pubkey hex.
 */
QString createCompressedPubkeyHex(const QByteArray &secretKey);
/**
 * @brief Parses parse pubkey.
 */
bool parsePubkey(const QString &hex, secp256k1_pubkey *pubkey);
/**
 * @brief Returns serialize pubkey.
 */
QString serializePubkey(const secp256k1_pubkey &pubkey);
/**
 * @brief Returns combine pubkeys.
 */
bool combinePubkeys(const QList<QString> &hexPubkeys, secp256k1_pubkey *combined);

/**
 * @brief Returns amount to nanogrin.
 */
quint64 amountToNanogrin(const QString &amount);
/**
 * @brief Returns add scalars.
 */
bool addScalars(const QByteArray &left, const QByteArray &right, QByteArray *sumOut);
/**
 * @brief Returns subtract scalars.
 */
bool subtractScalars(const QByteArray &left, const QByteArray &right, QByteArray *differenceOut);
}

#endif // WALLETCRYPTOSECPHELPERS_H
