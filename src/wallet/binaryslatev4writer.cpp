#include "binaryslatev4writer.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStringList>
#include <QVector>
#include <QDebug>

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
extern "C" {
#include "monocypher.h"
}
#endif

#include "slatev4.h"

namespace {

const char *kBase58Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const int kAgeFileKeySize = 16;
const int kAgePayloadNonceSize = 16;
const int kAgeChunkSize = 64 * 1024;
const int kChaChaOverhead = 16;
const quint64 kFeeFieldsMask = ((1ULL << 40) - 1ULL);
const quint8 kFeeFieldsShift = 0;
const char kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

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
    const quint64 parsedFee = parseNanogrin(fee);
    return (static_cast<quint64>(kFeeFieldsShift) << 40) | (parsedFee & kFeeFieldsMask);
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

QByteArray randomBytes(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return data;
}

QVector<int> bech32CharsetReverse()
{
    QVector<int> reverse(128, -1);
    for (int i = 0; kBech32Charset[i] != '\0'; ++i) {
        reverse[static_cast<int>(kBech32Charset[i])] = i;
    }
    return reverse;
}

QByteArray convertBitsToBytes(const QVector<int> &data, int fromBits, int toBits, bool pad)
{
    QByteArray output;
    int accumulator = 0;
    int bitCount = 0;
    const int maxValue = (1 << toBits) - 1;
    const int maxAccumulator = (1 << (fromBits + toBits - 1)) - 1;

    for (int i = 0; i < data.size(); ++i) {
        const int value = data.at(i);
        if (value < 0 || (value >> fromBits) != 0) {
            return QByteArray();
        }
        accumulator = ((accumulator << fromBits) | value) & maxAccumulator;
        bitCount += fromBits;
        while (bitCount >= toBits) {
            bitCount -= toBits;
            output.append(static_cast<char>((accumulator >> bitCount) & maxValue));
        }
    }

    if (pad) {
        if (bitCount > 0) {
            output.append(static_cast<char>((accumulator << (toBits - bitCount)) & maxValue));
        }
    } else if (bitCount >= fromBits || ((accumulator << (toBits - bitCount)) & maxValue) != 0) {
        return QByteArray();
    }

    return output;
}

QByteArray decodeBech32Payload(const QString &address, QString *hrpOut = 0)
{
    static const QVector<int> reverse = bech32CharsetReverse();

    const QString trimmed = address.trimmed().toLower();
    const int separator = trimmed.lastIndexOf(QLatin1Char('1'));
    if (separator <= 0 || separator + 7 > trimmed.size()) {
        return QByteArray();
    }

    if (hrpOut) {
        *hrpOut = trimmed.left(separator);
    }

    QVector<int> values;
    values.reserve(trimmed.size() - separator - 1);
    for (int i = separator + 1; i < trimmed.size(); ++i) {
        const ushort ch = trimmed.at(i).unicode();
        if (ch >= static_cast<ushort>(reverse.size()) || reverse.at(ch) < 0) {
            return QByteArray();
        }
        values.append(reverse.at(ch));
    }

    if (values.size() < 6) {
        return QByteArray();
    }

    values.resize(values.size() - 6);
    return convertBitsToBytes(values, 5, 8, false);
}

QByteArray hkdfSha256(const QByteArray &ikm,
                      const QByteArray &salt,
                      const QByteArray &info,
                      int outputLength)
{
    const QByteArray actualSalt = salt.isEmpty() ? QByteArray(32, '\0') : salt;
    const QByteArray prk = QMessageAuthenticationCode::hash(
        ikm, actualSalt, QCryptographicHash::Sha256);

    QByteArray output;
    output.reserve(outputLength);
    QByteArray previous;
    quint8 counter = 1;
    while (output.size() < outputLength) {
        QByteArray blockInput = previous + info + QByteArray(1, static_cast<char>(counter++));
        previous = QMessageAuthenticationCode::hash(blockInput, prk, QCryptographicHash::Sha256);
        output.append(previous);
    }
    output.truncate(outputLength);
    return output;
}

QString encodeBase64Raw(const QByteArray &input)
{
    QByteArray encoded = input.toBase64(QByteArray::Base64Encoding);
    while (encoded.endsWith('=')) {
        encoded.chop(1);
    }
    return QString::fromUtf8(encoded);
}

QString formatWrappedBase64(const QByteArray &input)
{
    const QString encoded = encodeBase64Raw(input);
    QString wrapped;
    for (int i = 0; i < encoded.size(); i += 64) {
        wrapped.append(encoded.mid(i, 64));
        wrapped.append(QLatin1Char('\n'));
    }
    return wrapped;
}

bool incrementStreamNonce(QByteArray *nonce)
{
    if (!nonce || nonce->size() != 12) {
        return false;
    }
    for (int i = nonce->size() - 2; i >= 0; --i) {
        unsigned char value = static_cast<unsigned char>(nonce->at(i));
        value = static_cast<unsigned char>(value + 1);
        (*nonce)[i] = static_cast<char>(value);
        if (value != 0) {
            return true;
        }
    }
    return false;
}

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
bool deriveX25519PublicKey(const QByteArray &privateKey, QByteArray *publicKeyOut)
{
    if (!publicKeyOut || privateKey.size() != 32) {
        return false;
    }

    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_x25519_public_key(reinterpret_cast<uint8_t *>(publicKey.data()),
                             reinterpret_cast<const uint8_t *>(privateKey.constData()));
    *publicKeyOut = publicKey;
    return true;
}

bool deriveX25519SharedSecret(const QByteArray &privateKey,
                              const QByteArray &peerPublicKey,
                              QByteArray *sharedSecretOut)
{
    if (!sharedSecretOut || privateKey.size() != 32 || peerPublicKey.size() != 32) {
        return false;
    }

    QByteArray sharedSecret(32, Qt::Uninitialized);
    crypto_x25519(reinterpret_cast<uint8_t *>(sharedSecret.data()),
                  reinterpret_cast<const uint8_t *>(privateKey.constData()),
                  reinterpret_cast<const uint8_t *>(peerPublicKey.constData()));
    *sharedSecretOut = sharedSecret;
    return true;
}

bool chacha20Poly1305Encrypt(const QByteArray &key,
                             const QByteArray &nonce,
                             const QByteArray &plaintext,
                             QByteArray *ciphertextOut)
{
    if (!ciphertextOut || key.size() != 32 || nonce.size() != 12) {
        return false;
    }

    QByteArray ciphertext(plaintext.size(), Qt::Uninitialized);
    unsigned char tag[16];
    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx,
                          reinterpret_cast<const uint8_t *>(key.constData()),
                          reinterpret_cast<const uint8_t *>(nonce.constData()));
    crypto_aead_write(&ctx,
                      reinterpret_cast<uint8_t *>(ciphertext.data()),
                      tag,
                      0,
                      0,
                      reinterpret_cast<const uint8_t *>(plaintext.constData()),
                      static_cast<size_t>(plaintext.size()));
    ciphertext.append(reinterpret_cast<const char *>(tag), sizeof(tag));
    *ciphertextOut = ciphertext;
    return true;
}

