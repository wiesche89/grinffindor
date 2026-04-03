#include "walletcryptohelpers.h"

#include <QCryptographicHash>
#include <QRandomGenerator>

namespace
{

const char kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/**
 * @brief convertBits
 * @param data
 * @param fromBits
 * @param toBits
 * @param pad
 * @return
 */
QVector<int> convertBits(const QByteArray &data, int fromBits, int toBits, bool pad)
{
    QVector<int> output;
    int accumulator = 0;
    int bitCount = 0;
    const int maxValue = (1 << toBits) - 1;
    const int maxAccumulator = (1 << (fromBits + toBits - 1)) - 1;

    for (int i = 0; i < data.size(); ++i) {
        const int value = static_cast<unsigned char>(data.at(i));
        if ((value >> fromBits) != 0) {
            return QVector<int>();
        }
        accumulator = ((accumulator << fromBits) | value) & maxAccumulator;
        bitCount += fromBits;
        while (bitCount >= toBits) {
            bitCount -= toBits;
            output.append((accumulator >> bitCount) & maxValue);
        }
    }

    if (pad) {
        if (bitCount > 0) {
            output.append((accumulator << (toBits - bitCount)) & maxValue);
        }
    } else if (bitCount >= fromBits || ((accumulator << (toBits - bitCount)) & maxValue) != 0) {
        return QVector<int>();
    }

    return output;
}

/**
 * @brief hrpExpand
 * @param hrp
 * @return
 */
QVector<int> hrpExpand(const QString &hrp)
{
    QVector<int> expanded;
    expanded.reserve(hrp.size() * 2 + 1);
    for (int i = 0; i < hrp.size(); ++i) {
        expanded.append(hrp.at(i).unicode() >> 5);
    }
    expanded.append(0);
    for (int i = 0; i < hrp.size(); ++i) {
        expanded.append(hrp.at(i).unicode() & 31);
    }
    return expanded;
}

/**
 * @brief bech32Polymod
 * @param values
 * @return
 */
quint32 bech32Polymod(const QVector<int> &values)
{
    static const quint32 generators[5] = {
        0x3b6a57b2U, 0x26508e6dU, 0x1ea119faU, 0x3d4233ddU, 0x2a1462b3U
    };

    quint32 checksum = 1;
    for (int i = 0; i < values.size(); ++i) {
        const quint32 top = checksum >> 25;
        checksum = ((checksum & 0x1ffffffU) << 5) ^ static_cast<quint32>(values.at(i));
        for (int j = 0; j < 5; ++j) {
            if (((top >> j) & 1U) != 0U) {
                checksum ^= generators[j];
            }
        }
    }
    return checksum;
}

/**
 * @brief bech32CreateChecksum
 * @param hrp
 * @param data
 * @return
 */
QVector<int> bech32CreateChecksum(const QString &hrp, const QVector<int> &data)
{
    QVector<int> values = hrpExpand(hrp);
    values += data;
    values += QVector<int>(6, 0);
    const quint32 polymod = bech32Polymod(values) ^ 1U;

    QVector<int> checksum;
    checksum.reserve(6);
    for (int i = 0; i < 6; ++i) {
        checksum.append((polymod >> (5 * (5 - i))) & 31U);
    }
    return checksum;
}

}

namespace WalletCryptoHelpers
{

/**
 * @brief hashBytes
 * @param input
 * @return
 */
QByteArray hashBytes(const QByteArray &input)
{
    return QCryptographicHash::hash(input, QCryptographicHash::Sha256);
}

/**
 * @brief toHex
 * @param data
 * @param size
 * @return
 */
QString toHex(const unsigned char *data, int size)
{
    return QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(data), size).toHex());
}

/**
 * @brief bech32Encode
 * @param hrp
 * @param payload
 * @return
 */
QString bech32Encode(const QString &hrp, const QByteArray &payload)
{
    const QVector<int> data = convertBits(payload, 8, 5, true);
    if (data.isEmpty() && !payload.isEmpty()) {
        return QString();
    }

    const QVector<int> checksum = bech32CreateChecksum(hrp, data);
    QString encoded = hrp + QLatin1Char('1');
    encoded.reserve(hrp.size() + 1 + data.size() + checksum.size());
    for (int i = 0; i < data.size(); ++i) {
        encoded.append(QLatin1Char(kBech32Charset[data.at(i)]));
    }
    for (int i = 0; i < checksum.size(); ++i) {
        encoded.append(QLatin1Char(kBech32Charset[checksum.at(i)]));
    }
    return encoded;
}

/**
 * @brief appendU8
 * @param out
 * @param value
 */
void appendU8(QByteArray &out, quint8 value)
{
    out.append(static_cast<char>(value));
}

/**
 * @brief appendU16
 * @param out
 * @param value
 */
void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>(value & 0xff));
}

/**
 * @brief appendU64
 * @param out
 * @param value
 */
void appendU64(QByteArray &out, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

/**
 * @brief fromHex
 * @param hex
 * @return
 */
QByteArray fromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toUtf8());
}

/**
 * @brief paymentProofMessage
 * @param slate
 * @return
 */
QByteArray paymentProofMessage(const SlateV4 &slate)
{
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(slate.id,
             slate.amount,
             slate.fee,
             slate.paymentProof.senderAddress,
             slate.paymentProof.receiverAddress)
        .toUtf8();
}

}
