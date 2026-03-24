#include "binaryslatev4reader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <limits>

#include "slatev4.h"

namespace {

QString formatNanogrin(quint64 value)
{
    const quint64 whole = value / 1000000000ULL;
    const quint64 fractional = value % 1000000000ULL;
    return QStringLiteral("%1.%2")
        .arg(QString::number(whole))
        .arg(QString::number(fractional), 9, QLatin1Char('0'));
}

class ByteCursor
{
public:
    explicit ByteCursor(const QByteArray &data) : m_data(data), m_offset(0) {}

    bool canRead(int count) const { return count >= 0 && m_offset + count <= m_data.size(); }

    quint8 readU8(bool *ok)
    {
        if (!canRead(1)) {
            if (ok) *ok = false;
            return 0;
        }
        if (ok) *ok = true;
        return static_cast<quint8>(m_data.at(m_offset++));
    }

    quint16 readU16(bool *ok)
    {
        if (!canRead(2)) {
            if (ok) *ok = false;
            return 0;
        }
        const quint16 value =
            (static_cast<quint8>(m_data.at(m_offset)) << 8)
            | static_cast<quint8>(m_data.at(m_offset + 1));
        m_offset += 2;
        if (ok) *ok = true;
        return value;
    }

    quint32 readU32(bool *ok)
    {
        if (!canRead(4)) {
            if (ok) *ok = false;
            return 0;
        }
        quint32 value = 0;
        for (int i = 0; i < 4; ++i) {
            value = (value << 8) | static_cast<quint8>(m_data.at(m_offset++));
        }
        if (ok) *ok = true;
        return value;
    }

    quint64 readU64(bool *ok)
    {
        if (!canRead(8)) {
            if (ok) *ok = false;
            return 0;
        }
        quint64 value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | static_cast<quint8>(m_data.at(m_offset++));
        }
        if (ok) *ok = true;
        return value;
    }

    QByteArray readBytes(int count, bool *ok)
    {
        if (!canRead(count)) {
            if (ok) *ok = false;
            return QByteArray();
        }
        const QByteArray out = m_data.mid(m_offset, count);
        m_offset += count;
        if (ok) *ok = true;
        return out;
    }

private:
    QByteArray m_data;
    int m_offset;
};

QString formatUuid(const QByteArray &bytes)
{
    if (bytes.size() != 16) {
        return QString();
    }
    const QString hex = QString::fromUtf8(bytes.toHex());
    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(hex.mid(0, 8), hex.mid(8, 4), hex.mid(12, 4), hex.mid(16, 4), hex.mid(20, 12));
}

