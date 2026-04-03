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
secp256k1_context *walletSecpContext();
secp256k1_bulletproof_generators *walletBulletproofGenerators();

QByteArray deriveValidSecretBytes(const QString &domain, const QString &left, const QString &right);
QByteArray deriveSigningBaseSecret(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag);
QByteArray deriveAggsigSecnonce(const QString &walletFingerprint,
                                const QString &workflowId,
                                const QString &roleTag);

QString createCompressedPubkeyHex(const QByteArray &secretKey);
bool parsePubkey(const QString &hex, secp256k1_pubkey *pubkey);
QString serializePubkey(const secp256k1_pubkey &pubkey);
bool combinePubkeys(const QList<QString> &hexPubkeys, secp256k1_pubkey *combined);

quint64 amountToNanogrin(const QString &amount);
bool addScalars(const QByteArray &left, const QByteArray &right, QByteArray *sumOut);
bool subtractScalars(const QByteArray &left, const QByteArray &right, QByteArray *differenceOut);
}

#endif // WALLETCRYPTOSECPHELPERS_H
