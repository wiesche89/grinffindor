#include "grinwalletworkflowhelpers.h"

#include "wallet/binaryslatev4reader.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QVector>

namespace
{
const char kBase58Alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool compactCommitLessThan(const SlateV4::Commit &left, const SlateV4::Commit &right)
{
    const QByteArray leftBytes = QByteArray::fromHex(left.commitment.toUtf8());
    const QByteArray rightBytes = QByteArray::fromHex(right.commitment.toUtf8());
    if (leftBytes.size() == rightBytes.size() && !leftBytes.isEmpty()) {
        return leftBytes < rightBytes;
    }
    return left.commitment < right.commitment;
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

QByteArray decodeBase58(const QString &text)
{
    QByteArray output;
    if (text.isEmpty()) {
        return output;
    }

    QVector<int> bytes(1, 0);
    for (int i = 0; i < text.size(); ++i) {
        const int value = QByteArray(kBase58Alphabet).indexOf(text.at(i).toLatin1());
        if (value < 0) {
            return QByteArray();
        }

        int carry = value;
        for (int j = 0; j < bytes.size(); ++j) {
            carry += bytes[j] * 58;
            bytes[j] = carry & 0xff;
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.append(carry & 0xff);
            carry >>= 8;
        }
    }

    for (int i = 0; i < text.size() && text.at(i) == QLatin1Char('1'); ++i) {
        output.append('\0');
    }
    for (int i = bytes.size() - 1; i >= 0; --i) {
        output.append(static_cast<char>(bytes.at(i)));
    }
    return output;
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
}

quint64 GrinWalletWorkflowHelpers::amountToNanogrin(const QString &amount)
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

QString GrinWalletWorkflowHelpers::formatNanogrin(quint64 amount)
{
    return QStringLiteral("%1.%2")
        .arg(QString::number(amount / 1000000000ULL))
        .arg(QString::number(amount % 1000000000ULL), 9, QLatin1Char('0'));
}

WalletOutput GrinWalletWorkflowHelpers::findTrackedOutputByCommitment(const QList<WalletOutput> &outputs,
                                                                      const QString &commitment)
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commitment) {
            return outputs.at(i);
        }
    }
    return WalletOutput();
}

WalletOutput GrinWalletWorkflowHelpers::normalizedTrackedOutput(const WalletOutput &output,
                                                               const WalletKeychain &keychain)
{
    if (!keychain.isValid()
        || output.keyPath.isEmpty()
        || !output.keyPath.startsWith(QStringLiteral("m/0/0/"))) {
        return output;
    }

    const WalletKeychain::OutputSecrets secrets =
        keychain.deriveOutputSecrets(output.childIndex, amountToNanogrin(output.amount));
    if (!secrets.success) {
        return output;
    }

    WalletOutput normalized = output;
    normalized.blindingFactor = QString::fromUtf8(secrets.blindingFactor.toHex());
    return normalized;
}

QString GrinWalletWorkflowHelpers::invoiceContextKey(const QString &suffix)
{
    return QStringLiteral("invoice_context_%1").arg(suffix);
}

QString GrinWalletWorkflowHelpers::standardContextKey(const QString &suffix)
{
    return QStringLiteral("standard_context_%1").arg(suffix);
}

WalletCryptoBackend::ParticipantContext GrinWalletWorkflowHelpers::participantContextFromJson(
    const QJsonObject &json,
    const QString &role)
{
    WalletCryptoBackend::ParticipantContext context;
    context.role = role;
    context.blindSecret = json.value(QStringLiteral("sec_key")).toString();
    context.nonceSecret = json.value(QStringLiteral("sec_nonce")).toString();
    context.blindPublic = json.value(QStringLiteral("pub_key")).toString();
    context.noncePublic = json.value(QStringLiteral("pub_nonce")).toString();
    return context;
}

QJsonObject GrinWalletWorkflowHelpers::participantContextToJson(
    const WalletCryptoBackend::ParticipantContext &context)
{
    QJsonObject json;
    json.insert(QStringLiteral("sec_key"), context.blindSecret);
    json.insert(QStringLiteral("sec_nonce"), context.nonceSecret);
    json.insert(QStringLiteral("pub_key"), context.blindPublic);
    json.insert(QStringLiteral("pub_nonce"), context.noncePublic);
    return json;
}

QList<SlateV4::Commit> GrinWalletWorkflowHelpers::sortedCompactCommitments(
    const QList<SlateV4::Commit> &commits)
{
    QList<SlateV4::Commit> inputs;
    QList<SlateV4::Commit> outputs;
    for (const SlateV4::Commit &commit : commits) {
        if (commit.proof.trimmed().isEmpty()) {
            inputs.append(commit);
        } else {
            outputs.append(commit);
        }
    }

    std::sort(inputs.begin(), inputs.end(), compactCommitLessThan);
    std::sort(outputs.begin(), outputs.end(), compactCommitLessThan);

    QList<SlateV4::Commit> ordered = inputs;
    ordered.append(outputs);
    return ordered;
}

