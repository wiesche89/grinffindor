#include "grinwalletcontroller.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QClipboard>
#include <QUrl>
#include <QVector>
#include <cstdlib>
#include <algorithm>

#ifdef Q_OS_WASM
#include <emscripten.h>

EM_JS(void, browserLocalStorageSet, (const char *key, const char *value), {
    try {
        if (typeof localStorage !== "undefined") {
            localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        }
    } catch (e) {}
});

EM_JS(char *, browserLocalStorageGet, (const char *key), {
    try {
        if (typeof localStorage !== "undefined") {
            const value = localStorage.getItem(UTF8ToString(key));
            if (value !== null && value !== undefined) {
                const length = lengthBytesUTF8(value) + 1;
                const buffer = _malloc(length);
                stringToUTF8(value, buffer, length);
                return buffer;
            }
        }
    } catch (e) {}
    return 0;
});
#endif

#include "wallet/slatev4.h"
#include "wallet/walletoutput.h"
#include "wallet/walletscanner.h"
#include "wallet/walletselection.h"
#include "wallet/walletkeychain.h"
#include "wallet/wallettxbuilder.h"
#include "wallet/binaryslatev4reader.h"
#include "wallet/binaryslatev4writer.h"
#include "wallet/walletcryptobackend.h"
#include "nodeforeignapi.h"
#include "result.h"
#include "tip.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "transaction.h"

namespace {

const char *kWalletStorePath = "/grin-wallet/browser-wallet.json";
const char *kWalletLocalStorageKey = "grinffindor.browserWallet";
const int kMnemonicEntropyBytes = 32;
const char *kBase58Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const char *kFixedNodeUrl = "https://mainnet.grinffindor.org/v2/foreign";

QString defaultNodeUrl()
{
    return QString::fromUtf8(kFixedNodeUrl);
}

bool isNodeUrlAccepted(const QString &nodeUrl)
{
    const QUrl parsed = QUrl::fromUserInput(nodeUrl.trimmed());
    return parsed.isValid()
        && !parsed.scheme().trimmed().isEmpty()
        && !parsed.host().trimmed().isEmpty()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"));
}

QString storageRootPath()
{
#ifdef Q_OS_WASM
    return QStringLiteral("/persistent");
#else
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty() ? QStringLiteral(".wallet-data") : appData;
#endif
}

QString storageFilePath()
{
    return storageRootPath() + QString::fromUtf8(kWalletStorePath);
}

void ensureStorageReady()
{
    const QFileInfo info(storageFilePath());
    QDir().mkpath(info.absolutePath());
#ifdef Q_OS_WASM
    EM_ASM({
        if (typeof FS !== "undefined" && typeof IDBFS !== "undefined") {
            try {
                FS.mkdir('/persistent');
            } catch (e) {}
            if (!Module.grinWalletIdbMounted) {
                FS.mount(IDBFS, {}, '/persistent');
                Module.grinWalletIdbMounted = true;
                FS.syncfs(true, function(err) {});
            }
        }
    });
#endif
}

void flushStorage()
{
#ifdef Q_OS_WASM
    EM_ASM({
        if (typeof FS !== "undefined" && Module.grinWalletIdbMounted) {
            FS.syncfs(false, function(err) {});
        }
    });
#endif
}

QJsonObject defaultDocument()
{
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));

    QJsonObject walletState;
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 1);
    walletState.insert(QStringLiteral("next_child_index"), 0);
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());

    QJsonObject root;
    root.insert(QStringLiteral("wallet"), QJsonObject());
    QJsonObject nodeConfig;
    nodeConfig.insert(QStringLiteral("url"), defaultNodeUrl());
    root.insert(QStringLiteral("node"), nodeConfig);
    root.insert(QStringLiteral("wallet_state"), walletState);
    root.insert(QStringLiteral("workflow_contexts"), QJsonObject());
    return root;
}


