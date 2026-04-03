#include "binaryslatev4reader.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMessageAuthenticationCode>
#include <QDebug>
#include <limits>

#include "slatev4.h"

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
#include "monocypher.h"
#endif

namespace {

const int kAgeFileKeySize = 16;
const int kAgePayloadNonceSize = 16;
const int kAgeChunkSize = 64 * 1024;
const int kChaChaOverhead = 16;

/**
 * @brief formatNanogrin
 * @param value
 * @return
 */
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

/**
 * @brief decodeBase64Raw
 * @param input
 * @return
 */
QByteArray decodeBase64Raw(const QString &input)
{
    QByteArray normalized = input.trimmed().toUtf8();
    const int pad = normalized.size() % 4;
    if (pad != 0) {
        normalized.append(QByteArray(4 - pad, '='));
    }
    return QByteArray::fromBase64(normalized, QByteArray::Base64Encoding);
}

/**
 * @brief hkdfSha256
 * @param ikm
 * @param salt
 * @param info
 * @param outputLength
 * @return
 */
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

/**
 * @brief x25519SecretFromWalletSecret
 * @param walletSecret
 * @return
 */
QByteArray x25519SecretFromWalletSecret(const QByteArray &walletSecret)
{
    if (walletSecret.size() != 32) {
        return QByteArray();
    }
    // Use BLAKE2b-512 to match Monocypher's crypto_eddsa_key_pair which uses
    // crypto_blake2b internally (not SHA-512). The X25519 scalar must be derived
    // the same way so that our x25519 public key matches what the sender computed
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

#ifdef GRIN_HAS_SLATEPACK_CRYPTO

/**
 * @brief deriveX25519PublicKey
 * @param privateKey
 * @param publicKeyOut
 * @return
 */
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

/**
 * @brief deriveX25519SharedSecret
 * @param privateKey
 * @param peerPublicKey
 * @param sharedSecretOut
 * @return
 */
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

/**
 * @brief chacha20Poly1305Decrypt
 * @param key
 * @param nonce
 * @param ciphertext
 * @param plaintextOut
 * @return
 */
bool chacha20Poly1305Decrypt(const QByteArray &key,
                             const QByteArray &nonce,
                             const QByteArray &ciphertext,
                             QByteArray *plaintextOut)
{
    if (!plaintextOut || key.size() != 32 || nonce.size() != 12 || ciphertext.size() < kChaChaOverhead) {
        return false;
    }

    const QByteArray tag = ciphertext.right(kChaChaOverhead);
    const QByteArray encrypted = ciphertext.left(ciphertext.size() - kChaChaOverhead);

    QByteArray plaintext(encrypted.size(), Qt::Uninitialized);
    crypto_aead_ctx ctx;
    crypto_aead_init_ietf(&ctx,
                          reinterpret_cast<const uint8_t *>(key.constData()),
                          reinterpret_cast<const uint8_t *>(nonce.constData()));
    if (crypto_aead_read(&ctx,
                         reinterpret_cast<uint8_t *>(plaintext.data()),
                         reinterpret_cast<const uint8_t *>(tag.constData()),
                         0, 0,
                         reinterpret_cast<const uint8_t *>(encrypted.constData()),
                         encrypted.size()) != 0) {
        return false;
    }
    *plaintextOut = plaintext;
    return true;
}
#endif

/**
 * @brief incrementStreamNonce
 * @param nonce
 * @return
 */
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
struct AgeStanza
{
    QString type;
    QStringList args;
    QByteArray body;
};

struct DecryptedSlatepackPayload
{
    QByteArray payload;
    QString sender;
    QStringList recipients;
};

/**
 * @brief parseSlatepackAddress
 * @param cursor
 * @param addressOut
 * @return
 */
bool parseSlatepackAddress(ByteCursor *cursor, QString *addressOut)
{
    if (!cursor || !addressOut) {
        return false;
    }

    bool ok = false;
    const int addressLength = static_cast<int>(cursor->readU8(&ok));
    if (!ok || addressLength <= 0) {
        return false;
    }

    const QByteArray encodedAddress = cursor->readBytes(addressLength, &ok);
    if (!ok || encodedAddress.isEmpty()) {
        return false;
    }

    *addressOut = QString::fromUtf8(encodedAddress);
    return !addressOut->trimmed().isEmpty();
}

/**
 * @brief parseBinaryEnvelopeOptionalFields
 * @param optFields
 * @param optFlags
 * @param senderOut
 * @param recipientsOut
 * @param errorOut
 * @return
 */
bool parseBinaryEnvelopeOptionalFields(const QByteArray &optFields,
                                       quint16 optFlags,
                                       QString *senderOut,
                                       QStringList *recipientsOut,
                                       QString *errorOut)
{
    ByteCursor cursor(optFields);
    bool ok = true;

    if ((optFlags & 0x01) != 0) {
        QString sender;
        if (!parseSlatepackAddress(&cursor, &sender)) {
            if (errorOut) *errorOut = QStringLiteral("Binary Slatepack sender address is invalid.");
            return false;
        }
        if (senderOut) {
            *senderOut = sender;
        }
    }

    if ((optFlags & 0x02) != 0) {
        const quint16 recipientCount = cursor.readU16(&ok);
        if (!ok) {
            if (errorOut) *errorOut = QStringLiteral("Binary Slatepack recipient count is invalid.");
            return false;
        }
        QStringList recipients;
        for (quint16 i = 0; i < recipientCount; ++i) {
            QString recipient;
            if (!parseSlatepackAddress(&cursor, &recipient)) {
                if (errorOut) *errorOut = QStringLiteral("Binary Slatepack recipient address is invalid.");
                return false;
            }
            recipients.append(recipient);
        }
        if (recipientsOut) {
            *recipientsOut = recipients;
        }
    }

    return true;
}

/**
 * @brief extractDecryptedSlatepackPayload
 * @param decrypted
 * @param resultOut
 * @param errorOut
 * @return
 */
bool extractDecryptedSlatepackPayload(const QByteArray &decrypted,
                                      DecryptedSlatepackPayload *resultOut,
                                      QString *errorOut)
{
    if (!resultOut) {
        if (errorOut) *errorOut = QStringLiteral("Slatepack metadata output target is missing.");
        return false;
    }
    if (decrypted.size() < 6) {
        if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack metadata is truncated.");
        return false;
    }

    ByteCursor lengthCursor(decrypted.left(4));
    bool ok = false;
    const quint32 metadataLength = lengthCursor.readU32(&ok);
    if (!ok || metadataLength + 4u > static_cast<quint32>(decrypted.size())) {
        if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack metadata length is invalid.");
        return false;
    }

    ByteCursor cursor(decrypted.mid(4, static_cast<int>(metadataLength)));
    const quint16 optionalFlags = cursor.readU16(&ok);
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack metadata flags are invalid.");
        return false;
    }

    DecryptedSlatepackPayload result;
    if ((optionalFlags & 0x01) != 0) {
        if (!parseSlatepackAddress(&cursor, &result.sender)) {
            if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack sender address is invalid.");
            return false;
        }
    }

    if ((optionalFlags & 0x02) != 0) {
        const quint16 recipientCount = cursor.readU16(&ok);
        if (!ok) {
            if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack recipient count is invalid.");
            return false;
        }
        for (quint16 i = 0; i < recipientCount; ++i) {
            QString recipient;
            if (!parseSlatepackAddress(&cursor, &recipient)) {
                if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack recipient address is invalid.");
                return false;
            }
            result.recipients.append(recipient);
        }
    }

    result.payload = decrypted.mid(static_cast<int>(metadataLength) + 4);
    if (result.payload.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Decrypted Slatepack payload is empty.");
        return false;
    }

    *resultOut = result;
    return true;
}

/**
 * @brief unwrapAgeFileKey
 * @param stanzas
 * @param x25519Secret
 * @param x25519Public
 * @param fileKeyOut
 * @return
 */
bool unwrapAgeFileKey(const QList<AgeStanza> &stanzas,
                      const QByteArray &x25519Secret,
                      const QByteArray &x25519Public,
                      QByteArray *fileKeyOut)
{
    if (!fileKeyOut) {
        return false;
    }

    for (int i = 0; i < stanzas.size(); ++i) {
        const AgeStanza &stanza = stanzas.at(i);
        if (stanza.type != QStringLiteral("X25519") || stanza.args.size() != 1) {
            continue;
        }

        const QByteArray peerPublicKey = decodeBase64Raw(stanza.args.first());
        if (peerPublicKey.size() != 32) {
            continue;
        }

        QByteArray sharedSecret;
        if (!deriveX25519SharedSecret(x25519Secret, peerPublicKey, &sharedSecret)) {
            continue;
        }

        QByteArray salt = peerPublicKey;
        salt.append(x25519Public);
        const QByteArray wrappingKey = hkdfSha256(
            sharedSecret,
            salt,
            QByteArrayLiteral("age-encryption.org/v1/X25519"),
            32);

        QByteArray unwrapped;
        if (chacha20Poly1305Decrypt(wrappingKey, QByteArray(12, '\0'), stanza.body, &unwrapped)
            && unwrapped.size() == kAgeFileKeySize) {
            *fileKeyOut = unwrapped;
            return true;
        }
    }

    return false;
}

/**
 * @brief decryptAgePayload
 * @param payload
 * @param walletSecret
 * @param plaintextOut
 * @param errorOut
 * @return
 */
bool decryptAgePayload(const QByteArray &payload,
                       const QByteArray &walletSecret,
                       QByteArray *plaintextOut,
                       QString *errorOut)
{
    if (!plaintextOut) {
        if (errorOut) *errorOut = QStringLiteral("Age payload output target is missing.");
        return false;
    }
    if (walletSecret.size() != 32) {
        if (errorOut) *errorOut = QStringLiteral("Wallet secret is unavailable for Slatepack decryption.");
        return false;
    }
    if (!payload.startsWith("age-encryption.org/v1\n")) {
        if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack payload is not a valid age message.");
        return false;
    }

    const QByteArray x25519Secret = x25519SecretFromWalletSecret(walletSecret);
    QByteArray x25519Public;
    if (!deriveX25519PublicKey(x25519Secret, &x25519Public)) {
        if (errorOut) *errorOut = QStringLiteral("Failed to derive Slatepack decryption identity.");
        return false;
    }

    int offset = QByteArrayLiteral("age-encryption.org/v1\n").size();
    QByteArray headerWithoutMac = QByteArrayLiteral("age-encryption.org/v1\n");
    QList<AgeStanza> stanzas;
    QByteArray headerMac;

    while (offset < payload.size()) {
        const int lineEnd = payload.indexOf('\n', offset);
        if (lineEnd < 0) {
            if (errorOut) *errorOut = QStringLiteral("Age header is truncated.");
            return false;
        }

        const QByteArray line = payload.mid(offset, lineEnd - offset + 1);
        offset = lineEnd + 1;

        if (line.startsWith("---")) {
            const QList<QByteArray> footerParts = line.trimmed().split(' ');
            if (footerParts.size() != 2) {
                if (errorOut) *errorOut = QStringLiteral("Age footer is malformed.");
                return false;
            }
            headerWithoutMac.append("---");
            headerMac = decodeBase64Raw(QString::fromUtf8(footerParts.at(1)));
            if (headerMac.size() != 32) {
                if (errorOut) *errorOut = QStringLiteral("Age header MAC is malformed.");
                return false;
            }
            break;
        }

        if (!line.startsWith("-> ")) {
            if (errorOut) *errorOut = QStringLiteral("Age recipient stanza is malformed.");
            return false;
        }

        headerWithoutMac.append(line);
        const QList<QByteArray> parts = line.trimmed().split(' ');
        if (parts.size() < 2) {
            if (errorOut) *errorOut = QStringLiteral("Age recipient stanza is incomplete.");
            return false;
        }

        AgeStanza stanza;
        stanza.type = QString::fromUtf8(parts.at(1));
        for (int i = 2; i < parts.size(); ++i) {
            stanza.args.append(QString::fromUtf8(parts.at(i)));
        }

        while (offset < payload.size()) {
            const int bodyLineEnd = payload.indexOf('\n', offset);
            if (bodyLineEnd < 0) {
                if (errorOut) *errorOut = QStringLiteral("Age stanza body is truncated.");
                return false;
            }

            const QByteArray bodyLine = payload.mid(offset, bodyLineEnd - offset + 1);
            offset = bodyLineEnd + 1;
            const QByteArray bodyDecoded = decodeBase64Raw(QString::fromUtf8(bodyLine.trimmed()));
            if (bodyDecoded.isEmpty() && !bodyLine.trimmed().isEmpty()) {
                if (errorOut) *errorOut = QStringLiteral("Age stanza body contains invalid base64.");
                return false;
            }

            stanza.body.append(bodyDecoded);
            headerWithoutMac.append(bodyLine);
            if (bodyLine.trimmed().size() < 64) {
                break;
            }
        }

        stanzas.append(stanza);
    }

    if (headerMac.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Age header MAC is missing.");
        return false;
    }

    QByteArray fileKey;
    if (!unwrapAgeFileKey(stanzas, x25519Secret, x25519Public, &fileKey)) {
        if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack is not addressed to this wallet.");
        return false;
    }

    const QByteArray expectedHeaderMac = QMessageAuthenticationCode::hash(
        headerWithoutMac,
        hkdfSha256(fileKey, QByteArray(), QByteArrayLiteral("header"), 32),
        QCryptographicHash::Sha256);
    if (expectedHeaderMac != headerMac) {
        if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack header authentication failed.");
        return false;
    }

    if (offset + kAgePayloadNonceSize > payload.size()) {
        if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack payload nonce is truncated.");
        return false;
    }

    const QByteArray streamNonceSeed = payload.mid(offset, kAgePayloadNonceSize);
    offset += kAgePayloadNonceSize;
    const QByteArray streamKey = hkdfSha256(fileKey, streamNonceSeed, QByteArrayLiteral("payload"), 32);
    const QByteArray encryptedStream = payload.mid(offset);
    if (encryptedStream.isEmpty()) {
        if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack payload is empty.");
        return false;
    }

    QByteArray streamNonce(12, '\0');
    QByteArray decrypted;
    int streamOffset = 0;
    while (streamOffset < encryptedStream.size()) {
        const int remaining = encryptedStream.size() - streamOffset;
        const int chunkSize = qMin(kAgeChunkSize + kChaChaOverhead, remaining);
        const QByteArray chunk = encryptedStream.mid(streamOffset, chunkSize);

        QByteArray plaintext;
        QByteArray tryNonce = streamNonce;
        bool lastChunk = chunkSize < (kAgeChunkSize + kChaChaOverhead);
        if (lastChunk) {
            tryNonce[11] = 0x01;
        }

        bool ok = chacha20Poly1305Decrypt(streamKey, tryNonce, chunk, &plaintext);
        if (!ok && !lastChunk && chunkSize == (kAgeChunkSize + kChaChaOverhead)) {
            tryNonce = streamNonce;
            tryNonce[11] = 0x01;
            ok = chacha20Poly1305Decrypt(streamKey, tryNonce, chunk, &plaintext);
            lastChunk = ok;
        }

        if (!ok) {
            if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack payload authentication failed.");
            return false;
        }

        decrypted.append(plaintext);
        streamOffset += chunkSize;
        if (lastChunk) {
            if (streamOffset != encryptedStream.size()) {
                if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack contains trailing payload data.");
                return false;
            }
            *plaintextOut = decrypted;
            return true;
        }

        if (!incrementStreamNonce(&streamNonce)) {
            if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack nonce overflowed.");
            return false;
        }
    }

    if (errorOut) *errorOut = QStringLiteral("Encrypted Slatepack payload ended unexpectedly.");
    return false;
}
#endif

/**
 * @brief formatUuid
 * @param bytes
 * @return
 */
QString formatUuid(const QByteArray &bytes)
{
    if (bytes.size() != 16) {
        return QString();
    }
    const QString hex = QString::fromUtf8(bytes.toHex());
    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(hex.mid(0, 8), hex.mid(8, 4), hex.mid(12, 4), hex.mid(16, 4), hex.mid(20, 12));
}

/**
 * @brief decodeSlateV4BinaryPayload
 * @param payload
 * @param decodedOut
 * @param errorOut
 * @return
 */
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
    const bool hasParticipantCountField = (optionalFields & 0x01) != 0;
    if (hasParticipantCountField) slate.numParticipants = static_cast<int>(cursor.readU8(&ok));
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
    slate.metadata.insert(QStringLiteral("external_binary"), true);