QString GrinWalletWorkflowHelpers::encodeSlatepackArmor(const QString &payloadJson, const QString &sender)
{
    QJsonObject envelope;
    QJsonObject version;
    version.insert(QStringLiteral("major"), 1);
    version.insert(QStringLiteral("minor"), 0);
    envelope.insert(QStringLiteral("slatepack"), version);
    envelope.insert(QStringLiteral("mode"), 0);
    if (!sender.trimmed().isEmpty()) {
        envelope.insert(QStringLiteral("sender"), sender.trimmed());
    }
    envelope.insert(QStringLiteral("payload"), QString::fromUtf8(payloadJson.toUtf8().toBase64()));

    const QByteArray serialized = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    const QByteArray checksum = QCryptographicHash::hash(
        QCryptographicHash::hash(serialized, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256).left(4);
    return QStringLiteral("BEGINSLATEPACK. %1. ENDSLATEPACK.\n")
        .arg(formatArmored(encodeBase58(checksum + serialized)));
}

namespace
{
QString userFacingSlatepackParseNote(const QString &parseError)
{
    const QString trimmed = parseError.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Slatepack payload could not be parsed.");
    }

    if (trimmed.contains(QStringLiteral("Wallet secret is unavailable"), Qt::CaseInsensitive)) {
        return QStringLiteral("This Slatepack is encrypted. Unlock the wallet that owns the recipient address before decoding it.");
    }
    if (trimmed.contains(QStringLiteral("not addressed to this wallet"), Qt::CaseInsensitive)) {
        return QStringLiteral("This Slatepack is encrypted for a different recipient wallet.");
    }
    if (trimmed.contains(QStringLiteral("authentication failed"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("invalid base64"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("truncated"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("malformed"), Qt::CaseInsensitive)) {
        return QStringLiteral("The Slatepack looks damaged or incomplete. Copy it again and make sure no characters are missing.");
    }

    return trimmed;
}

QString buildSlatepackDiagnostic(const QString &kind,
                                 const QByteArray &payload,
                                 const QString &note)
{
    QJsonObject diagnostic;
    diagnostic.insert(QStringLiteral("external_slatepack"), true);
    diagnostic.insert(QStringLiteral("diagnostic_kind"), kind);
    diagnostic.insert(QStringLiteral("payload_size"), payload.size());
    diagnostic.insert(QStringLiteral("payload_hex_preview"), QString::fromUtf8(payload.left(96).toHex()));
    diagnostic.insert(QStringLiteral("note"), userFacingSlatepackParseNote(note));
    return QString::fromUtf8(QJsonDocument(diagnostic).toJson(QJsonDocument::Indented));
}

QString decodeSlatepackArmor(const QString &slatepack)
{
    QString cleaned = slatepack;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() < 3
        || parts.at(0) != QStringLiteral("BEGINSLATEPACK")
        || parts.at(2) != QStringLiteral("ENDSLATEPACK")) {
        return QString();
    }

    const QByteArray decoded = decodeBase58(parts.at(1));
    if (decoded.size() < 5) {
        return QString();
    }

    const QByteArray checksum = decoded.left(4);
    const QByteArray payload = decoded.mid(4);
    const QByteArray expected = QCryptographicHash::hash(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256).left(4);
    if (checksum != expected) {
        return QString();
    }

    QString decodedPayload;
    QString parseError;
    if (!BinarySlateV4Reader::decodeSlatepackPayload(payload, QByteArray(), &decodedPayload, &parseError)) {
        return buildSlatepackDiagnostic(QStringLiteral("armored"), payload, parseError);
    }
    return decodedPayload;
}
}

QString GrinWalletWorkflowHelpers::decodeIncomingSlatepack(const QString &input,
                                                           const QByteArray &decryptionKey)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QJsonDocument jsonDocument = QJsonDocument::fromJson(trimmed.toUtf8());
    if (jsonDocument.isObject()) {
        QString decodedPayload;
        QString parseError;
        if (BinarySlateV4Reader::decodeSlatepackPayload(
                trimmed.toUtf8(), decryptionKey, &decodedPayload, &parseError)) {
            return decodedPayload;
        }

        const QJsonObject object = jsonDocument.object();
        if (object.contains(QStringLiteral("slatepack")) || object.contains(QStringLiteral("payload"))) {
            const QByteArray payload =
                QByteArray::fromBase64(object.value(QStringLiteral("payload")).toString().toUtf8());
            return buildSlatepackDiagnostic(QStringLiteral("json"), payload, parseError);
        }
    }

    const QString armored = decodeSlatepackArmor(trimmed);
    if (!armored.isEmpty()) {
        return armored;
    }

    QString decodedPayload;
    QString parseError;
    if (BinarySlateV4Reader::decodeSlatepackPayload(
            trimmed.toUtf8(), decryptionKey, &decodedPayload, &parseError)) {
        return decodedPayload;
    }

    return QString();
}
