#ifndef WALLETCRYPTOAGGSIGHELPERS_H
#define WALLETCRYPTOAGGSIGHELPERS_H

#include <QByteArray>
#include <QString>

class SlateV4;

extern "C" {
#include "secp256k1.h"
}

namespace WalletCryptoHelpers
{
QByteArray buildKernelSignatureMessageForFeature(const QString &feature,
                                                 quint64 fee,
                                                 quint64 lockHeight,
                                                 QString *errorOut);
QByteArray buildKernelSignatureMessage(const SlateV4 &slate);

int findParticipantIndex(const SlateV4 &slate, const QString &publicBlind);
bool createPartialSignature(const QByteArray &messageHash,
                            const QByteArray &seckey,
                            const QByteArray &secnonce,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkeyTotal,
                            QByteArray *signatureOut);
bool aggsigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut);
bool aggsigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut);
bool kernelSigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut);
bool kernelSigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut);
bool verifyPartialSignature(const QByteArray &signature,
                            const QByteArray &messageHash,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkey,
                            const secp256k1_pubkey &pubkeyTotal);
}

#endif // WALLETCRYPTOAGGSIGHELPERS_H
