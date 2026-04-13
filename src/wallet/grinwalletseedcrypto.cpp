#include "grinwalletseedcrypto.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include "../3rdparty/monocypher/monocypher.h"
#include "walletsecurerandom.h"

namespace {

const int kMnemonicEntropyBytes = 32;
const int kSeedCipherVersion = 4;
const int kSeedCipherArgon2Blocks = 32768;
const int kSeedCipherArgon2Passes = 4;
const int kSeedCipherArgon2Lanes = 1;
const int kSeedCipherKeyBytes = 32;
const int kSeedCipherMacBytes = 16;

void wipeByteArray(QByteArray *value)
{
    if (!value) {
        return;
    }
    if (!value->isEmpty()) {
        crypto_wipe(value->data(), static_cast<size_t>(value->size()));
    }
    value->clear();
    value->squeeze();
}

/**
 * @brief Generates bytes.
 * @param size
 * @return
 */
QByteArray randomBytes(int size)
{
    return WalletSecureRandom::bytes(size);
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
 * @brief Builds key material for the current seed cipher format.
 * @param password
 * @param salt
 * @param blocks
 * @param passes
 * @param keyOut
 * @return
 */
bool deriveKeyMaterial(const QString &password,
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
    QByteArray passwordBytes = password.toUtf8();
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
    wipeByteArray(&passwordBytes);
    wipeByteArray(&workArea);
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
    QByteArray salt = randomBytes(16);
    QByteArray nonce = randomBytes(24);
    QByteArray plain = normalizeMnemonic(mnemonic).toUtf8();
    QByteArray keyMaterial;
    if (!deriveKeyMaterial(password,
                           salt,
                           kSeedCipherArgon2Blocks,
                           kSeedCipherArgon2Passes,
                           &keyMaterial)) {
        wipeByteArray(&plain);
        wipeByteArray(&nonce);
        wipeByteArray(&salt);
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
    wipeByteArray(&keyMaterial);
    wipeByteArray(&mac);
    wipeByteArray(&cipher);
    wipeByteArray(&plain);
    wipeByteArray(&nonce);
    wipeByteArray(&salt);
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
    if (version != kSeedCipherVersion
        || encrypted.value(QStringLiteral("kdf_algorithm")).toString() != QStringLiteral("argon2id")) {
        return false;
    }

    QByteArray salt = QByteArray::fromBase64(encrypted.value(QStringLiteral("salt")).toString().toUtf8());
    QByteArray nonce = QByteArray::fromBase64(encrypted.value(QStringLiteral("nonce")).toString().toUtf8());
    QByteArray cipher = QByteArray::fromBase64(encrypted.value(QStringLiteral("cipher")).toString().toUtf8());
    QByteArray mac = QByteArray::fromBase64(encrypted.value(QStringLiteral("mac")).toString().toUtf8());
    const int blocks = std::max(8, encrypted.value(QStringLiteral("kdf_blocks")).toInt(kSeedCipherArgon2Blocks));
    const int passes = std::max(1, encrypted.value(QStringLiteral("kdf_passes")).toInt(kSeedCipherArgon2Passes));
    QByteArray keyMaterial;
    if (!deriveKeyMaterial(password, salt, blocks, passes, &keyMaterial)) {
        wipeByteArray(&mac);
        wipeByteArray(&cipher);
        wipeByteArray(&nonce);
        wipeByteArray(&salt);
        return false;
    }

    if (nonce.size() != 24 || mac.size() != kSeedCipherMacBytes) {
        crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
        wipeByteArray(&keyMaterial);
        wipeByteArray(&mac);
        wipeByteArray(&cipher);
        wipeByteArray(&nonce);
        wipeByteArray(&salt);
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
    wipeByteArray(&keyMaterial);
    if (unlockResult != 0) {
        crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
        wipeByteArray(&plain);
        wipeByteArray(&mac);
        wipeByteArray(&cipher);
        wipeByteArray(&nonce);
        wipeByteArray(&salt);
        return false;
    }

    const QString mnemonic = normalizeMnemonic(QString::fromUtf8(plain));
    crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
    wipeByteArray(&plain);
    if (!isValidMnemonic(mnemonic)) {
        wipeByteArray(&mac);
        wipeByteArray(&cipher);
        wipeByteArray(&nonce);
        wipeByteArray(&salt);
        return false;
    }
    if (mnemonicOut) {
        *mnemonicOut = mnemonic;
        mnemonicOut->detach();
    }
    wipeByteArray(&mac);
    wipeByteArray(&cipher);
    wipeByteArray(&nonce);
    wipeByteArray(&salt);
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
