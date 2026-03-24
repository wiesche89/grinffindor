#include "walletkeychain.h"
#include "walletblake2b.h"

#include <QCryptographicHash>
#include <QFile>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QVector>

extern "C" {
#include "secp256k1.h"
#include "secp256k1_bulletproofs.h"
#include "secp256k1_commitment.h"
}

namespace {

const size_t kBulletproofGeneratorCount = 256;
const size_t kBulletproofScratchSpaceSize = 8 * 1024 * 1024;

class KeychainSecpHolder
{
public:
    KeychainSecpHolder()
    {
        m_context = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        if (m_context) {
            m_generators = secp256k1_bulletproof_generators_create(
                m_context,
                &secp256k1_generator_const_g,
                kBulletproofGeneratorCount);
        }
    }

    ~KeychainSecpHolder()
    {
        if (m_generators) {
            secp256k1_bulletproof_generators_destroy(m_context, m_generators);
        }
        if (m_context) {
            secp256k1_context_destroy(m_context);
        }
    }

    secp256k1_context *context() const { return m_context; }

private:
    secp256k1_context *m_context = 0;
    secp256k1_bulletproof_generators *m_generators = 0;
};

static const secp256k1_pubkey kGeneratorJPub = {{
    0x5f, 0x15, 0x21, 0x36, 0x93, 0x93, 0x01, 0x2a,
    0x8d, 0x8b, 0x39, 0x7e, 0x9b, 0xf4, 0x54, 0x29,
    0x2f, 0x5a, 0x1b, 0x3d, 0x38, 0x85, 0x16, 0xc2,
    0xf3, 0x03, 0xfc, 0x95, 0x67, 0xf5, 0x60, 0xb8,
    0x3a, 0xc4, 0xc5, 0xa6, 0xdc, 0xa2, 0x01, 0x59,
    0xfc, 0x56, 0xcf, 0x74, 0x9a, 0xa6, 0xa5, 0x65,
    0x31, 0x6a, 0xa5, 0x03, 0x74, 0x42, 0x3f, 0x42,
    0x53, 0x8f, 0xaa, 0x2c, 0xd3, 0x09, 0x3f, 0xa4
}};

KeychainSecpHolder &walletKeychainSecp()
{
    static KeychainSecpHolder holder;
    return holder;
}

QByteArray hash256(const QByteArray &input)
{
    return QCryptographicHash::hash(input, QCryptographicHash::Blake2b_256);
}

QString normalizeMnemonic(const QString &mnemonic)
{
    QString normalized = mnemonic.toLower().trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

QStringList mnemonicWords()
{
    QStringList words;
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

QByteArray entropyFromMnemonic(const QString &mnemonic)
{
    const QStringList parts = normalizeMnemonic(mnemonic).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList words = mnemonicWords();
    if (parts.size() != 24 || words.size() != 2048) {
        return QByteArray();
    }

    QByteArray bits;
    for (int i = 0; i < parts.size(); ++i) {
        const int index = words.indexOf(parts.at(i));
        if (index < 0) {
            return QByteArray();
        }
        for (int bit = 10; bit >= 0; --bit) {
            bits.append((index & (1 << bit)) ? '\x01' : '\x00');
        }
    }

    const int checksumLength = bits.size() / 33;
    const int entropyLength = bits.size() - checksumLength;
    const QByteArray entropy = bytesFromBits(bits.left(entropyLength));
    if (entropy.size() != 32) {
        return QByteArray();
    }

    const QByteArray checksumBits = bitsFromBytes(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256));
    if (bits.mid(entropyLength, checksumLength) != checksumBits.left(checksumLength)) {
        return QByteArray();
    }

    return entropy;
}

QByteArray createCompressedPubkey(const QByteArray &secretKey)
{
    secp256k1_context *context = walletKeychainSecp().context();
    if (!context || secretKey.size() != 32) {
        return QByteArray();
    }

    secp256k1_pubkey pubkey;
    if (secp256k1_ec_pubkey_create(context,
                                   &pubkey,
                                   reinterpret_cast<const unsigned char *>(secretKey.constData())) != 1) {
        return QByteArray();
    }

    unsigned char serialized[33];
    size_t serializedSize = sizeof(serialized);
    if (secp256k1_ec_pubkey_serialize(context,
                                      serialized,
                                      &serializedSize,
                                      &pubkey,
                                      SECP256K1_EC_COMPRESSED) != 1) {
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(serialized), static_cast<int>(serializedSize));
}

QByteArray createNonce(const QByteArray &commitment, const QByteArray &nonceHash)
{
    return WalletBlake2b::hash256(commitment, nonceHash);
}

QByteArray blindSwitch(const QByteArray &blind, quint64 amount)
{
    secp256k1_context *context = walletKeychainSecp().context();
    if (!context || blind.size() != 32) {
        return QByteArray();
    }

    unsigned char blindSwitchOut[32];
    const int result = secp256k1_blind_switch(
        context,
        blindSwitchOut,
        reinterpret_cast<const unsigned char *>(blind.constData()),
        amount,
        &secp256k1_generator_const_h,
        &secp256k1_generator_const_g,
        &kGeneratorJPub);

    if (result != 1) {
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(blindSwitchOut), 32);
}

void appendU32(QByteArray &buffer, quint32 value)
{
    buffer.append(static_cast<char>((value >> 24) & 0xff));
    buffer.append(static_cast<char>((value >> 16) & 0xff));
    buffer.append(static_cast<char>((value >> 8) & 0xff));
    buffer.append(static_cast<char>(value & 0xff));
}

quint32 readU32(const unsigned char *data)
{
    return (static_cast<quint32>(data[0]) << 24)
         | (static_cast<quint32>(data[1]) << 16)
         | (static_cast<quint32>(data[2]) << 8)
         | static_cast<quint32>(data[3]);
}

QString formatKeyPath(const QVector<quint32> &path)
{
    QString result = QStringLiteral("m");
    for (int i = 0; i < path.size(); ++i) {
        result += QStringLiteral("/%1").arg(QString::number(path.at(i)));
    }
    return result;
}

bool deriveChildPrivateKey(const QByteArray &parentSecret,
                           const QByteArray &parentChainCode,
                           quint32 childIndex,
                           QByteArray *childSecretOut,
                           QByteArray *childChainCodeOut)
{
    secp256k1_context *context = walletKeychainSecp().context();
    if (!context
        || parentSecret.size() != 32
        || parentChainCode.size() != 32
        || !childSecretOut
        || !childChainCodeOut) {
        return false;
    }

    const QByteArray parentCompressedPubkey = createCompressedPubkey(parentSecret);
    if (parentCompressedPubkey.size() != 33) {
        return false;
    }

    QByteArray serialized;
    serialized.reserve(37);
    serialized.append(parentCompressedPubkey);
    appendU32(serialized, childIndex);

    const QByteArray hmac = QMessageAuthenticationCode::hash(
        serialized,
        parentChainCode,
        QCryptographicHash::Sha512);
    if (hmac.size() != 64) {
        return false;
    }

    QByteArray childSecret = parentSecret;
    const QByteArray left = hmac.left(32);
    if (secp256k1_ec_privkey_tweak_add(
            context,
            reinterpret_cast<unsigned char *>(childSecret.data()),
            reinterpret_cast<const unsigned char *>(left.constData())) != 1) {
        return false;
    }

    *childSecretOut = childSecret;
    *childChainCodeOut = hmac.mid(32, 32);
    return true;
}

QByteArray buildEnhancedProofMessage(const QVector<quint32> &path)
{
    QByteArray message(20, '\0');
    message[2] = 1;
    message[3] = static_cast<char>(qMin(path.size(), 4));
    QByteArray serialized;
    serialized.reserve(path.size() * 4);
    for (int i = 0; i < path.size() && i < 4; ++i) {
        appendU32(serialized, path.at(i));
    }
    const int copyLength = qMin(serialized.size(), 16);
    std::memcpy(message.data() + 4, serialized.constData(), static_cast<size_t>(copyLength));
    return message;
}

}

WalletKeychain::WalletKeychain(const QString &mnemonic)
    : m_mnemonic(mnemonic.trimmed())
{
    if (m_mnemonic.isEmpty()) {
        return;
    }

    const QByteArray entropy = entropyFromMnemonic(m_mnemonic);
    if (entropy.size() != 32) {
        return;
    }

    const QByteArray hmac = QMessageAuthenticationCode::hash(
        entropy,
        QByteArrayLiteral("IamVoldemort"),
        QCryptographicHash::Sha512);
    if (hmac.size() != 64) {
        return;
    }

    m_masterSecret = hmac.left(32);
    m_masterChainCode = hmac.mid(32, 32);
    m_masterPublicKey = createCompressedPubkey(m_masterSecret);
    if (!m_masterPublicKey.isEmpty()) {
        m_rewindNonceHash = hash256(m_masterPublicKey);
        m_bulletProofNonce = blindSwitch(m_masterSecret, 0);
    }
}

bool WalletKeychain::isValid() const
{
    return m_masterSecret.size() == 32
        && m_masterChainCode.size() == 32
        && !m_masterPublicKey.isEmpty()
        && m_rewindNonceHash.size() == 32;
}

QString WalletKeychain::masterPublicKey() const
{
    return QString::fromUtf8(m_masterPublicKey.toHex());
}

WalletKeychain::RewindResult WalletKeychain::rewindOutputProof(const QString &commitment, const QString &proof) const
{
    RewindResult result;
    if (!isValid()) {
        return result;
    }

    const QByteArray commitmentBytes = QByteArray::fromHex(commitment.toUtf8());
    const QByteArray proofBytes = QByteArray::fromHex(proof.toUtf8());
    if (commitmentBytes.size() != 33 || proofBytes.isEmpty()) {
        return result;
    }

    secp256k1_context *context = walletKeychainSecp().context();
    secp256k1_pedersen_commitment pedersenCommitment;
    if (secp256k1_pedersen_commitment_parse(
            context,
            &pedersenCommitment,
            reinterpret_cast<const unsigned char *>(commitmentBytes.constData())) != 1) {
        return result;
    }

    const QByteArray nonce = createNonce(commitmentBytes, m_rewindNonceHash);
    uint64_t value = 0;
    unsigned char blind[32];
    unsigned char message[20];

    int rewound = secp256k1_bulletproof_rangeproof_rewind(
        context,
        &value,
        blind,
        reinterpret_cast<const unsigned char *>(proofBytes.constData()),
        static_cast<size_t>(proofBytes.size()),
        0,
        &pedersenCommitment,
        &secp256k1_generator_const_h,
        reinterpret_cast<const unsigned char *>(nonce.constData()),
        0,
        0,
        message);

    if (rewound != 1) {
        const QByteArray legacyNonce = createNonce(commitmentBytes, m_bulletProofNonce);
        rewound = secp256k1_bulletproof_rangeproof_rewind(
            context,
            &value,
            blind,
            reinterpret_cast<const unsigned char *>(proofBytes.constData()),
            static_cast<size_t>(proofBytes.size()),
            0,
            &pedersenCommitment,
            &secp256k1_generator_const_h,
            reinterpret_cast<const unsigned char *>(legacyNonce.constData()),
            0,
            0,
            message);
    }

    if (rewound != 1) {
        return result;
    }

    result.success = true;
    result.amount = value;
    result.blindingFactor = QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(blind), 32).toHex());
    const unsigned char *messageBytes = reinterpret_cast<const unsigned char *>(message);
    QVector<quint32> path;
    if (messageBytes[2] == 1 && messageBytes[3] > 0 && messageBytes[3] <= 4) {
        const quint32 pathLength = messageBytes[3];
        for (quint32 i = 0; i < pathLength; ++i) {
            path.append(readU32(messageBytes + 4 + (i * 4)));
        }
    } else if (readU32(messageBytes) == 0) {
        for (int i = 0; i < 3; ++i) {
            path.append(readU32(messageBytes + 4 + (i * 4)));
        }
    }
    if (!path.isEmpty()) {
        result.keyPath = formatKeyPath(path);
        result.childIndex = path.last();
    }
    return result;
}

WalletKeychain::OutputSecrets WalletKeychain::deriveOutputSecrets(quint32 childIndex, quint64 amount) const
{
    OutputSecrets secrets;
    if (!isValid()) {
        return secrets;
    }

    QVector<quint32> path;
    path << 0u << 0u << childIndex;

    QByteArray secret = m_masterSecret;
    QByteArray chainCode = m_masterChainCode;
    for (int i = 0; i < path.size(); ++i) {
        QByteArray childSecret;
        QByteArray childChainCode;
        if (!deriveChildPrivateKey(secret, chainCode, path.at(i), &childSecret, &childChainCode)) {
            return secrets;
        }
        secret = childSecret;
        chainCode = childChainCode;
    }

    secrets.success = true;
    secrets.childIndex = childIndex;
    secrets.keyPath = formatKeyPath(path);
    secrets.blindingFactor = blindSwitch(secret, amount);
    secrets.privateNonceHash = WalletBlake2b::hash256(m_masterSecret);
    secrets.rewindNonceHash = m_rewindNonceHash;
    secrets.proofMessage = buildEnhancedProofMessage(path);
    if (secrets.blindingFactor.size() != 32 || secrets.proofMessage.size() != 20) {
        secrets.success = false;
    }
    return secrets;
}