    if (decodedOut) {
        *decodedOut = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    }
    return true;
}

/**
 * @brief finalizeDecodedSlatePayload
 * @param payload
 * @param senderAddress
 * @param recipients
 * @param decodedOut
 * @param errorOut
 * @return
 */
bool finalizeDecodedSlatePayload(const QByteArray &payload,
                                 const QString &senderAddress,
                                 const QStringList &recipients,
                                 QString *decodedOut,
                                 QString *errorOut)
{
    QString decoded;
    if (!decodeSlateV4BinaryPayload(payload, &decoded, errorOut)) {
        const QJsonDocument jsonDocument = QJsonDocument::fromJson(payload);
        if (!jsonDocument.isObject()) {
            return false;
        }
        decoded = QString::fromUtf8(jsonDocument.toJson(QJsonDocument::Indented));
    }

    QJsonDocument document = QJsonDocument::fromJson(decoded.toUtf8());
    if (!document.isObject()) {
        if (decodedOut) {
            *decodedOut = decoded;
        }
        return true;
    }

    QJsonObject object = document.object();
    if (!senderAddress.trimmed().isEmpty()) {
        object.insert(QStringLiteral("slatepack_sender"), senderAddress.trimmed());
    }
    if (!recipients.isEmpty()) {
        QJsonArray recipientArray;
        for (int i = 0; i < recipients.size(); ++i) {
            recipientArray.append(recipients.at(i));
        }
        object.insert(QStringLiteral("slatepack_recipients"), recipientArray);
    }

    if (decodedOut) {
        *decodedOut = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
    }
    return true;
}