QByteArray x25519SecretFromWalletSecret(const QByteArray &walletSecret)
{
    if (walletSecret.size() != 32) {
        return QByteArray();
    }
    // Use BLAKE2b-512 to match Monocypher's crypto_eddsa_key_pair which uses
    // crypto_blake2b internally (not SHA-512). The X25519 scalar must be derived
    // the same way so that our x25519 public key matches what recipients compute
    // from our Ed25519 slatepack address via the birational map.
    QByteArray expanded(64, Qt::Uninitialized);
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    crypto_blake2b(
        reinterpret_cast<uint8_t *>(expanded.data()),
        64,
        reinterpret_cast<const uint8_t *>(walletSecret.constData()),
        static_cast<size_t>(walletSecret.size()));
#else
    return QByteArray();
#endif
    QByteArray scalar = expanded.left(32);
    scalar[0] = static_cast<char>(static_cast<unsigned char>(scalar[0]) & 248U);
    scalar[31] = static_cast<char>((static_cast<unsigned char>(scalar[31]) & 127U) | 64U);
    return scalar;
}

bool recipientAddressToX25519(const QString &recipientAddress, QByteArray *x25519PublicOut)
{
    if (!x25519PublicOut) {
        return false;
    }

    const QByteArray legacyHex = QByteArray::fromHex(recipientAddress.trimmed().toUtf8());
    if (legacyHex.size() == 32) {
        *x25519PublicOut = legacyHex;
        return true;
    }

    const QByteArray ed25519Public = decodeBech32Payload(recipientAddress);
    if (ed25519Public.size() != 32) {
        return false;
    }

    QByteArray x25519Public(32, Qt::Uninitialized);
    crypto_eddsa_to_x25519(reinterpret_cast<uint8_t *>(x25519Public.data()),
                           reinterpret_cast<const uint8_t *>(ed25519Public.constData()));
    *x25519PublicOut = x25519Public;
    return true;
}