bool decodeSlateV4BinaryPayload(const QByteArray &payload, QString *decodedOut, QString *errorOut)
{
    ByteCursor cursor(payload);
    bool ok = false;

    SlateV4 slate;
    slate.ver.slateVersion = static_cast<int>(cursor.readU16(&ok));
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slate version.");
        return false;
    }
    slate.ver.blockHeaderVersion = static_cast<int>(cursor.readU16(&ok));
    const QByteArray idBytes = cursor.readBytes(16, &ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slate id.");
        return false;
    }
    slate.id = formatUuid(idBytes);

    const quint8 stageValue = cursor.readU8(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slate state.");
        return false;
    }
    switch (stageValue) {
    case 1: slate.state = SlateV4::Standard1; break;
    case 2: slate.state = SlateV4::Standard2; break;
    case 3: slate.state = SlateV4::Standard3; break;
    case 4: slate.state = SlateV4::Invoice1; break;
    case 5: slate.state = SlateV4::Invoice2; break;
    case 6: slate.state = SlateV4::Invoice3; break;
    default: slate.state = SlateV4::Unknown; break;
    }

    const QByteArray offset = cursor.readBytes(32, &ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slate offset.");
        return false;
    }
    slate.offset = QString::fromUtf8(offset.toHex());

    const quint8 optionalFields = cursor.readU8(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slate field status.");
        return false;
    }
    if (optionalFields & 0x01) slate.numParticipants = static_cast<int>(cursor.readU8(&ok));
    if (optionalFields & 0x02) slate.amount = formatNanogrin(cursor.readU64(&ok));
    if (optionalFields & 0x04) {
        const quint64 rawFee = cursor.readU64(&ok);
        const quint8 feeShift = static_cast<quint8>((rawFee >> 40) & 0x0f);
        const quint64 fee = rawFee & ((1ULL << 40) - 1ULL);
        slate.fee = feeShift == 0 ? formatNanogrin(fee) : QString::number(fee);
    }
    if (optionalFields & 0x08) slate.kernelFeatures = static_cast<int>(cursor.readU8(&ok));
    if (optionalFields & 0x10) slate.ttl = QString::number(cursor.readU64(&ok));
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary optional fields.");
        return false;
    }

    const quint8 numSigs = cursor.readU8(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary signature count.");
        return false;
    }
    for (quint8 i = 0; i < numSigs; ++i) {
        SlateV4::ParticipantData sig;
        const quint8 hasPart = cursor.readU8(&ok);
        sig.xs = QString::fromUtf8(cursor.readBytes(33, &ok).toHex());
        sig.nonce = QString::fromUtf8(cursor.readBytes(33, &ok).toHex());
        if (hasPart > 0) {
            sig.part = QString::fromUtf8(cursor.readBytes(64, &ok).toHex());
        }
        if (!ok) {
            if (errorOut) *errorOut = QStringLiteral("Invalid binary signature entry.");
            return false;
        }
        slate.signatures.append(sig);
    }

    const quint8 optionalStructs = cursor.readU8(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary struct status.");
        return false;
    }
    if (optionalStructs & 0x01) {
        const quint16 numComs = cursor.readU16(&ok);
        for (quint16 i = 0; i < numComs; ++i) {
            SlateV4::Commit commit;
            const quint8 hasProof = cursor.readU8(&ok);
            commit.feature = static_cast<int>(cursor.readU8(&ok));
            commit.commitment = QString::fromUtf8(cursor.readBytes(33, &ok).toHex());
            if (hasProof > 0) {
                const quint64 proofSize = cursor.readU64(&ok);
                if (!ok || proofSize > 675 || proofSize > static_cast<quint64>(std::numeric_limits<int>::max())) {
                    if (errorOut) *errorOut = QStringLiteral("Invalid binary rangeproof length.");
                    return false;
                }
                commit.proof = QString::fromUtf8(cursor.readBytes(static_cast<int>(proofSize), &ok).toHex());
            }
            if (!ok) {
                if (errorOut) *errorOut = QStringLiteral("Invalid binary commitment entry.");
                return false;
            }
            slate.commitments.append(commit);
        }
    }
    if (optionalStructs & 0x02) {
        slate.hasPaymentProof = true;
        slate.paymentProof.senderAddress = QString::fromUtf8(cursor.readBytes(32, &ok).toHex());
        slate.paymentProof.receiverAddress = QString::fromUtf8(cursor.readBytes(32, &ok).toHex());
        const quint8 hasReceiverSig = cursor.readU8(&ok);
        if (hasReceiverSig == 1) {
            slate.paymentProof.receiverSignature = QString::fromUtf8(cursor.readBytes(64, &ok).toHex());
        }
        if (!ok) {
            if (errorOut) *errorOut = QStringLiteral("Invalid binary payment proof.");
            return false;
        }
    }

    if (slate.kernelFeatures != 0) {
        slate.metadata.insert(QStringLiteral("feature_args_present"), true);
        if (slate.kernelFeatures == 2 && cursor.canRead(8)) {
            slate.metadata.insert(QStringLiteral("lock_height"), QString::number(cursor.readU64(&ok)));
        }
    }

    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
    slate.metadata.insert(QStringLiteral("workflow_id"), slate.id);
    slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    slate.metadata.insert(QStringLiteral("external_binary"), true);

    if (decodedOut) {
        *decodedOut = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    }
    return true;
}

}

bool BinarySlateV4Reader::decodeSlatepackPayload(const QByteArray &payload, QString *decodedOut, QString *errorOut)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (doc.isObject()) {
        const QString inner = QString::fromUtf8(QByteArray::fromBase64(doc.object().value(QStringLiteral("payload")).toString().toUtf8()));
        if (!inner.isEmpty()) {
            if (decodedOut) {
                *decodedOut = inner;
            }
            return true;
        }
    }

    ByteCursor cursor(payload);
    bool ok = false;
    const quint8 major = cursor.readU8(&ok);
    const quint8 minor = cursor.readU8(&ok);
    const quint8 mode = cursor.readU8(&ok);
    const quint16 optFlags = cursor.readU16(&ok);
    const quint32 optFieldsLen = cursor.readU32(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slatepack header.");
        return false;
    }

    const QByteArray optFields = cursor.readBytes(static_cast<int>(optFieldsLen), &ok);
    Q_UNUSED(optFields);
    Q_UNUSED(optFlags);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slatepack metadata.");
        return false;
    }

    const quint64 payloadSize = cursor.readU64(&ok);
    if (!ok || payloadSize > static_cast<quint64>(std::numeric_limits<int>::max())) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slatepack payload length.");
        return false;
    }

    const QByteArray innerPayload = cursor.readBytes(static_cast<int>(payloadSize), &ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Binary slatepack payload is truncated.");
        return false;
    }

    if (mode != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Encrypted Slatepack v%1.%2 was recognized, but decryption is not implemented yet.")
                .arg(QString::number(major), QString::number(minor));
        }
        return false;
    }

    return decodeSlateV4BinaryPayload(innerPayload, decodedOut, errorOut);
}
