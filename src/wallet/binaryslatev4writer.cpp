#include "binaryslatev4writer.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QStringList>
#include <QVector>

#include "slatev4.h"

namespace {

const char *kBase58Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

void appendU8(QByteArray &out, quint8 value)
{
    out.append(static_cast<char>(value));
}

void appendU16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>(value & 0xff));
}

void appendU32(QByteArray &out, quint32 value)
{
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

void appendU64(QByteArray &out, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

bool appendHex(QByteArray &out, const QString &hex, int expectedBytes)
{
    QByteArray bytes = QByteArray::fromHex(hex.toUtf8());
    if (bytes.size() != expectedBytes) {
        return false;
    }
    out.append(bytes);
    return true;
}

QStringList uuidParts(const QString &uuid)
{
    return uuid.trimmed().split(QLatin1Char('-'), Qt::SkipEmptyParts);
}

bool appendUuid(QByteArray &out, const QString &uuid)
{
    const QString compact = uuidParts(uuid).join(QString());
    return appendHex(out, compact, 16);
}

quint64 parseNanogrin(const QString &amount)
{
    const QString trimmed = amount.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    const QStringList parts = trimmed.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = parts.size() == 2 ? parts.at(1) : QString();
    if (fractional.size() > 9) {
        fractional = fractional.left(9);
    }
    while (fractional.size() < 9) {
        fractional.append(QLatin1Char('0'));
    }

    bool fracOk = false;
    const quint64 frac = fractional.isEmpty() ? 0 : fractional.toULongLong(&fracOk);
    if (!fractional.isEmpty() && !fracOk) {
        return 0;
    }

    return whole * 1000000000ULL + frac;
}

quint64 encodeFeeFields(const QString &fee)
{
    return parseNanogrin(fee);
}

quint8 stageByte(SlateV4::State state)
{
    switch (state) {
    case SlateV4::Standard1: return 1;
    case SlateV4::Standard2: return 2;
    case SlateV4::Standard3: return 3;
    case SlateV4::Invoice1: return 4;
    case SlateV4::Invoice2: return 5;
    case SlateV4::Invoice3: return 6;
    case SlateV4::Unknown:
    default:
        return 0;
    }
}

QString encodeBase58(const QByteArray &input)
{
    if (input.isEmpty()) {
        return QString();
    }

    QVector<int> digits(1, 0);
    for (int i = 0; i < input.size(); ++i) {
        int carry = static_cast<unsigned char>(input.at(i));
        for (int j = 0; j < digits.size(); ++j) {
            carry += digits[j] << 8;
            digits[j] = carry % 58;
            carry /= 58;
        }
        while (carry > 0) {
            digits.append(carry % 58);
            carry /= 58;
        }
    }

    QString result;
    for (int i = 0; i < input.size() && input.at(i) == '\0'; ++i) {
        result.append(QLatin1Char('1'));
    }
    for (int i = digits.size() - 1; i >= 0; --i) {
        result.append(QLatin1Char(kBase58Alphabet[digits.at(i)]));
    }
    return result;
}

QString formatArmored(const QString &data)
{
    QString out;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) {
            if (i % (15 * 200) == 0) {
                out.append(QLatin1Char('\n'));
            } else if (i % 15 == 0) {
                out.append(QLatin1Char(' '));
            }
        }
        out.append(data.at(i));
    }
    return out;
}