QByteArray buildEncryptedMetadata(const QString &sender, const QStringList &recipients)
{
    QByteArray metadata;
    quint16 optionalFlags = 0;
    if (!sender.trimmed().isEmpty()) {
        optionalFlags |= 0x01;
    }
    if (!recipients.isEmpty()) {
        optionalFlags |= 0x02;
    }

    appendU16(metadata, optionalFlags);
    if ((optionalFlags & 0x01) != 0) {
        const QByteArray senderBytes = sender.trimmed().toUtf8();
        appendU8(metadata, static_cast<quint8>(senderBytes.size()));
        metadata.append(senderBytes);
    }
    if ((optionalFlags & 0x02) != 0) {
        appendU16(metadata, static_cast<quint16>(recipients.size()));
        for (int i = 0; i < recipients.size(); ++i) {
            const QByteArray recipientBytes = recipients.at(i).trimmed().toUtf8();
            appendU8(metadata, static_cast<quint8>(recipientBytes.size()));
            metadata.append(recipientBytes);
        }
    }

    QByteArray packed;
    appendU32(packed, static_cast<quint32>(metadata.size()));
    packed.append(metadata);
    return packed;
}

bool encryptAgePayload(const QByteArray &payload,
                       const QString &senderAddress,
                       const QStringList &recipients,
                       const QByteArray &senderSecret,
                       QByteArray *encryptedOut,
                       QString *errorOut)
{
    if (!encryptedOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Encrypted Slatepack output target is missing.");
        }
        return false;
    }
    if (recipients.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("At least one recipient address is required for Slatepack encryption.");
        }
        return false;
    }
    if (senderSecret.size() != 32) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet secret is unavailable for Slatepack encryption.");
        }
        return false;
    }

    const QByteArray fileKey = randomBytes(kAgeFileKeySize);
    const QByteArray senderX25519Secret = x25519SecretFromWalletSecret(senderSecret);
    QByteArray senderX25519Public;
    if (!deriveX25519PublicKey(senderX25519Secret, &senderX25519Public)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to derive sender Slatepack encryption key.");
        }
        return false;
    }

    const QByteArray ephemeralSecret = randomBytes(32);
    QByteArray ephemeralPublic;
    if (!deriveX25519PublicKey(ephemeralSecret, &ephemeralPublic)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to derive ephemeral Slatepack encryption key.");
        }
        return false;
    }

    QByteArray header = QByteArrayLiteral("age-encryption.org/v1\n");
    for (int i = 0; i < recipients.size(); ++i) {
        QByteArray recipientX25519;
        if (!recipientAddressToX25519(recipients.at(i), &recipientX25519)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Recipient Slatepack address is invalid: %1").arg(recipients.at(i));
            }
            return false;
        }

        QByteArray sharedSecret;
        if (!deriveX25519SharedSecret(ephemeralSecret, recipientX25519, &sharedSecret)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to derive recipient shared secret.");
            }
            return false;
        }

        QByteArray salt = ephemeralPublic;
        salt.append(recipientX25519);
        const QByteArray wrappingKey = hkdfSha256(
            sharedSecret,
            salt,
            QByteArrayLiteral("age-encryption.org/v1/X25519"),
            32);

        QByteArray wrappedFileKey;
        if (!chacha20Poly1305Encrypt(wrappingKey, QByteArray(12, '\0'), fileKey, &wrappedFileKey)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to wrap Slatepack file key.");
            }
            return false;
        }

        header.append("-> X25519 ");
        header.append(encodeBase64Raw(ephemeralPublic).toUtf8());
        header.append('\n');
        header.append(formatWrappedBase64(wrappedFileKey).toUtf8());
    }

    const QByteArray headerMac = QMessageAuthenticationCode::hash(
        header + QByteArrayLiteral("---"),
        hkdfSha256(fileKey, QByteArray(), QByteArrayLiteral("header"), 32),
        QCryptographicHash::Sha256);

    QByteArray agePayload = header;
    agePayload.append("--- ");
    agePayload.append(encodeBase64Raw(headerMac).toUtf8());
    agePayload.append('\n');

    const QByteArray streamNonceSeed = randomBytes(kAgePayloadNonceSize);
    agePayload.append(streamNonceSeed);
    const QByteArray streamKey = hkdfSha256(fileKey, streamNonceSeed, QByteArrayLiteral("payload"), 32);

    QByteArray streamNonce(12, '\0');
    const QByteArray packagedPayload = buildEncryptedMetadata(senderAddress, recipients) + payload;
    for (int offset = 0; offset < packagedPayload.size(); offset += kAgeChunkSize) {
        const QByteArray chunk = packagedPayload.mid(offset, kAgeChunkSize);
        QByteArray chunkNonce = streamNonce;
        const bool lastChunk = (offset + kAgeChunkSize) >= packagedPayload.size();
        if (lastChunk) {
            chunkNonce[11] = 0x01;
        }

        QByteArray encryptedChunk;
        if (!chacha20Poly1305Encrypt(streamKey, chunkNonce, chunk, &encryptedChunk)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to encrypt Slatepack payload chunk.");
            }
            return false;
        }

        agePayload.append(encryptedChunk);
        if (!lastChunk && !incrementStreamNonce(&streamNonce)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Slatepack encryption nonce overflowed.");
            }
            return false;
        }
    }

    *encryptedOut = agePayload;
    return true;
}
#endif