QJsonObject loadDocument()
{
    ensureStorageReady();
#ifdef Q_OS_WASM
    if (char *storedJson = browserLocalStorageGet(kWalletLocalStorageKey)) {
        const QByteArray localJson(storedJson);
        free(storedJson);
        const QJsonDocument localDoc = QJsonDocument::fromJson(localJson);
        if (localDoc.isObject()) {
            return localDoc.object();
        }
    }
#endif
    QFile file(storageFilePath());
    if (!file.exists()) {
        return defaultDocument();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultDocument();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? doc.object() : defaultDocument();
}

bool saveDocument(const QJsonObject &document)
{
    ensureStorageReady();
    const QByteArray json = QJsonDocument(document).toJson(QJsonDocument::Indented);
#ifdef Q_OS_WASM
    browserLocalStorageSet(kWalletLocalStorageKey, json.constData());
#endif
    QSaveFile file(storageFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(json);
    if (!file.commit()) {
        return false;
    }
    flushStorage();
    return true;
}

QByteArray randomBytes(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return data;
}

quint64 amountToNanogrin(const QString &amount)
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

QStringList &mnemonicWords()
{
    static QStringList words;
    if (!words.isEmpty()) {
        return words;
    }

    QFile file(QStringLiteral(":/qml/src/wallet/resources/bip39_english.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return words;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            words.append(line);
        }
    }
    return words;
}

QByteArray bitsFromBytes(const QByteArray &bytes)
{
    QByteArray bits;
    bits.reserve(bytes.size() * 8);
    for (int i = 0; i < bytes.size(); ++i) {
        const unsigned char value = static_cast<unsigned char>(bytes.at(i));
        for (int bit = 7; bit >= 0; --bit) {
            bits.append((value & (1u << bit)) ? '\x01' : '\x00');
        }
    }
    return bits;
}

QByteArray bytesFromBits(const QByteArray &bits)
{
    QByteArray bytes;
    bytes.reserve(bits.size() / 8);
    for (int i = 0; i + 7 < bits.size(); i += 8) {
        unsigned char value = 0;
        for (int bit = 0; bit < 8; ++bit) {
            value = static_cast<unsigned char>((value << 1) | (bits.at(i + bit) ? 1 : 0));
        }
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}

QString normalizeMnemonic(const QString &mnemonic)
{
    QString normalized = mnemonic.toLower().trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

quint32 nextChildIndexFromState(const QJsonObject &walletState)
{
    return static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
}

QString mnemonicFromEntropy(const QByteArray &entropy)
{
    const QStringList &words = mnemonicWords();
    if (entropy.size() != kMnemonicEntropyBytes || words.size() != 2048) {
        return QString();
    }

    QByteArray bits = bitsFromBytes(entropy);
    const QByteArray checksumBits = bitsFromBytes(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256));
    const int checksumLength = entropy.size() * 8 / 32;
    bits.append(checksumBits.left(checksumLength));

    QStringList mnemonic;
    for (int i = 0; i + 10 < bits.size(); i += 11) {
        int index = 0;
        for (int bit = 0; bit < 11; ++bit) {
            index = (index << 1) | (bits.at(i + bit) ? 1 : 0);
        }
        mnemonic.append(words.at(index));
    }
    return mnemonic.join(QStringLiteral(" "));
}

bool entropyFromMnemonic(const QString &mnemonic, QByteArray *entropyOut)
{
    const QString normalized = normalizeMnemonic(mnemonic);
    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList &words = mnemonicWords();
    if (parts.size() != 24 || words.size() != 2048) {
        return false;
    }

    QByteArray bits;
    for (int i = 0; i < parts.size(); ++i) {
        const int index = words.indexOf(parts.at(i));
        if (index < 0) {
            return false;
        }
        for (int bit = 10; bit >= 0; --bit) {
            bits.append((index & (1 << bit)) ? '\x01' : '\x00');
        }
    }

    const int checksumLength = bits.size() / 33;
    const int entropyLength = bits.size() - checksumLength;
    const QByteArray entropy = bytesFromBits(bits.left(entropyLength));
    if (entropy.size() != kMnemonicEntropyBytes) {
        return false;
    }

    const QByteArray checksumBits = bitsFromBytes(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256));
    if (bits.mid(entropyLength, checksumLength) != checksumBits.left(checksumLength)) {
        return false;
    }

    if (entropyOut) {
        *entropyOut = entropy;
    }
    return true;
}

bool validateMnemonicValue(const QString &mnemonic)
{
    return entropyFromMnemonic(mnemonic, 0);
}

QByteArray deriveLegacyKeyMaterial(const QString &password, const QByteArray &salt)
{
    const QByteArray material = password.toUtf8() + salt;
    QByteArray digest = QCryptographicHash::hash(material, QCryptographicHash::Sha256);
    for (int i = 0; i < 120000; ++i) {
        digest = QCryptographicHash::hash(digest + material, QCryptographicHash::Sha256);
    }
    return digest;
}

QByteArray xorStream(const QByteArray &data, const QByteArray &key, const QByteArray &nonce)
{
    QByteArray output(data.size(), Qt::Uninitialized);
    int offset = 0;
    quint32 counter = 0;
    while (offset < data.size()) {
        const QByteArray block = QCryptographicHash::hash(
            key + nonce + QByteArray::number(counter++), QCryptographicHash::Sha256);
        for (int i = 0; i < block.size() && offset < data.size(); ++i, ++offset) {
            output[offset] = static_cast<char>(
                static_cast<unsigned char>(data.at(offset)) ^ static_cast<unsigned char>(block.at(i)));
        }
    }
    return output;
}

QByteArray deriveKeyMaterialV2(const QString &password, const QByteArray &salt, int iterations, int outputLength)
{
    const QByteArray material = password.toUtf8() + salt;
    QByteArray state = QCryptographicHash::hash(material, QCryptographicHash::Sha512);
    for (int i = 0; i < iterations; ++i) {
        state = QCryptographicHash::hash(state + material + QByteArray::number(i), QCryptographicHash::Sha512);
    }

    QByteArray output;
    output.reserve(outputLength);
    QByteArray blockSeed = state;
    quint32 counter = 0;
    while (output.size() < outputLength) {
        blockSeed = QCryptographicHash::hash(
            blockSeed + material + QByteArray::number(counter++),
            QCryptographicHash::Sha512);
        output.append(blockSeed);
    }
    output.truncate(outputLength);
    return output;
}

QJsonObject encryptMnemonic(const QString &mnemonic, const QString &password)
{
    const QByteArray salt = randomBytes(16);
    const QByteArray nonce = randomBytes(24);
    const int iterations = 240000;
    const QByteArray keyMaterial = deriveKeyMaterialV2(password, salt, iterations, 64);
    const QByteArray encryptionKey = keyMaterial.left(32);
    const QByteArray macKey = keyMaterial.mid(32, 32);
    const QByteArray plain = normalizeMnemonic(mnemonic).toUtf8();
    const QByteArray cipher = xorStream(plain, encryptionKey, nonce);
    const QByteArray mac = QCryptographicHash::hash(macKey + nonce + cipher + macKey, QCryptographicHash::Sha256);

    QJsonObject encrypted;
    encrypted.insert(QStringLiteral("version"), 2);
    encrypted.insert(QStringLiteral("kdf_iterations"), iterations);
    encrypted.insert(QStringLiteral("salt"), QString::fromUtf8(salt.toBase64()));
    encrypted.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
    encrypted.insert(QStringLiteral("cipher"), QString::fromUtf8(cipher.toBase64()));
    encrypted.insert(QStringLiteral("mac"), QString::fromUtf8(mac.toBase64()));
    return encrypted;
}

bool decryptMnemonic(const QJsonObject &encrypted, const QString &password, QString *mnemonicOut)
{
    const int version = encrypted.value(QStringLiteral("version")).toInt(1);
    const QByteArray salt = QByteArray::fromBase64(encrypted.value(QStringLiteral("salt")).toString().toUtf8());
    const QByteArray nonce = QByteArray::fromBase64(encrypted.value(QStringLiteral("nonce")).toString().toUtf8());
    const QByteArray cipher = QByteArray::fromBase64(encrypted.value(QStringLiteral("cipher")).toString().toUtf8());
    const QByteArray mac = QByteArray::fromBase64(encrypted.value(QStringLiteral("mac")).toString().toUtf8());
    QByteArray encryptionKey;
    QByteArray macKey;
    if (version >= 2) {
        const int iterations = std::max(1, encrypted.value(QStringLiteral("kdf_iterations")).toInt(240000));
        const QByteArray keyMaterial = deriveKeyMaterialV2(password, salt, iterations, 64);
        encryptionKey = keyMaterial.left(32);
        macKey = keyMaterial.mid(32, 32);
    } else {
        const QByteArray legacyKey = deriveLegacyKeyMaterial(password, salt);
        encryptionKey = legacyKey;
        macKey = legacyKey;
    }

    const QByteArray expectedMac = QCryptographicHash::hash(macKey + nonce + cipher + macKey, QCryptographicHash::Sha256);
    if (expectedMac != mac) {
        return false;
    }

    const QString mnemonic = normalizeMnemonic(QString::fromUtf8(xorStream(cipher, encryptionKey, nonce)));
    if (!validateMnemonicValue(mnemonic)) {
        return false;
    }
    if (mnemonicOut) {
        *mnemonicOut = mnemonic;
    }
    return true;
}

QString seedFingerprintForMnemonic(const QString &mnemonic)
{
    return QString::fromUtf8(
        QCryptographicHash::hash(normalizeMnemonic(mnemonic).toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

QString generateWorkflowId()
{
    return QString::fromUtf8(randomBytes(16).toHex());
}

QString amountStringFromJson(const QJsonObject &balances, const QString &field)
{
    return balances.value(field).toString(QStringLiteral("0.000000000"));
}

WalletOutput findTrackedOutputByCommitment(const QList<WalletOutput> &outputs, const QString &commitment)
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commitment) {
            return outputs.at(i);
        }
    }
    return WalletOutput();
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

QString encodeSlatepackArmor(const QString &payloadJson, const QString &sender)
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
    return QStringLiteral("BEGINSLATEPACK. %1. ENDSLATEPACK.\n").arg(formatArmored(encodeBase58(checksum + serialized)));
}

QString userFacingSlatepackParseNote(const QString &parseError);
QString buildSlatepackDiagnostic(const QString &kind,
                                 const QByteArray &payload,
                                 const QString &note);

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

QString decodeIncomingSlatepack(const QString &input, const QByteArray &decryptionKey)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QJsonDocument jsonDocument = QJsonDocument::fromJson(trimmed.toUtf8());
    if (jsonDocument.isObject()) {
        QString decodedPayload;
        QString parseError;
        if (BinarySlateV4Reader::decodeSlatepackPayload(trimmed.toUtf8(), decryptionKey, &decodedPayload, &parseError)) {
            return decodedPayload;
        }

        const QJsonObject object = jsonDocument.object();
        if (object.contains(QStringLiteral("slatepack")) || object.contains(QStringLiteral("payload"))) {
            const QByteArray payload = QByteArray::fromBase64(object.value(QStringLiteral("payload")).toString().toUtf8());
            return buildSlatepackDiagnostic(QStringLiteral("json"), payload, parseError);
        }

        return QString::fromUtf8(jsonDocument.toJson(QJsonDocument::Indented));
    }

    QString cleaned = trimmed;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() >= 3
        && parts.at(0) == QStringLiteral("BEGINSLATEPACK")
        && parts.at(2) == QStringLiteral("ENDSLATEPACK")) {
        const QByteArray decoded = decodeBase58(parts.at(1));
        if (decoded.size() >= 5) {
            const QByteArray payload = decoded.mid(4);
            QString decodedPayload;
            QString parseError;
            if (BinarySlateV4Reader::decodeSlatepackPayload(payload, decryptionKey, &decodedPayload, &parseError)) {
                return decodedPayload;
            }
            return buildSlatepackDiagnostic(QStringLiteral("armored"), payload, parseError);
        }
    }

    return decodeSlatepackArmor(trimmed);
}

} // namespace

GrinWalletController::GrinWalletController(QObject *parent) :
    QObject(parent),
    m_nodeApi(0),
    m_autoRefreshTimer(0),
    m_walletExists(false),
    m_walletUnlocked(false),
    m_chainHeight(0),
    m_syncStatus(QStringLiteral("Idle")),
    m_totalBalance(QStringLiteral("0.000000000")),
    m_spendableBalance(QStringLiteral("0.000000000")),
    m_lockedBalance(QStringLiteral("0.000000000")),
    m_immatureBalance(QStringLiteral("0.000000000")),
    m_scanHeight(0),
    m_walletScanInFlight(false),
    m_seedScanActive(false),
    m_seedScanNextIndex(1),
    m_broadcastStatusRefreshInFlight(false),
    m_kernelStatusCheckInFlight(false)
{
}