bool serializeSlate(const SlateV4 &slate, QByteArray *payloadOut)
{
    if (!payloadOut) {
        return false;
    }

    QByteArray payload;
    appendU16(payload, static_cast<quint16>(slate.ver.slateVersion));
    appendU16(payload, static_cast<quint16>(slate.ver.blockHeaderVersion));
    if (!appendUuid(payload, slate.id)) {
        return false;
    }
    appendU8(payload, stageByte(slate.state));
    if (!appendHex(payload, slate.offset, 32)) {
        return false;
    }

    quint8 optionalFields = 0;
    if (slate.numParticipants != 2) optionalFields |= 0x01;
    if (!slate.amount.trimmed().isEmpty() && parseNanogrin(slate.amount) > 0) optionalFields |= 0x02;
    if (!slate.fee.trimmed().isEmpty() && parseNanogrin(slate.fee) > 0) optionalFields |= 0x04;
    if (slate.kernelFeatures != 0) optionalFields |= 0x08;
    if (!slate.ttl.trimmed().isEmpty() && slate.ttl != QStringLiteral("0")) optionalFields |= 0x10;
    appendU8(payload, optionalFields);

    if (optionalFields & 0x01) appendU8(payload, static_cast<quint8>(slate.numParticipants));
    if (optionalFields & 0x02) appendU64(payload, parseNanogrin(slate.amount));
    if (optionalFields & 0x04) appendU64(payload, encodeFeeFields(slate.fee));
    if (optionalFields & 0x08) appendU8(payload, static_cast<quint8>(slate.kernelFeatures));
    if (optionalFields & 0x10) appendU64(payload, slate.ttl.toULongLong());

    appendU8(payload, static_cast<quint8>(slate.signatures.size()));
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate.signatures.at(i);
        appendU8(payload, sig.part.isEmpty() ? 0 : 1);
        if (!appendHex(payload, sig.xs, 33) || !appendHex(payload, sig.nonce, 33)) {
            return false;
        }
        if (!sig.part.isEmpty() && !appendHex(payload, sig.part, 64)) {
            return false;
        }
    }

    quint8 optionalStructs = 0;
    if (!slate.commitments.isEmpty()) optionalStructs |= 0x01;
    if (slate.hasPaymentProof) optionalStructs |= 0x02;
    appendU8(payload, optionalStructs);

    if (optionalStructs & 0x01) {
        appendU16(payload, static_cast<quint16>(slate.commitments.size()));
        for (int i = 0; i < slate.commitments.size(); ++i) {
            const SlateV4::Commit &commit = slate.commitments.at(i);
            appendU8(payload, commit.proof.isEmpty() ? 0 : 1);
            appendU8(payload, static_cast<quint8>(commit.feature));
            if (!appendHex(payload, commit.commitment, 33)) {
                return false;
            }
            if (!commit.proof.isEmpty()) {
                const QByteArray proofBytes = QByteArray::fromHex(commit.proof.toUtf8());
                if (proofBytes.size() > 675) {
                    return false;
                }
                appendU64(payload, static_cast<quint64>(proofBytes.size()));
                payload.append(proofBytes);
            }
        }
    }

    if (optionalStructs & 0x02) {
        if (!appendHex(payload, slate.paymentProof.senderAddress, 32)
            || !appendHex(payload, slate.paymentProof.receiverAddress, 32)) {
            return false;
        }
        appendU8(payload, slate.paymentProof.receiverSignature.isEmpty() ? 0 : 1);
        if (!slate.paymentProof.receiverSignature.isEmpty()
            && !appendHex(payload, slate.paymentProof.receiverSignature, 64)) {
            return false;
        }
    }

    if (slate.kernelFeatures == 2 && slate.metadata.contains(QStringLiteral("lock_height"))) {
        appendU64(payload, slate.metadata.value(QStringLiteral("lock_height")).toString().toULongLong());
    }

    *payloadOut = payload;
    return true;
}

QString armorPayload(const QByteArray &payload)
{
    const QByteArray checksum = QCryptographicHash::hash(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256).left(4);
    return QStringLiteral("BEGINSLATEPACK. %1. ENDSLATEPACK.\n")
        .arg(formatArmored(encodeBase58(checksum + payload)));
}

}

bool BinarySlateV4Writer::encodeSlatepack(const SlateV4 &slate, QString *armoredOut, QString *errorOut)
{
    QByteArray slatePayload;
    if (!serializeSlate(slate, &slatePayload)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to serialize binary SlateV4 payload.");
        }
        return false;
    }

    QByteArray slatepackPayload;
    appendU8(slatepackPayload, 1);
    appendU8(slatepackPayload, 0);
    appendU8(slatepackPayload, 0);
    appendU16(slatepackPayload, 0);
    appendU32(slatepackPayload, 0);
    appendU64(slatepackPayload, static_cast<quint64>(slatePayload.size()));
    slatepackPayload.append(slatePayload);

    if (armoredOut) {
        *armoredOut = armorPayload(slatepackPayload);
    }
    return true;
}