QByteArray serializeJsonEnvelope(const QByteArray &payload, int mode, const QString &sender)
{
    QJsonObject envelope;
    envelope.insert(QStringLiteral("slatepack"), QStringLiteral("SP"));
    envelope.insert(QStringLiteral("mode"), mode);
    if (!sender.trimmed().isEmpty()) {
        envelope.insert(QStringLiteral("sender"), sender.trimmed());
    }
    envelope.insert(QStringLiteral("payload"), QString::fromUtf8(payload.toBase64(QByteArray::Base64Encoding)));
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

QByteArray buildBinaryEnvelopeOptionalFields(const QString &sender, const QStringList &recipients, quint16 *flagsOut)
{
    quint16 flags = 0;
    QByteArray fields;

    if (!sender.trimmed().isEmpty()) {
        const QByteArray senderBytes = sender.trimmed().toUtf8();
        if (!senderBytes.isEmpty() && senderBytes.size() <= 255) {
            flags |= 0x01;
            appendU8(fields, static_cast<quint8>(senderBytes.size()));
            fields.append(senderBytes);
        }
    }

    QStringList normalizedRecipients;
    for (int i = 0; i < recipients.size(); ++i) {
        const QString recipient = recipients.at(i).trimmed();
        if (!recipient.isEmpty()) {
            normalizedRecipients.append(recipient);
        }
    }

    if (!normalizedRecipients.isEmpty()) {
        flags |= 0x02;
        appendU16(fields, static_cast<quint16>(normalizedRecipients.size()));
        for (int i = 0; i < normalizedRecipients.size(); ++i) {
            const QByteArray recipientBytes = normalizedRecipients.at(i).toUtf8();
            if (recipientBytes.isEmpty() || recipientBytes.size() > 255) {
                continue;
            }
            appendU8(fields, static_cast<quint8>(recipientBytes.size()));
            fields.append(recipientBytes);
        }
    }

    if (flagsOut) {
        *flagsOut = flags;
    }
    return fields;
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

    const bool compactExternalInvoiceI2 =
        slate.metadata.value(QStringLiteral("external_binary")).toBool()
        && slate.state == SlateV4::Invoice2
        && slate.signatures.size() == 1
        && slate.amount.trimmed().isEmpty();

    quint8 optionalFields = 0;
    if (slate.numParticipants != 2 && !compactExternalInvoiceI2) optionalFields |= 0x01;
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

bool BinarySlateV4Writer::encodeSlatepack(const SlateV4 &slate,
                                          QString *armoredOut,
                                          QString *errorOut,
                                          const QString &sender,
                                          const QStringList &recipients,
                                          const QByteArray &senderSecret)
{
    QByteArray slatePayload;
    if (!serializeSlate(slate, &slatePayload)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to serialize binary SlateV4 payload.");
        }
        return false;
    }

    const bool preserveExternalBinary = slate.metadata.value(QStringLiteral("external_binary")).toBool();
    quint16 optFlags = 0;
    const QByteArray optFields = buildBinaryEnvelopeOptionalFields(
        preserveExternalBinary ? sender : QString(),
        QStringList(),
        &optFlags);

    QByteArray slatepackPayload;
    appendU8(slatepackPayload, 1);
    appendU8(slatepackPayload, 0);
    appendU8(slatepackPayload, 0);
    appendU16(slatepackPayload, optFlags);
    appendU32(slatepackPayload, static_cast<quint32>(optFields.size()));
    slatepackPayload.append(optFields);
    appendU64(slatepackPayload, static_cast<quint64>(slatePayload.size()));
    slatepackPayload.append(slatePayload);

    QByteArray outputPayload = slatepackPayload;
    QString outputFormat = QStringLiteral("binary-raw");
    const bool encryptionEnabled = false;
    if (encryptionEnabled && !recipients.isEmpty() && !preserveExternalBinary) {
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
        QByteArray encryptedPayload;
        if (!encryptAgePayload(slatePayload, sender, recipients, senderSecret, &encryptedPayload, errorOut)) {
            return false;
        }
        outputPayload = serializeJsonEnvelope(encryptedPayload, 1, sender);
        outputFormat = QStringLiteral("json-encrypted");
#else
        if (errorOut) {
            *errorOut = QStringLiteral("This build does not support recipient-encrypted Slatepacks.");
        }
        return false;
#endif
    }

    if (!sender.trimmed().isEmpty() && !preserveExternalBinary) {
        // Keep sender in optional binary header fields for plaintext mode compatibility.
        quint16 plainOptFlags = 0;
        const QByteArray plainOptFields = buildBinaryEnvelopeOptionalFields(
            sender,
            QStringList(),
            &plainOptFlags);

        outputPayload.clear();
        appendU8(outputPayload, 1);
        appendU8(outputPayload, 0);
        appendU8(outputPayload, 0);
        appendU16(outputPayload, plainOptFlags);
        appendU32(outputPayload, static_cast<quint32>(plainOptFields.size()));
        outputPayload.append(plainOptFields);
        appendU64(outputPayload, static_cast<quint64>(slatePayload.size()));
        outputPayload.append(slatePayload);
        outputFormat = QStringLiteral("binary-plain-sender");
    }

    if (armoredOut) {
        *armoredOut = armorPayload(outputPayload);
    }
    return true;
}