bool GrinWalletController::walletExists() const { return m_walletExists; }
bool GrinWalletController::walletUnlocked() const { return m_walletUnlocked; }
QString GrinWalletController::walletName() const { return m_walletName; }
QString GrinWalletController::mnemonicPreview() const { return m_mnemonicPreview; }
QString GrinWalletController::seedFingerprint() const { return m_seedFingerprint; }
QString GrinWalletController::nodeUrl() const { return m_nodeUrl; }
qulonglong GrinWalletController::chainHeight() const { return m_chainHeight; }
QString GrinWalletController::syncStatus() const { return m_syncStatus; }
QString GrinWalletController::totalBalance() const { return m_totalBalance; }
QString GrinWalletController::spendableBalance() const { return m_spendableBalance; }
QString GrinWalletController::lockedBalance() const { return m_lockedBalance; }
QString GrinWalletController::immatureBalance() const { return m_immatureBalance; }
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }
QString GrinWalletController::lastError() const { return m_lastError; }
QString GrinWalletController::lastInfo() const { return m_lastInfo; }
QString GrinWalletController::workflowId() const { return m_workflowId; }
QString GrinWalletController::workflowState() const { return m_workflowState; }
QString GrinWalletController::workflowMode() const { return m_workflowMode; }
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }
QVariantList GrinWalletController::transactionHistory() const
{
    QVariantList history;
    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    history.reserve(transactions.size());
    for (int i = transactions.size() - 1; i >= 0; --i) {
        history.append(transactions.at(i).toObject().toVariantMap());
    }
    return history;
}

void GrinWalletController::initialize()
{
    loadFromStorage();
    connectNodeClient();
    startAutoRefresh();
    refreshNodeStatus();
}

QString GrinWalletController::generateMnemonic() const
{
    return mnemonicFromEntropy(randomBytes(kMnemonicEntropyBytes));
}

bool GrinWalletController::validateMnemonic(const QString &mnemonic) const
{
    return validateMnemonicValue(mnemonic);
}

void GrinWalletController::createWallet(const QString &walletName, const QString &password)
{
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }

    const QString mnemonic = generateMnemonic();
    if (mnemonic.isEmpty()) {
        setLastError(QStringLiteral("Failed to generate a valid seed phrase."));
        return;
    }

    QJsonObject document = defaultDocument();
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), seedFingerprintForMnemonic(mnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptMnemonic(mnemonic, password));
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("generated"));
    document.insert(QStringLiteral("wallet"), wallet);

    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = mnemonic;
    m_mnemonicPreview = mnemonic;
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet created locally. Save the seed phrase now - it will not be shown again after this session."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::importWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{
    restoreWallet(walletName, mnemonic, password);
}

void GrinWalletController::restoreWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{
    const QString normalizedMnemonic = normalizeMnemonic(mnemonic);
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }
    if (!validateMnemonicValue(normalizedMnemonic)) {
        setLastError(QStringLiteral("Mnemonic is not valid BIP39 input."));
        return;
    }

    QJsonObject document = defaultDocument();
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), seedFingerprintForMnemonic(normalizedMnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptMnemonic(normalizedMnemonic, password));
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("restored"));
    document.insert(QStringLiteral("wallet"), wallet);

    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = normalizedMnemonic;
    m_mnemonicPreview.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet restored locally. Seed is encrypted in local storage and stays hidden after setup."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::unlockWallet(const QString &password)
{
    const QJsonObject wallet = loadDocument().value(QStringLiteral("wallet")).toObject();
    QString mnemonic;
    if (wallet.isEmpty()
        || !decryptMnemonic(wallet.value(QStringLiteral("encrypted_seed")).toObject(), password, &mnemonic)) {
        setLastError(QStringLiteral("Failed to unlock wallet. Password is invalid or local data is corrupted."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = wallet.value(QStringLiteral("name")).toString();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    m_sessionMnemonic = mnemonic;
    m_mnemonicPreview.clear();
    emit walletChanged();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet unlocked locally."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::lockWallet()
{
    m_walletUnlocked = false;
    m_sessionMnemonic.clear();
    m_mnemonicPreview.clear();
    emit walletChanged();
    setLastInfo(QStringLiteral("Wallet locked. Seed material cleared from the UI state."));
}

void GrinWalletController::dismissMnemonicPreview()
{
    if (m_mnemonicPreview.isEmpty()) {
        return;
    }

    m_mnemonicPreview.clear();
    emit walletChanged();
    setLastInfo(QStringLiteral("Seed phrase hidden. Use your password to unlock the wallet next time."));
}

void GrinWalletController::deleteWallet()
{
    if (!saveDocument(defaultDocument())) {
        setLastError(QStringLiteral("Failed to delete the local wallet configuration."));
        return;
    }

    clearWorkflow();
    loadFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Local wallet configuration deleted. You can now create or restore a wallet."));
}

bool GrinWalletController::setNodeUrl(const QString &nodeUrl)
{
    const QString trimmed = nodeUrl.trimmed();
    if (!isNodeUrlAccepted(trimmed)) {
        setLastError(QStringLiteral("Node URL must be a valid http or https endpoint."));
        return false;
    }

    QJsonObject document = loadDocument();
    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("url"), trimmed);
    document.insert(QStringLiteral("node"), node);
    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist node settings."));
        return false;
    }

    m_nodeUrl = trimmed;
    emit nodeConfigChanged();
    connectNodeClient();
    setLastError(QString());
    setLastInfo(QStringLiteral("External node updated. Reconnecting to %1").arg(trimmed));
    refreshNodeStatus();
    return true;
}

void GrinWalletController::resetNodeUrl()
{
    setNodeUrl(defaultNodeUrl());
}

void GrinWalletController::refreshNodeStatus()
{
    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    m_syncStatus = QStringLiteral("Querying node...");
    emit statusChanged();
    m_nodeApi->getTipAsync();
}

void GrinWalletController::syncWallet()
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before running a wallet sync."));
        setLastInfo(QStringLiteral("Wallet sync was skipped because the wallet is locked."));
        return;
    }

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        setLastError(QStringLiteral("Node tip is not available yet. Try sync again after refresh."));
        return;
    }
    requestWalletScan();
}

void GrinWalletController::rescanWallet()
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before starting a full rescan."));
        setLastInfo(QStringLiteral("Full rescan was skipped because the wallet is locked."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastError(QStringLiteral("A wallet scan is already running."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("transactions"), QJsonArray());
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 0);
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("full-rescan"));
    walletState.insert(QStringLiteral("last_synced_at"), QString());
    document.insert(QStringLiteral("wallet_state"), walletState);
    document.insert(QStringLiteral("workflow_contexts"), QJsonObject());
    saveDocument(document);
    refreshStateFromStorage();

    m_seedScanNextIndex = 1;
    setLastError(QString());
    setLastInfo(QStringLiteral("Full wallet rescan queued from the beginning."));

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        return;
    }

    requestWalletScan();
}

QString GrinWalletController::requestPasteText() const
{
#ifdef Q_OS_WASM
    const char *value = emscripten_run_script_string(
        "(function(){"
        "  var text = window.prompt('Paste mnemonic here', '');"
        "  return text === null ? '' : text;"
        "})()");
    return QString::fromUtf8(value ? value : "");
#else
    const QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString();
#endif
}

bool GrinWalletController::isValidNodeUrl(const QString &nodeUrl) const
{
    return isNodeUrlAccepted(nodeUrl);
}

QString GrinWalletController::createSlatepackTemplate(const QString &sender) const
{
    QJsonObject slate;
    slate.insert(QStringLiteral("version"), QStringLiteral("v4"));
    slate.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    slate.insert(QStringLiteral("wallet"), m_walletName);
    slate.insert(QStringLiteral("note"), QStringLiteral("Placeholder slatepack until full interactive tx building is implemented."));
    return encodeSlatepackArmor(QString::fromUtf8(QJsonDocument(slate).toJson(QJsonDocument::Indented)), sender.trimmed());
}

