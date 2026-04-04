#ifndef WALLETCRYPTOBASEHELPERS_H
#define WALLETCRYPTOBASEHELPERS_H

#include <QByteArray>
#include <QString>

class SlateV4;

namespace WalletCryptoHelpers
{
/**
 * @brief Returns whether h bytes.
 */
QByteArray hashBytes(const QByteArray &input);
/**
 * @brief Returns to hex.
 */
QString toHex(const unsigned char *data, int size);
/**
 * @brief Returns bech32 encode.
 */
QString bech32Encode(const QString &hrp, const QByteArray &payload);

/**
 * @brief Processes append u8.
 */
void appendU8(QByteArray &out, quint8 value);
/**
 * @brief Processes append u16.
 */
void appendU16(QByteArray &out, quint16 value);
/**
 * @brief Processes append u64.
 */
void appendU64(QByteArray &out, quint64 value);

/**
 * @brief Returns from hex.
 */
QByteArray fromHex(const QString &hex);
/**
 * @brief Returns payment proof message.
 */
QByteArray paymentProofMessage(const SlateV4 &slate);
}

#endif // WALLETCRYPTOBASEHELPERS_H