/**
 * @brief decodeJsonSlatepackPayload
 * @param object
 * @param decryptionKey
 * @param decodedOut
 * @param errorOut
 * @return
 */
bool decodeJsonSlatepackPayload(const QJsonObject &object,
                                const QByteArray &decryptionKey,
                                QString *decodedOut,
                                QString *errorOut)
{
    const int mode = object.value(QStringLiteral("mode")).toInt(0);
    const QString sender = object.value(QStringLiteral("sender")).toString().trimmed();
    const QByteArray payloadBytes = QByteArray::fromBase64(object.value(QStringLiteral("payload")).toString().toUtf8());

    if (mode == 0) {
        return finalizeDecodedSlatePayload(payloadBytes, sender, QStringList(), decodedOut, errorOut);
    }

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    if (!decryptionKey.isEmpty()) {
        QByteArray decryptedPayload;
        QString decryptError;
        if (!decryptAgePayload(payloadBytes, decryptionKey, &decryptedPayload, &decryptError)) {
            if (errorOut) {
                *errorOut = decryptError.isEmpty()
                    ? QStringLiteral("Encrypted JSON Slatepack decryption failed.")
                    : decryptError;
            }
            return false;
        }

        DecryptedSlatepackPayload unpacked;
        if (!extractDecryptedSlatepackPayload(decryptedPayload, &unpacked, errorOut)) {
            return false;
        }
        return finalizeDecodedSlatePayload(
            unpacked.payload,
            unpacked.sender.trimmed().isEmpty() ? sender : unpacked.sender,
            unpacked.recipients,
            decodedOut,
            errorOut);
    }
#endif

    QJsonObject encrypted;
    encrypted.insert(QStringLiteral("encrypted_slatepack"), true);
    encrypted.insert(QStringLiteral("major"), 1);
    encrypted.insert(QStringLiteral("minor"), 0);
    encrypted.insert(QStringLiteral("mode"), mode);
    encrypted.insert(QStringLiteral("payload_size"), static_cast<qint64>(payloadBytes.size()));
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    encrypted.insert(QStringLiteral("note"),
                     QStringLiteral("Encrypted JSON Slatepack was recognized, but the wallet is currently locked or no matching decryption key is available."));
#else
    encrypted.insert(QStringLiteral("note"),
                     QStringLiteral("Encrypted JSON Slatepack was recognized, but this build does not support recipient decryption yet."));
#endif
    if (decodedOut) {
        *decodedOut = QString::fromUtf8(QJsonDocument(encrypted).toJson(QJsonDocument::Indented));
    }
    return true;
}

}