void GrinWalletController::startSendWorkflow(const QString &amount, const QString &note)
{
    const quint64 requestedAmount = amountToNanogrin(amount);
    if (requestedAmount == 0) {
        setLastError(QStringLiteral("Send amount must be greater than zero."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const WalletSelection::Result selection =
        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, m_chainHeight);
    if (!selection.success) {
        setLastError(selection.error);
        return;
    }

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                outputs[i].locked = true;
                outputs[i].workflowId.clear();
            }
        }
    }

    SlateV4 slate;
    const QString workflowId = generateWorkflowId();
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                outputs[i].workflowId = workflowId;
            }
        }
    }
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    const WalletCryptoBackend::ParticipantContext senderContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    slate.state = SlateV4::Standard1;
    slate.amount = amount.trimmed();
    slate.fee = QStringLiteral("%1.%2")
        .arg(QString::number(selection.fee / 1000000000ULL))
        .arg(QString::number(selection.fee % 1000000000ULL), 9, QLatin1Char('0'));
    slate.offset = WalletCryptoBackend::createOffset(m_seedFingerprint, slate.id);
    slate.signatures.append(WalletCryptoBackend::createParticipantData(senderContext));
    slate.hasPaymentProof = true;
    slate.paymentProof = WalletCryptoBackend::createPaymentProof(senderContext, senderContext);
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("grin-browser-wallet"));
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    slate.metadata.insert(QStringLiteral("wallet"), m_walletName);
    slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    slate.metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonObject localContext;
    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    QJsonArray selectedCommitments;
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        selectedCommitments.append(selection.selectedOutputs.at(i).commitment);
    }
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    if (selection.change > 0) {
        const QString changeAmount = QStringLiteral("%1.%2")
            .arg(QString::number(selection.change / 1000000000ULL))
            .arg(QString::number(selection.change % 1000000000ULL), 9, QLatin1Char('0'));
        WalletOutput changeOutput;
        SlateV4::Commit changeCommit;
        QString outputError;
        if (buildOwnedOutput(QStringLiteral("change"), changeAmount, &changeOutput, &changeCommit, &outputError)) {
            storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (!outputError.isEmpty()) {
            setLastInfo(QStringLiteral("Change output fallback used: %1").arg(outputError));
        }
    }
    storeWorkflowContext(workflowId, localContext);
    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());
    const QString decoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(slate, &armoredSlatepack, &writerError)) {
        armoredSlatepack = encodeSlatepackArmor(decoded, QString());
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(slate.workflowId(), slate.modeCode(), slate.stateCode(), armoredSlatepack, decoded);
    setLastInfo(QStringLiteral("SEND workflow started at S1. Share the generated Slatepack with the receiver."));
}

void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    SlateV4 slate;
    const QString workflowId = generateWorkflowId();
    const WalletCryptoBackend::ParticipantContext receiverContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    slate.state = SlateV4::Invoice1;
    slate.amount = amount.trimmed();
    slate.offset = WalletCryptoBackend::createOffset(m_seedFingerprint, slate.id);
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiverContext));
    WalletOutput invoiceOutput;
    SlateV4::Commit invoiceCommit;
    QString outputError;
    if (buildOwnedOutput(QStringLiteral("invoice"), slate.amount, &invoiceOutput, &invoiceCommit, &outputError)) {
        slate.commitments.append(invoiceCommit);
        storeOwnedOutput(invoiceOutput);
    } else {
        const WalletCryptoBackend::CommitmentResult fallbackCommit =
            WalletCryptoBackend::createCommitment(m_seedFingerprint, slate.id, QStringLiteral("invoice"), slate.amount);
        if (!fallbackCommit.success) {
            setLastError(!outputError.isEmpty()
                ? QStringLiteral("Failed to derive invoice output: %1").arg(outputError)
                : (fallbackCommit.error.isEmpty()
                    ? QStringLiteral("Failed to create invoice commitment.")
                    : fallbackCommit.error));
            return;
        }

        slate.commitments.append(fallbackCommit.commit);
        storeOwnedOutput(QStringLiteral("invoice"), slate.amount, fallbackCommit.commit);
    }
    slate.hasPaymentProof = true;
    slate.paymentProof = WalletCryptoBackend::createPaymentProof(receiverContext, receiverContext);
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("grin-browser-wallet"));
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    slate.metadata.insert(QStringLiteral("wallet"), m_walletName);
    slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    slate.metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());
    const QString decoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(slate, &armoredSlatepack, &writerError)) {
        armoredSlatepack = encodeSlatepackArmor(decoded, QString());
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(slate.workflowId(), slate.modeCode(), slate.stateCode(), armoredSlatepack, decoded);
    setLastInfo(QStringLiteral("RECEIVE workflow started at I1. Share the invoice Slatepack with the sender."));
}

