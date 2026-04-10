#include "grinwalletseedcrypto.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include "../3rdparty/monocypher/monocypher.h"

namespace {

const int kMnemonicEntropyBytes = 32;
const int kSeedCipherVersion = 3;
const int kSeedCipherArgon2Blocks = 256;
const int kSeedCipherArgon2Passes = 3;
const int kSeedCipherArgon2Lanes = 1;
const int kSeedCipherKeyBytes = 32;
const int kSeedCipherMacBytes = 16;

/**
 * @brief Generates bytes.
 * @param size
 * @return
 */
QByteArray randomBytes(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return data;
}

/**
 * @brief Processes mnemonic words.
 * @return
 */
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

/**
 * @brief Processes bits from bytes.
 * @param bytes
 * @return
 */
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

/**
 * @brief Processes bytes from bits.
 * @param bits
 * @return
 */
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

/**
 * @brief Processes mnemonic from entropy.
 * @param entropy
 * @return
 */
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

/**
 * @brief Returns whether entropy from mnemonic.
 * @param mnemonic
 * @param entropyOut
 * @return
 */
bool entropyFromMnemonic(const QString &mnemonic, QByteArray *entropyOut)
{
    const QString normalized = GrinWalletSeedCrypto::normalizeMnemonic(mnemonic);
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

/**
 * @brief Builds legacy key material.
 * @param password
 * @param salt
 * @return
 */
QByteArray deriveLegacyKeyMaterial(const QString &password, const QByteArray &salt)
{
    const QByteArray material = password.toUtf8() + salt;
    QByteArray digest = QCryptographicHash::hash(material, QCryptographicHash::Sha256);
    for (int i = 0; i < 120000; ++i) {
        digest = QCryptographicHash::hash(digest + material, QCryptographicHash::Sha256);
    }
    return digest;
}

/**
 * @brief Processes xor stream.
 * @param data
 * @param key
 * @param nonce
 * @return
 */
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

/**
 * @brief Builds key material v2.
 * @param password
 * @param salt
 * @param iterations
 * @param outputLength
 * @return
 */
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

/**
 * @brief Builds key material v3.
 * @param password
 * @param salt
 * @param blocks
 * @param passes
 * @param keyOut
 * @return
 */
bool deriveKeyMaterialV3(const QString &password,
                         const QByteArray &salt,
                         int blocks,
                         int passes,
                         QByteArray *keyOut)
{
    if (!keyOut || salt.isEmpty() || blocks < 8 || passes < 1) {
        return false;
    }

    QByteArray keyMaterial(kSeedCipherKeyBytes, Qt::Uninitialized);
    QByteArray workArea(blocks * 1024, Qt::Uninitialized);
    const QByteArray passwordBytes = password.toUtf8();
    static const QByteArray additionalData("grinffindor.seed.v3", 19);

    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = static_cast<uint32_t>(blocks);
    config.nb_passes = static_cast<uint32_t>(passes);
    config.nb_lanes = kSeedCipherArgon2Lanes;

    crypto_argon2_inputs inputs;
    inputs.pass = reinterpret_cast<const uint8_t *>(passwordBytes.constData());
    inputs.salt = reinterpret_cast<const uint8_t *>(salt.constData());
    inputs.pass_size = static_cast<uint32_t>(passwordBytes.size());
    inputs.salt_size = static_cast<uint32_t>(salt.size());

    crypto_argon2_extras extras = crypto_argon2_no_extras;
    extras.ad = reinterpret_cast<const uint8_t *>(additionalData.constData());
    extras.ad_size = static_cast<uint32_t>(additionalData.size());

    crypto_argon2(reinterpret_cast<uint8_t *>(keyMaterial.data()),
                  static_cast<uint32_t>(keyMaterial.size()),
                  workArea.data(),
                  config,
                  inputs,
                  extras);

    crypto_wipe(workArea.data(), static_cast<size_t>(workArea.size()));
    *keyOut = keyMaterial;
    crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
    return true;
}

} // namespace

/**
 * @brief GrinWalletSeedCrypto::generateMnemonic
 * @return
 */
QString GrinWalletSeedCrypto::generateMnemonic()
{
    return mnemonicFromEntropy(randomBytes(kMnemonicEntropyBytes));
}

/**
 * @brief GrinWalletSeedCrypto::normalizeMnemonic
 * @param mnemonic
 * @return
 */
QString GrinWalletSeedCrypto::normalizeMnemonic(const QString &mnemonic)
{
    QString normalized = mnemonic.toLower().trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

/**
 * @brief GrinWalletSeedCrypto::isValidMnemonic
 * @param mnemonic
 * @return
 */
bool GrinWalletSeedCrypto::isValidMnemonic(const QString &mnemonic)
{
    return entropyFromMnemonic(mnemonic, 0);
}

/**
 * @brief GrinWalletSeedCrypto::encryptMnemonic
 * @param mnemonic
 * @param password
 * @return
 */
QJsonObject GrinWalletSeedCrypto::encryptMnemonic(const QString &mnemonic, const QString &password)
{
    const QByteArray salt = randomBytes(16);
    const QByteArray nonce = randomBytes(24);
    const QByteArray plain = normalizeMnemonic(mnemonic).toUtf8();
    QByteArray keyMaterial;
    if (!deriveKeyMaterialV3(password,
                             salt,
                             kSeedCipherArgon2Blocks,
                             kSeedCipherArgon2Passes,
                             &keyMaterial)) {
        return QJsonObject();
    }

    QByteArray cipher(plain.size(), Qt::Uninitialized);
    QByteArray mac(kSeedCipherMacBytes, Qt::Uninitialized);
    static const QByteArray associatedData("grinffindor.seed-store", 22);
    crypto_aead_lock(reinterpret_cast<uint8_t *>(cipher.data()),
                     reinterpret_cast<uint8_t *>(mac.data()),
                     reinterpret_cast<const uint8_t *>(keyMaterial.constData()),
                     reinterpret_cast<const uint8_t *>(nonce.constData()),
                     reinterpret_cast<const uint8_t *>(associatedData.constData()),
                     static_cast<size_t>(associatedData.size()),
                     reinterpret_cast<const uint8_t *>(plain.constData()),
                     static_cast<size_t>(plain.size()));

    QJsonObject encrypted;
    encrypted.insert(QStringLiteral("version"), kSeedCipherVersion);
    encrypted.insert(QStringLiteral("kdf_algorithm"), QStringLiteral("argon2id"));
    encrypted.insert(QStringLiteral("kdf_blocks"), kSeedCipherArgon2Blocks);
    encrypted.insert(QStringLiteral("kdf_passes"), kSeedCipherArgon2Passes);
    encrypted.insert(QStringLiteral("kdf_lanes"), kSeedCipherArgon2Lanes);
    encrypted.insert(QStringLiteral("salt"), QString::fromUtf8(salt.toBase64()));
    encrypted.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
    encrypted.insert(QStringLiteral("cipher"), QString::fromUtf8(cipher.toBase64()));
    encrypted.insert(QStringLiteral("mac"), QString::fromUtf8(mac.toBase64()));
    crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
    return encrypted;
}

/**
 * @brief GrinWalletSeedCrypto::decryptMnemonic
 * @param encrypted
 * @param password
 * @param mnemonicOut
 * @return
 */
bool GrinWalletSeedCrypto::decryptMnemonic(const QJsonObject &encrypted,
                                           const QString &password,
                                           QString *mnemonicOut)
{
    const int version = encrypted.value(QStringLiteral("version")).toInt(1);
    const QByteArray salt = QByteArray::fromBase64(encrypted.value(QStringLiteral("salt")).toString().toUtf8());
    const QByteArray nonce = QByteArray::fromBase64(encrypted.value(QStringLiteral("nonce")).toString().toUtf8());
    const QByteArray cipher = QByteArray::fromBase64(encrypted.value(QStringLiteral("cipher")).toString().toUtf8());

    const QByteArray mac = QByteArray::fromBase64(encrypted.value(QStringLiteral("mac")).toString().toUtf8());

    if (version >= kSeedCipherVersion) {
        const int blocks = std::max(8, encrypted.value(QStringLiteral("kdf_blocks")).toInt(kSeedCipherArgon2Blocks));
        const int passes = std::max(1, encrypted.value(QStringLiteral("kdf_passes")).toInt(kSeedCipherArgon2Passes));
        QByteArray keyMaterial;
        if (!deriveKeyMaterialV3(password, salt, blocks, passes, &keyMaterial)) {
            return false;
        }

        if (nonce.size() != 24 || mac.size() != kSeedCipherMacBytes) {
            crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
            return false;
        }

        QByteArray plain(cipher.size(), Qt::Uninitialized);
        static const QByteArray associatedData("grinffindor.seed-store", 22);
        const int unlockResult =
            crypto_aead_unlock(reinterpret_cast<uint8_t *>(plain.data()),
                               reinterpret_cast<const uint8_t *>(mac.constData()),
                               reinterpret_cast<const uint8_t *>(keyMaterial.constData()),
                               reinterpret_cast<const uint8_t *>(nonce.constData()),
                               reinterpret_cast<const uint8_t *>(associatedData.constData()),
                               static_cast<size_t>(associatedData.size()),
                               reinterpret_cast<const uint8_t *>(cipher.constData()),
                               static_cast<size_t>(cipher.size()));
        crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
        if (unlockResult != 0) {
            crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
            return false;
        }

        const QString mnemonic = normalizeMnemonic(QString::fromUtf8(plain));
        crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
        if (!isValidMnemonic(mnemonic)) {
            return false;
        }
        if (mnemonicOut) {
            *mnemonicOut = mnemonic;
        }
        return true;
    }

    QByteArray encryptionKey;
    QByteArray macKey;
    if (version == 2) {
        const int iterations = std::max(1, encrypted.value(QStringLiteral("kdf_iterations")).toInt(240000));
        const QByteArray keyMaterial = deriveKeyMaterialV2(password, salt, iterations, 64);
        encryptionKey = keyMaterial.left(32);
        macKey = keyMaterial.mid(32, 32);
    } else {
        const QByteArray legacyKey = deriveLegacyKeyMaterial(password, salt);
        encryptionKey = legacyKey;
        macKey = legacyKey;
    }

    const QByteArray expectedMac =

        QCryptographicHash::hash(macKey + nonce + cipher + macKey, QCryptographicHash::Sha256);
    if (expectedMac != mac) {
        return false;
    }

    const QString mnemonic = normalizeMnemonic(QString::fromUtf8(xorStream(cipher, encryptionKey, nonce)));
    if (!isValidMnemonic(mnemonic)) {
        return false;
    }
    if (mnemonicOut) {
        *mnemonicOut = mnemonic;
    }
    return true;
}

/**
 * @brief GrinWalletSeedCrypto::seedFingerprint
 * @param mnemonic
 * @return
 */
QString GrinWalletSeedCrypto::seedFingerprint(const QString &mnemonic)
{
    return QString::fromUtf8(
        QCryptographicHash::hash(normalizeMnemonic(mnemonic).toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}
