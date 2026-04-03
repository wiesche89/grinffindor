#ifndef WALLETCRYPTOBASEHELPERS_H
#define WALLETCRYPTOBASEHELPERS_H

#include <QByteArray>
#include <QString>

class SlateV4;

namespace WalletCryptoHelpers
{
QByteArray hashBytes(const QByteArray &input);
QString toHex(const unsigned char *data, int size);
QString bech32Encode(const QString &hrp, const QByteArray &payload);

void appendU8(QByteArray &out, quint8 value);
void appendU16(QByteArray &out, quint16 value);
void appendU64(QByteArray &out, quint64 value);

QByteArray fromHex(const QString &hex);
QByteArray paymentProofMessage(const SlateV4 &slate);
}

#endif // WALLETCRYPTOBASEHELPERS_H