void GrinWalletController::processWorkflowSlatepack(const QString &slatepack)
{
    QByteArray decryptionKey;
    if (m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            decryptionKey = keychain.slatepackSecretKey();
        }
    }

    const QString decoded = decodeIncomingSlatepack(slatepack, decryptionKey);
    if (decoded.isEmpty()) {
        setLastError(QStringLiteral("Incoming Slatepack could not be decoded."));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(decoded.toUtf8());
    if (!document.isObject()) {
        setLastError(QStringLiteral("Decoded Slatepack is not valid JSON."));
        return;
    }
    if (document.object().value(QStringLiteral("encrypted_slatepack")).toBool()) {
        const QString info = document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Encrypted Slatepacks are not supported yet."));
        setLastError(info);
        setWorkflow(QString(), QString(), QString(), slatepack, QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return;
    }
    if (document.object().value(QStringLiteral("external_slatepack")).toBool()) {
        setLastError(document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Incoming Slatepack armor was recognized, but payload parsing failed.")));
        setWorkflow(QString(), QString(), QString(), slatepack, QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return;
    }

    SlateV4 slate = SlateV4::fromJson(document.object());
    if (slate.workflowId().isEmpty() && !slate.id.isEmpty() && slate.state != SlateV4::Unknown) {
        slate.metadata.insert(QStringLiteral("workflow_id"), slate.id);
        slate.metadata.insert(QStringLiteral("workflow"),
                              slate.metadata.value(QStringLiteral("external_binary")).toBool()
                                  ? QStringLiteral("external-grin-slatepack")
                                  : QStringLiteral("imported-slatepack"));
    }
    if (slate.network().trimmed().isEmpty()) {
        slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    }

    const QString workflowId = slate.workflowId();
    const QString mode = slate.modeCode();
    const QString state = slate.stateCode();
    if (workflowId.isEmpty() || mode == QStringLiteral("unknown") || state == QStringLiteral("NA")) {
        setLastError(QStringLiteral("Incoming Slatepack is missing workflow metadata."));
        return;
    }

    if (slate.isFinalState()) {
        setWorkflow(workflowId, mode, state, slatepack, QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented)));
        setLastInfo(QStringLiteral("Workflow %1 is already complete at %2.").arg(workflowId, state));
        return;
    }

    const QString localRoleTag =
        (state == QStringLiteral("S1")) ? QStringLiteral("receiver")
      : (state == QStringLiteral("S2")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I1")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I2")) ? QStringLiteral("receiver")
      : QString();

    if (localRoleTag.isEmpty()) {
        setLastError(QStringLiteral("Unsupported workflow transition."));
        return;
    }

    QString cryptoError;
    if (slate.metadata.value(QStringLiteral("external_binary")).toBool()
        && state == QStringLiteral("S1")
        && localRoleTag == QStringLiteral("receiver")) {
        const QString receiverOffset = WalletCryptoBackend::createOffset(
            m_seedFingerprint, slate.workflowId() + QStringLiteral(":receiver"));
        const QString adjustedOffset = WalletCryptoBackend::addOffsets(slate.offset, receiverOffset, &cryptoError);
        if (adjustedOffset.isEmpty()) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to adjust receiver offset.") : cryptoError);
            return;
        }
        slate.offset = adjustedOffset;
        slate.metadata.insert(QStringLiteral("receiver_offset"), receiverOffset);
        if (slate.commitments.isEmpty()) {
            WalletOutput receiveOutput;
            SlateV4::Commit receiveCommit;
            if (buildOwnedOutput(QStringLiteral("receive"), slate.amount, &receiveOutput, &receiveCommit, &cryptoError)) {
                receiveOutput.workflowId = workflowId;
                receiveOutput.pending = true;
                receiveOutput.locked = false;
                slate.commitments.append(receiveCommit);
                slate.metadata.insert(QStringLiteral("receiver_blind"), receiveOutput.blindingFactor);
                slate.metadata.insert(QStringLiteral("receiver_child_index"), static_cast<int>(receiveOutput.childIndex));
                slate.metadata.insert(QStringLiteral("receiver_key_path"), receiveOutput.keyPath);
                storeOwnedOutput(receiveOutput);
            } else {
                setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to derive receiver output.") : cryptoError);
                return;
            }
        }
    }

    if (!WalletCryptoBackend::applyRound2Signature(&slate, m_seedFingerprint, localRoleTag, &cryptoError)) {
        setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to apply round 2 signature.") : cryptoError);
        return;
    }

    slate.advanceState();
    const QString nextState = slate.stateCode();
    const bool externalBinary = slate.metadata.value(QStringLiteral("external_binary")).toBool();
    if (!externalBinary && slate.commitments.isEmpty()) {
        const WalletCryptoBackend::CommitmentResult commitResult = WalletCryptoBackend::createCommitment(
            m_seedFingerprint, slate.id, mode == QStringLiteral("invoice") ? QStringLiteral("invoice") : QStringLiteral("send"), slate.amount);
        if (!commitResult.success) {
            setLastError(commitResult.error.isEmpty()
                ? QStringLiteral("Failed to create workflow commitment.")
                : commitResult.error);
            return;
        }

        slate.commitments.append(commitResult.commit);
        if (mode == QStringLiteral("invoice")) {
            storeOwnedOutput(QStringLiteral("invoice"), slate.amount, commitResult.commit);
        }
    }
    slate.metadata.insert(QStringLiteral("processed_by"), m_walletName);
    slate.metadata.insert(QStringLiteral("processed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if ((nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3"))
        && !WalletCryptoBackend::finalizeSlate(&slate, &cryptoError)) {
        setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to finalize slate signature.") : cryptoError);
        return;
    }

    persistWorkflowTransaction(slate, false);

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        const QJsonObject localContext = workflowContext(workflowId);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        if (!selectedCommitments.isEmpty()) {
            const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
            const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
            QList<WalletOutput> selectedInputs;
            for (int i = 0; i < selectedCommitments.size(); ++i) {
                selectedInputs.append(findTrackedOutputByCommitment(
                    trackedOutputs, selectedCommitments.at(i).toString()));
            }

            WalletOutput receiverOutput;
            if (!slate.commitments.isEmpty()) {
                receiverOutput.commitment = slate.commitments.first().commitment;
                receiverOutput.proof = slate.commitments.first().proof;
                receiverOutput.amount = slate.amount;
            }

            WalletOutput changeOutput;
            const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
            if (!changeCommit.isEmpty()) {
                changeOutput = findTrackedOutputByCommitment(trackedOutputs, changeCommit);
                if (changeOutput.commitment.isEmpty()) {
                    changeOutput.commitment = changeCommit;
                    changeOutput.proof = localContext.value(QStringLiteral("change_proof")).toString();
                    changeOutput.amount = localContext.value(QStringLiteral("change_amount_display")).toString();
                }
            }

            const WalletTxBuilder::BuildResult txBuild = WalletTxBuilder::buildTransactionSkeleton(
                slate,
                selectedInputs,
                receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput,
                changeOutput.commitment.isEmpty() ? 0 : &changeOutput);
            if (txBuild.success) {
                slate.metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                slate.metadata.insert(QStringLiteral("tx_ready"), true);
                persistWorkflowTransaction(slate, false);
                finalizeWorkflowOutputs(slate, false);
            } else {
                slate.metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
            }
        }
    }

    const QString updatedDecoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString updatedSlatepack;
    if (!BinarySlateV4Writer::encodeSlatepack(slate, &updatedSlatepack, &cryptoError)) {
        if (externalBinary) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to encode binary Slatepack.") : cryptoError);
            return;
        }
        updatedSlatepack = encodeSlatepackArmor(updatedDecoded, QString());
    }
    setWorkflow(workflowId, mode, nextState, updatedSlatepack, updatedDecoded);

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        setLastInfo(QStringLiteral("Workflow %1 advanced to %2 and reached the final exchange step.").arg(workflowId, nextState));
    } else {
        setLastInfo(QStringLiteral("Workflow %1 advanced from %2 to %3.").arg(workflowId, state, nextState));
    }
}

void GrinWalletController::clearWorkflow()
{
    setWorkflow(QString(), QString(), QString(), QString(), QString());
    setLastInfo(QStringLiteral("Workflow state cleared."));
}

void GrinWalletController::broadcastCurrentWorkflowTransaction()
{
    const QJsonDocument workflowDoc = QJsonDocument::fromJson(m_workflowDecoded.toUtf8());
    if (!workflowDoc.isObject()) {
        setLastError(QStringLiteral("Current workflow does not contain decodable transaction data."));
        return;
    }

    const QJsonObject txSkeleton = workflowDoc.object().value(QStringLiteral("tx_skeleton")).toObject();
    if (txSkeleton.isEmpty()) {
        setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
        return;
    }

    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    const SlateV4 slate = SlateV4::fromJson(workflowDoc.object());
    persistWorkflowTransaction(slate, true);
    finalizeWorkflowOutputs(slate, true);
    m_pendingBroadcastWorkflowId = slate.workflowId();
    m_nodeApi->pushTransactionAsync(Transaction::fromJson(txSkeleton), true);
}

void GrinWalletController::broadcastTransaction(const QString &workflowId)
{
    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        const QJsonObject txSkeleton = tx.value(QStringLiteral("tx_skeleton")).toObject();
        if (txSkeleton.isEmpty()) {
            setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
            return;
        }
        if (!m_nodeApi) {
            connectNodeClient();
        }
        if (!m_nodeApi) {
            setLastError(QStringLiteral("Node client is not configured."));
            return;
        }

        SlateV4 slate;
        slate.id = tx.value(QStringLiteral("slate_id")).toString();
        slate.amount = tx.value(QStringLiteral("amount")).toString();
        slate.fee = tx.value(QStringLiteral("fee")).toString();
        slate.offset = tx.value(QStringLiteral("offset")).toString();
        slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        slate.setStateFromCode(tx.value(QStringLiteral("state")).toString());
        persistWorkflowTransaction(slate, true);
        finalizeWorkflowOutputs(slate, true);
        m_pendingBroadcastWorkflowId = workflowId;
        m_nodeApi->pushTransactionAsync(Transaction::fromJson(txSkeleton), true);
        return;
    }

    setLastError(QStringLiteral("Transaction not found in wallet history."));
}

void GrinWalletController::cancelTransaction(const QString &workflowId)
{
    if (workflowId.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Workflow id is required for cancel."));
        return;
    }

    const QJsonArray existingTransactions = loadDocument()
                                                .value(QStringLiteral("wallet_state"))
                                                .toObject()
                                                .value(QStringLiteral("transactions"))
                                                .toArray();
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject tx = existingTransactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        if (tx.value(QStringLiteral("confirmations")).toInt() > 0
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("confirmed")) {
            setLastError(QStringLiteral("Confirmed transactions can no longer be cancelled."));
            return;
        }
        break;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (outputs[i].spent) {
            continue;
        }

        if (outputs[i].source == QStringLiteral("change")
            || outputs[i].source == QStringLiteral("receive")
            || outputs[i].source == QStringLiteral("invoice")) {
            outputs.removeAt(i);
            --i;
            continue;
        }

        outputs[i].locked = false;
        outputs[i].pending = false;
        outputs[i].workflowId.clear();
    }

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() == workflowId) {
            tx.insert(QStringLiteral("status"), QStringLiteral("cancelled"));
            tx.insert(QStringLiteral("cancelled_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            transactions.replace(i, tx);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    if (m_workflowId == workflowId) {
        clearWorkflow();
    }
    setLastError(QString());
    setLastInfo(QStringLiteral("Transaction %1 cancelled and locks released.").arg(workflowId));
}

QString GrinWalletController::encodeSlatepack(const QString &slateJson, const QString &sender) const
{
    const QString trimmed = slateJson.trimmed();
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8());
    if (document.isObject()) {
        const SlateV4 slate = SlateV4::fromJson(document.object());
        if (slate.state != SlateV4::Unknown && !slate.id.trimmed().isEmpty()) {
            QString binarySlatepack;
            QString writerError;
            if (BinarySlateV4Writer::encodeSlatepack(slate, &binarySlatepack, &writerError)) {
                return binarySlatepack;
            }
        }
    }

    return encodeSlatepackArmor(trimmed, sender.trimmed());
}