/**
 * @brief BinarySlateV4Reader::decodeSlatepackPayload
 * @param payload
 * @param decryptionKey
 * @param decodedOut
 * @param errorOut
 * @return
 */
bool BinarySlateV4Reader::decodeSlatepackPayload(const QByteArray &payload,
                                                 const QByteArray &decryptionKey,
                                                 QString *decodedOut,
                                                 QString *errorOut)
{

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (doc.isObject()) {
        const QJsonObject object = doc.object();
        if (object.contains(QStringLiteral("slatepack"))
            && object.contains(QStringLiteral("mode"))
            && object.contains(QStringLiteral("payload"))) {
            return decodeJsonSlatepackPayload(object, decryptionKey, decodedOut, errorOut);
        }

        const QString inner = QString::fromUtf8(QByteArray::fromBase64(object.value(QStringLiteral("payload")).toString().toUtf8()));
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
    if (!ok) {
        if (errorOut) *errorOut = QStringLiteral("Invalid binary slatepack metadata.");
        return false;
    }

    QString envelopeSender;
    QStringList envelopeRecipients;
    QString envelopeError;
    if (!parseBinaryEnvelopeOptionalFields(optFields, optFlags, &envelopeSender, &envelopeRecipients, &envelopeError)) {
        if (errorOut) {
            *errorOut = envelopeError.isEmpty()
                ? QStringLiteral("Invalid binary slatepack optional fields.")
                : envelopeError;
        }
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
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
        if (!decryptionKey.isEmpty()) {
            QByteArray decryptedPayload;
            QString decryptError;
            if (decryptAgePayload(innerPayload, decryptionKey, &decryptedPayload, &decryptError)) {
                DecryptedSlatepackPayload unpacked;
                if (!extractDecryptedSlatepackPayload(decryptedPayload, &unpacked, errorOut)) {
                    return false;
                }
                return finalizeDecodedSlatePayload(unpacked.payload,
                                                   unpacked.sender,
                                                   unpacked.recipients,
                                                   decodedOut,
                                                   errorOut);
            }
            if (errorOut) {
                *errorOut = decryptError.isEmpty()
                    ? QStringLiteral("Encrypted Slatepack decryption failed.")
                    : decryptError;
            }
            return false;
        }
#endif
        QJsonObject encrypted;
        encrypted.insert(QStringLiteral("encrypted_slatepack"), true);
        encrypted.insert(QStringLiteral("major"), static_cast<int>(major));
        encrypted.insert(QStringLiteral("minor"), static_cast<int>(minor));
        encrypted.insert(QStringLiteral("mode"), static_cast<int>(mode));
        encrypted.insert(QStringLiteral("opt_flags"), static_cast<int>(optFlags));
        encrypted.insert(QStringLiteral("payload_size"), static_cast<qint64>(payloadSize));
#ifndef GRIN_HAS_SLATEPACK_CRYPTO
        encrypted.insert(QStringLiteral("note"),
                         QStringLiteral("Encrypted Slatepack v%1.%2 was recognized, but this build does not support recipient decryption yet.")
                             .arg(QString::number(major), QString::number(minor)));
#else
        encrypted.insert(QStringLiteral("note"),
                         QStringLiteral("Encrypted Slatepack v%1.%2 was recognized, but the wallet is currently locked or no matching decryption key is available.")
                             .arg(QString::number(major), QString::number(minor)));
#endif
        if (decodedOut) {
            *decodedOut = QString::fromUtf8(QJsonDocument(encrypted).toJson(QJsonDocument::Indented));
        }
        return true;
    }

    QString decodedSlate;
    if (!decodeSlateV4BinaryPayload(innerPayload, &decodedSlate, errorOut)) {
        return false;
    }

    if (envelopeSender.trimmed().isEmpty() && envelopeRecipients.isEmpty()) {
        if (decodedOut) {
            *decodedOut = decodedSlate;
        }
        return true;
    }

    return finalizeDecodedSlatePayload(innerPayload, envelopeSender, envelopeRecipients, decodedOut, errorOut);
}