QString GrinWalletController::decodeSlatepack(const QString &slatepack) const
{
    QByteArray decryptionKey;
    if (m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            decryptionKey = keychain.slatepackSecretKey();
        }
    }
    return decodeIncomingSlatepack(slatepack, decryptionKey);
}

void GrinWalletController::loadFromStorage()
{
    const QJsonObject document = loadDocument();
    const QJsonObject wallet = document.value(QStringLiteral("wallet")).toObject();

    m_walletExists = !wallet.isEmpty();
    m_walletUnlocked = false;
    m_walletName = wallet.value(QStringLiteral("name")).toString();
    m_sessionMnemonic.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    m_mnemonicPreview.clear();
    const QJsonObject node = document.value(QStringLiteral("node")).toObject();
    const QString storedNodeUrl = node.value(QStringLiteral("url")).toString();
    m_nodeUrl = isNodeUrlAccepted(storedNodeUrl) ? storedNodeUrl : defaultNodeUrl();

    emit walletChanged();
    emit nodeConfigChanged();
    refreshStateFromStorage();
}

void GrinWalletController::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

void GrinWalletController::setLastInfo(const QString &info)
{
    m_lastInfo = info;
    emit lastInfoChanged();
}

void GrinWalletController::setWorkflow(const QString &id, const QString &mode, const QString &state, const QString &slatepack, const QString &decoded)
{
    m_workflowId = id;
    m_workflowMode = mode;
    m_workflowState = state;
    m_workflowSlatepack = slatepack;
    m_workflowDecoded = decoded;
    emit workflowChanged();
}

void GrinWalletController::connectNodeClient()
{
    if (m_nodeApi) {
        m_nodeApi->deleteLater();
        m_nodeApi = 0;
    }
    if (m_nodeUrl.trimmed().isEmpty()) {
        return;
    }

    m_nodeApi = new NodeForeignApi(m_nodeUrl, QString());
    m_nodeApi->setParent(this);

    connect(m_nodeApi, &NodeForeignApi::getTipFinished, this, [this](const Result<Tip> &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Node query failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            return;
        }

        const Tip tip = result.value();
        m_chainHeight = tip.height();
        m_syncStatus = QStringLiteral("Connected to external node");
        refreshTransactionConfirmations();
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QStringLiteral("Node tip updated to height %1.").arg(QString::number(m_chainHeight)));

        if (m_walletUnlocked
            && !m_sessionMnemonic.trimmed().isEmpty()
            && !m_walletScanInFlight
            && !m_seedScanActive) {
            if (m_scanHeight == 0) {
                rescanWallet();
            } else {
                requestWalletScan();
            }
        }

        refreshBroadcastStatuses();
    });

    connect(m_nodeApi, &NodeForeignApi::getOutputsFinished, this, [this](const Result<QList<OutputPrintable> > &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Wallet scan failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            m_walletScanInFlight = false;
            return;
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
        tracked = WalletScanner::reconcileTrackedOutputs(tracked, result.value());

        walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_chainHeight));
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("tracked-outputs"));
        walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        document.insert(QStringLiteral("wallet_state"), walletState);

        if (!saveDocument(document)) {
            setLastError(QStringLiteral("Failed to persist wallet scan results."));
            return;
        }

        refreshStateFromStorage();
        m_syncStatus = QStringLiteral("Wallet outputs synced");
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QStringLiteral("Wallet scan updated %1 tracked outputs from node data.")
                        .arg(QString::number(tracked.size())));
        m_walletScanInFlight = false;
    });

    connect(m_nodeApi, &NodeForeignApi::getUnspentOutputsFinished, this, [this](const Result<OutputListing> &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Seed scan failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            m_seedScanActive = false;
            m_walletScanInFlight = false;
            return;
        }

        if (!m_seedScanActive || !m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
            return;
        }

        WalletKeychain keychain(m_sessionMnemonic);
        if (!keychain.isValid()) {
            setLastError(QStringLiteral("Wallet keychain could not be derived for seed scan."));
            m_seedScanActive = false;
            m_walletScanInFlight = false;
            return;
        }

        const QList<WalletOutput> discovered =
            WalletScanner::discoverOwnedOutputs(result.value().outputs(), keychain);
        for (int i = 0; i < discovered.size(); ++i) {
            bool exists = false;
            for (int j = 0; j < m_seedScanDiscovered.size(); ++j) {
                if (m_seedScanDiscovered.at(j).commitment == discovered.at(i).commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_seedScanDiscovered.append(discovered.at(i));
            }
        }

        const OutputListing listing = result.value();
        if (listing.lastRetrievedIndex() > 0 && listing.highestIndex() > 0
            && listing.lastRetrievedIndex() < listing.highestIndex()) {
            m_seedScanNextIndex = listing.lastRetrievedIndex() + 1;
            QJsonObject document = loadDocument();
            QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
            walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(listing.lastRetrievedIndex()));
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            m_syncStatus = QStringLiteral("Seed scan page %1 / %2")
                               .arg(QString::number(listing.lastRetrievedIndex()))
                               .arg(QString::number(listing.highestIndex()));
            emit statusChanged();
            m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
            return;
        }
        if (listing.lastRetrievedIndex() == 0 && discovered.size() >= 1000) {
            m_seedScanNextIndex += 1000;
            QJsonObject document = loadDocument();
            QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
            walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(m_seedScanNextIndex - 1));
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            m_syncStatus = QStringLiteral("Seed scan page starting at %1")
                               .arg(QString::number(m_seedScanNextIndex));
            emit statusChanged();
            m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
            return;
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
        for (int i = 0; i < m_seedScanDiscovered.size(); ++i) {
            bool exists = false;
            for (int j = 0; j < tracked.size(); ++j) {
                if (tracked.at(j).commitment == m_seedScanDiscovered.at(i).commitment) {
                    WalletOutput merged = tracked.at(j);
                    const WalletOutput discovered = m_seedScanDiscovered.at(i);
                    merged.proof = discovered.proof;
                    merged.amount = discovered.amount;
                    merged.keyPath = discovered.keyPath;
                    merged.blindingFactor = discovered.blindingFactor;
                    merged.childIndex = discovered.childIndex;
                    merged.height = discovered.height;
                    merged.coinbase = discovered.coinbase;
                    merged.onChain = discovered.onChain;
                    merged.spent = discovered.spent;
                    if (discovered.onChain && !discovered.spent) {
                        merged.pending = false;
                        merged.locked = false;
                    }
                    tracked[j] = merged;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                tracked.append(m_seedScanDiscovered.at(i));
            }
        }

        quint32 nextChildIndex = nextChildIndexFromState(walletState);
        for (int i = 0; i < tracked.size(); ++i) {
            if (tracked.at(i).childIndex + 1 > nextChildIndex) {
                nextChildIndex = tracked.at(i).childIndex + 1;
            }
        }

        walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_chainHeight));
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        walletState.insert(QStringLiteral("restore_leaf_index"),
                           QString::number(listing.lastRetrievedIndex() > 0
                                               ? listing.lastRetrievedIndex()
                                               : (m_seedScanNextIndex > 0 ? m_seedScanNextIndex - 1 : 1)));
        walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(nextChildIndex));
        const QString previousSyncMode = walletState.value(QStringLiteral("last_sync_mode")).toString();
        const bool rebuildingTransactions = previousSyncMode == QStringLiteral("full-rescan");
        if (rebuildingTransactions) {
            walletState.insert(QStringLiteral("transactions"), rebuildTransactionHistoryFromOutputs(tracked));
        }
        walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("seed-rewind"));
        walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        document.insert(QStringLiteral("wallet_state"), walletState);

        if (!saveDocument(document)) {
            setLastError(QStringLiteral("Failed to persist seed scan results."));
            return;
        }

        refreshStateFromStorage();
        m_syncStatus = QStringLiteral("Seed scan complete");
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QStringLiteral("Seed scan discovered %1 owned outputs.")
                        .arg(QString::number(m_seedScanDiscovered.size())));
        m_seedScanActive = false;
        m_walletScanInFlight = false;
    });

    connect(m_nodeApi, &NodeForeignApi::getUnconfirmedTransactionsFinished, this, [this](const Result<QList<PoolEntry> > &result) {
        m_broadcastStatusRefreshInFlight = false;
        if (result.hasError()) {
            return;
        }

        QSet<QString> mempoolExcesses;
        const QList<PoolEntry> entries = result.value();
        for (int i = 0; i < entries.size(); ++i) {
            const QVector<TxKernel> kernels = entries.at(i).tx().body().kernels();
            for (int j = 0; j < kernels.size(); ++j) {
                if (!kernels.at(j).excess().isEmpty()) {
                    mempoolExcesses.insert(kernels.at(j).excess());
                }
            }
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
        m_kernelStatusQueue.clear();

        for (int i = 0; i < transactions.size(); ++i) {
            QJsonObject entry = transactions.at(i).toObject();
            if (!entry.value(QStringLiteral("broadcasted")).toBool()) {
                continue;
            }

            const QString status = entry.value(QStringLiteral("status")).toString();
            if (status == QStringLiteral("confirmed") || status == QStringLiteral("cancelled")) {
                continue;
            }

            const QString excess = kernelExcessFromEntry(entry);
            if (excess.isEmpty()) {
                continue;
            }

            entry.insert(QStringLiteral("kernel_excess"), excess);
            entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            entry.insert(QStringLiteral("status"),
                         mempoolExcesses.contains(excess)
                             ? QStringLiteral("in_mempool")
                             : QStringLiteral("broadcasted"));
            entry.insert(QStringLiteral("confirmations"), 0);
            transactions.replace(i, entry);
            m_kernelStatusQueue.append(qMakePair(entry.value(QStringLiteral("workflow_id")).toString(), excess));
        }

        walletState.insert(QStringLiteral("transactions"), transactions);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
        refreshStateFromStorage();
        startNextKernelStatusCheck();
    });

    connect(m_nodeApi, &NodeForeignApi::getKernelFinished, this, [this](const Result<LocatedTxKernel> &result) {
        m_kernelStatusCheckInFlight = false;
        if (!m_currentKernelWorkflowId.isEmpty() && !result.hasError()) {
            updateTransactionEntry(m_currentKernelWorkflowId, [this, &result](QJsonObject &entry) {
                entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
                entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(result.value().height()));
                entry.insert(QStringLiteral("confirmations"),
                             static_cast<qint64>(m_chainHeight >= result.value().height()
                                 ? (m_chainHeight - result.value().height() + 1)
                                 : 0));
                entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            });
        }

        m_currentKernelWorkflowId.clear();
        m_currentKernelExcess.clear();
        startNextKernelStatusCheck();
    });

    connect(m_nodeApi, &NodeForeignApi::pushTransactionFinished, this, [this](const Result<bool> &result) {
        const QString workflowId = m_pendingBroadcastWorkflowId;
        m_pendingBroadcastWorkflowId.clear();

        if (result.hasError() || !result.value()) {
            if (!workflowId.isEmpty()) {
                updateTransactionEntry(workflowId, [&result](QJsonObject &entry) {
                    entry.insert(QStringLiteral("status"), QStringLiteral("broadcast_failed"));
                    entry.insert(QStringLiteral("broadcasted"), false);
                    entry.insert(QStringLiteral("broadcast_error"),
                                 result.hasError()
                                     ? result.errorMessage()
                                     : QStringLiteral("Node rejected transaction broadcast."));
                    entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                });
            }
            setLastError(result.hasError()
                             ? result.errorMessage()
                             : QStringLiteral("Node rejected transaction broadcast."));
            return;
        }

        if (!workflowId.isEmpty()) {
            updateTransactionEntry(workflowId, [](QJsonObject &entry) {
                entry.insert(QStringLiteral("status"), QStringLiteral("broadcasted"));
                entry.insert(QStringLiteral("broadcasted"), true);
                entry.insert(QStringLiteral("broadcast_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                entry.remove(QStringLiteral("broadcast_error"));
            });
        }

        setLastError(QString());
        setLastInfo(QStringLiteral("Transaction broadcast submitted to node."));
        refreshBroadcastStatuses();
    });
}

void GrinWalletController::refreshStateFromStorage()
{
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QJsonObject balances = walletState.value(QStringLiteral("balances")).toObject();

    m_totalBalance = amountStringFromJson(balances, QStringLiteral("total"));
    m_spendableBalance = amountStringFromJson(balances, QStringLiteral("spendable"));
    m_lockedBalance = amountStringFromJson(balances, QStringLiteral("locked"));
    m_immatureBalance = amountStringFromJson(balances, QStringLiteral("immature"));
    m_scanHeight = static_cast<qulonglong>(walletState.value(QStringLiteral("scan_height")).toInt());
    emit statusChanged();
}

void GrinWalletController::startAutoRefresh()
{
    if (m_autoRefreshTimer) {
        return;
    }

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(30000);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &GrinWalletController::refreshNodeStatus);
    m_autoRefreshTimer->start();
}

void GrinWalletController::storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit)
{
    if (commit.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commit.commitment) {
            return;
        }
    }

    WalletOutput output;
    output.commitment = commit.commitment;
    output.proof = commit.proof;
    output.amount = amount;
    output.source = source;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    storeOwnedOutput(output);
}

void GrinWalletController::storeOwnedOutput(const WalletOutput &output)
{
    if (output.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == output.commitment) {
            outputs[i] = output;
            walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
            walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
            if (output.childIndex + 1 > nextChildIndexFromState(walletState)) {
                walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
            }
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            refreshStateFromStorage();
            return;
        }
    }

    outputs.append(output);
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    if (output.childIndex + 1 > nextChildIndexFromState(walletState)) {
        walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
    }
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

bool GrinWalletController::buildOwnedOutput(const QString &source,
                                            const QString &amount,
                                            WalletOutput *outputOut,
                                            SlateV4::Commit *commitOut,
                                            QString *errorOut) const
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output target is missing.");
        }
        return false;
    }
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet must be unlocked to derive owned outputs.");
        }
        return false;
    }

    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const quint32 childIndex = nextChildIndexFromState(walletState);
    const WalletKeychain keychain(m_sessionMnemonic);
    if (!keychain.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet keychain could not be derived.");
        }
        return false;
    }

    const WalletCryptoBackend::OwnedCommitment owned =
        WalletCryptoBackend::createOwnedCommitment(keychain, childIndex, amount);
    if (!owned.success) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet output derivation failed.");
        }
        return false;
    }

    WalletOutput output;
    output.commitment = owned.commit.commitment;
    output.proof = owned.commit.proof;
    output.amount = amount;
    output.source = source;
    output.keyPath = owned.keyPath;
    output.blindingFactor = owned.blindingFactor;
    output.childIndex = owned.childIndex;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    output.pending = false;

    *outputOut = output;
    *commitOut = owned.commit;
    return true;
}

void GrinWalletController::persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();

    QJsonObject entry;
    entry.insert(QStringLiteral("workflow_id"), slate.workflowId());
    entry.insert(QStringLiteral("mode"), slate.modeCode());
    entry.insert(QStringLiteral("state"), slate.stateCode());
    entry.insert(QStringLiteral("amount"), slate.amount);
    entry.insert(QStringLiteral("fee"), slate.fee);
    entry.insert(QStringLiteral("slate_id"), slate.id);
    entry.insert(QStringLiteral("offset"), slate.offset);
    entry.insert(QStringLiteral("broadcasted"), broadcasted);
    entry.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    entry.insert(QStringLiteral("tx_ready"), slate.metadata.value(QStringLiteral("tx_ready")).toBool());
    entry.insert(QStringLiteral("confirmations"), 0);
    entry.insert(QStringLiteral("kernel_excess"),
                 slate.metadata.value(QStringLiteral("pubkey_total")).toString());
    entry.insert(QStringLiteral("kernel_signature"),
                 slate.metadata.value(QStringLiteral("final_sig")).toString());
    entry.insert(QStringLiteral("status"),
                 broadcasted ? QStringLiteral("broadcasted")
                             : (slate.metadata.value(QStringLiteral("tx_ready")).toBool()
                                    ? QStringLiteral("ready")
                                    : QStringLiteral("in_progress")));
    if (slate.hasPaymentProof) {
        entry.insert(QStringLiteral("payment_proof"), slate.paymentProof.toJson());
        entry.insert(QStringLiteral("payment_proof_status"),
                     slate.paymentProof.receiverSignature.isEmpty()
                         ? QStringLiteral("pending")
                         : QStringLiteral("receiver_signed"));
    }
    if (slate.metadata.value(QStringLiteral("tx_skeleton")).isObject()) {
        const QJsonObject txSkeleton = slate.metadata.value(QStringLiteral("tx_skeleton")).toObject();
        entry.insert(QStringLiteral("tx_skeleton"), txSkeleton);
        const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
        if (!kernels.isEmpty()) {
            const QJsonObject kernel = kernels.first().toObject();
            if (!kernel.value(QStringLiteral("excess")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_excess"), kernel.value(QStringLiteral("excess")).toString());
            }
            if (!kernel.value(QStringLiteral("excess_sig")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_signature"), kernel.value(QStringLiteral("excess_sig")).toString());
            }
        }
    }

    bool replaced = false;
    for (int i = 0; i < transactions.size(); ++i) {
        if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == slate.workflowId()) {
            transactions.replace(i, entry);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        transactions.append(entry);
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
}

void GrinWalletController::updateTransactionEntry(const QString &workflowId, const std::function<void (QJsonObject &)> &updater)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool updated = false;
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        if (entry.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        updater(entry);
        transactions.replace(i, entry);
        updated = true;
        break;
    }

    if (!updated) {
        return;
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

QString GrinWalletController::kernelExcessFromEntry(const QJsonObject &entry) const
{
    const QString direct = entry.value(QStringLiteral("kernel_excess")).toString();
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonObject txSkeleton = entry.value(QStringLiteral("tx_skeleton")).toObject();
    const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
    if (!kernels.isEmpty()) {
        return kernels.first().toObject().value(QStringLiteral("excess")).toString();
    }

    return QString();
}

void GrinWalletController::refreshTransactionConfirmations()
{
    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool changed = false;

    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
        const QString status = entry.value(QStringLiteral("status")).toString();

        int confirmations = 0;
        if (confirmedHeight > 0 && m_chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<int>(m_chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
        }

        if (entry.value(QStringLiteral("confirmations")).toInt() != confirmations) {
            entry.insert(QStringLiteral("confirmations"), confirmations);
            changed = true;
        }

        if (confirmedHeight > 0 && confirmations > 0 && status != QStringLiteral("confirmed")) {
            entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
            changed = true;
        }

        transactions.replace(i, entry);
    }

    if (changed) {
        walletState.insert(QStringLiteral("transactions"), transactions);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
    }
}

QJsonArray GrinWalletController::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs) const
{
    QJsonArray transactions;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.commitment.isEmpty()) {
            continue;
        }

        QJsonObject entry;
        entry.insert(QStringLiteral("workflow_id"),
                     QStringLiteral("rescan-%1").arg(output.commitment.left(24)));
        entry.insert(QStringLiteral("mode"),
                     output.source == QStringLiteral("change") ? QStringLiteral("send") : QStringLiteral("receive"));
        entry.insert(QStringLiteral("state"), QStringLiteral("chain"));
        entry.insert(QStringLiteral("amount"), output.amount);
        entry.insert(QStringLiteral("fee"), QStringLiteral("unknown"));
        entry.insert(QStringLiteral("slate_id"), QString());
        entry.insert(QStringLiteral("offset"), QString());
        entry.insert(QStringLiteral("broadcasted"), true);
        entry.insert(QStringLiteral("tx_ready"), true);
        entry.insert(QStringLiteral("status"), output.spent ? QStringLiteral("spent") : QStringLiteral("confirmed"));
        entry.insert(QStringLiteral("commitment"), output.commitment);
        entry.insert(QStringLiteral("source"), output.source);
        entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(output.height));
        entry.insert(QStringLiteral("confirmations"),
                     output.height > 0 && m_chainHeight >= output.height
                         ? static_cast<qint64>(m_chainHeight - output.height + 1)
                         : 0);
        entry.insert(QStringLiteral("timestamp"), QString());
        transactions.append(entry);
    }

    return transactions;
}

void GrinWalletController::refreshBroadcastStatuses()
{
    if (!m_nodeApi || m_broadcastStatusRefreshInFlight || m_kernelStatusCheckInFlight) {
        return;
    }

    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    bool needsRefresh = false;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        if (!entry.value(QStringLiteral("broadcasted")).toBool()) {
            continue;
        }

        const QString status = entry.value(QStringLiteral("status")).toString();
        if (status != QStringLiteral("confirmed") && status != QStringLiteral("cancelled")) {
            needsRefresh = true;
            break;
        }
    }

    if (!needsRefresh) {
        return;
    }

    m_broadcastStatusRefreshInFlight = true;
    m_nodeApi->getUnconfirmedTransactionsAsync();
}

void GrinWalletController::startNextKernelStatusCheck()
{
    if (!m_nodeApi || m_kernelStatusCheckInFlight || m_kernelStatusQueue.isEmpty()) {
        return;
    }

    const QPair<QString, QString> next = m_kernelStatusQueue.takeFirst();
    m_currentKernelWorkflowId = next.first;
    m_currentKernelExcess = next.second;
    if (m_currentKernelExcess.isEmpty()) {
        startNextKernelStatusCheck();
        return;
    }

    m_kernelStatusCheckInFlight = true;
    m_nodeApi->getKernelAsync(m_currentKernelExcess, 0, static_cast<int>(m_chainHeight > 0 ? m_chainHeight + 2 : 0));
}

void GrinWalletController::finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonObject localContext = workflowContext(slate.workflowId());
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selectedCommitments.size(); ++j) {
            if (outputs[i].commitment == selectedCommitments.at(j).toString()) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = !broadcasted;
                outputs[i].locked = !broadcasted;
                outputs[i].spent = broadcasted;
                outputs[i].onChain = false;
            }
        }

        if (!changeCommit.isEmpty() && outputs[i].commitment == changeCommit) {
            outputs[i].workflowId = slate.workflowId();
            outputs[i].pending = true;
            outputs[i].locked = !broadcasted;
        }

        for (int j = 0; j < slate.commitments.size(); ++j) {
            if (outputs[i].commitment == slate.commitments.at(j).commitment) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = true;
                outputs[i].locked = !broadcasted;
            }
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

void GrinWalletController::storeWorkflowContext(const QString &workflowId, const QJsonObject &context)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.insert(workflowId, context);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    saveDocument(document);
}

QJsonObject GrinWalletController::workflowContext(const QString &workflowId) const
{
    if (workflowId.isEmpty()) {
        return QJsonObject();
    }

    const QJsonObject document = loadDocument();
    return document.value(QStringLiteral("workflow_contexts")).toObject().value(workflowId).toObject();
}

void GrinWalletController::startSeedScan()
{
    m_seedScanActive = true;
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    m_seedScanNextIndex = qMax<qulonglong>(
        1,
        walletState.value(QStringLiteral("restore_leaf_index")).toVariant().toULongLong() + 1);
    m_seedScanDiscovered.clear();
    m_syncStatus = QStringLiteral("Seed scan started at leaf %1").arg(QString::number(m_seedScanNextIndex));
    emit statusChanged();
    m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
}

void GrinWalletController::finishSeedScan(const QString &message)
{
    m_seedScanActive = false;
    if (!message.isEmpty()) {
        setLastInfo(message);
    }
}

void GrinWalletController::requestWalletScan()
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before scanning outputs."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastInfo(QStringLiteral("Wallet scan is already running."));
        return;
    }

    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (outputs.isEmpty()) {
        QJsonObject document = loadDocument();
        QJsonObject state = document.value(QStringLiteral("wallet_state")).toObject();
        state.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        state.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
        document.insert(QStringLiteral("wallet_state"), state);
        saveDocument(document);
        refreshStateFromStorage();
        setLastInfo(QStringLiteral("Wallet has no tracked outputs yet. Starting seed scan."));
    }

    m_syncStatus = QStringLiteral("Scanning wallet outputs...");
    emit statusChanged();
    m_walletScanInFlight = true;
    startSeedScan();
}
