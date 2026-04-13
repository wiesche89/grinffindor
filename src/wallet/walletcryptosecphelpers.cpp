#include "walletcryptohelpers.h"

#include <QStringList>
#include <cstring>

#include "walletsecurerandom.h"

namespace
{

const size_t kBulletproofGeneratorCount = 256;

class SecpContextHolder
{
public:
    SecpContextHolder()
    {
        m_context = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
        if (m_context) {
            const QByteArray seedBytes = WalletSecureRandom::bytes(32);
            unsigned char seed[32];
            std::memset(seed, 0, sizeof(seed));
            if (seedBytes.size() == 32) {
                std::memcpy(seed, seedBytes.constData(), sizeof(seed));
            }
            const int randomized = secp256k1_context_randomize(m_context, seed);
            Q_UNUSED(randomized);
            m_generators = secp256k1_bulletproof_generators_create(
                m_context,
                &secp256k1_generator_const_g,
                kBulletproofGeneratorCount);
        }
    }

    ~SecpContextHolder()
    {
        if (m_generators) {
            secp256k1_bulletproof_generators_destroy(m_context, m_generators);
        }
        if (m_context) {
            secp256k1_context_destroy(m_context);
        }
    }

    secp256k1_context *context() const
    {
        return m_context;
    }

    secp256k1_bulletproof_generators *generators() const
    {
        return m_generators;
    }

private:
    secp256k1_context *m_context = 0;
    secp256k1_bulletproof_generators *m_generators = 0;
};

/**
 * @brief Processes wallet secp holder.
 * @return
 */
SecpContextHolder &walletSecpHolder()
{
    static SecpContextHolder holder;
    return holder;
}

}

namespace WalletCryptoHelpers
{

/**
 * @brief Processes wallet secp context.
 * @return
 */
secp256k1_context *walletSecpContext()
{
    return walletSecpHolder().context();
}

/**
 * @brief Processes wallet bulletproof generators.
 * @return
 */
secp256k1_bulletproof_generators *walletBulletproofGenerators()
{
    return walletSecpHolder().generators();
}

/**
 * @brief Builds valid secret bytes.
 * @param domain
 * @param left
 * @param right
 * @return
 */
QByteArray deriveValidSecretBytes(const QString &domain, const QString &left, const QString &right)
{
    secp256k1_context *context = walletSecpContext();
    for (int counter = 0; counter < 1024; ++counter) {
        QByteArray candidate = hashBytes(QStringLiteral("%1:%2:%3:%4")
                                             .arg(domain, left, right, QString::number(counter))
                                             .toUtf8());
        if (candidate.size() == 32
            && context
            && secp256k1_ec_seckey_verify(context, reinterpret_cast<const unsigned char *>(candidate.constData())) == 1) {
            return candidate;
        }
    }

    QByteArray fallback(32, Qt::Uninitialized);
    do {
        if (!WalletSecureRandom::fill(&fallback)) {
            return QByteArray();
        }
    } while (!context
             || secp256k1_ec_seckey_verify(context, reinterpret_cast<const unsigned char *>(fallback.constData())) != 1);
    return fallback;
}

/**
 * @brief Builds signing base secret.
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @return
 */
QByteArray deriveSigningBaseSecret(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag)
{
    return deriveValidSecretBytes(QStringLiteral("blind-base"),
                                  walletFingerprint,
                                  workflowId + QLatin1Char(':') + roleTag);
}

/**
 * @brief Builds aggsig secnonce.
 * @param walletFingerprint
 * @param workflowId
 * @param roleTag
 * @return
 */
QByteArray deriveAggsigSecnonce(const QString &walletFingerprint,
                                const QString &workflowId,
                                const QString &roleTag,
                                const QString &nonceEntropy)
{
    const QByteArray seed = hashBytes(
        QStringLiteral("nonce-seed:%1:%2:%3:%4")
            .arg(walletFingerprint, workflowId, roleTag, nonceEntropy.trimmed())
            .toUtf8());
    if (seed.size() != 32) {
        return QByteArray();
    }

    unsigned char secnonce[32];
    if (secp256k1_aggsig_export_secnonce_single(
            walletSecpContext(),
            secnonce,
            reinterpret_cast<const unsigned char *>(seed.constData())) != 1) {
        return QByteArray();
    }
    return QByteArray(reinterpret_cast<const char *>(secnonce), sizeof(secnonce));
}

/**
 * @brief Builds compressed pubkey hex.
 * @param secretKey
 * @return
 */
QString createCompressedPubkeyHex(const QByteArray &secretKey)
{
    secp256k1_context *context = walletSecpContext();
    if (!context || secretKey.size() != 32) {
        return QString();
    }

    secp256k1_pubkey pubkey;
    if (secp256k1_ec_pubkey_create(context, &pubkey,
                                   reinterpret_cast<const unsigned char *>(secretKey.constData())) != 1) {
        return QString();
    }

    unsigned char serialized[33];
    size_t serializedSize = sizeof(serialized);
    if (secp256k1_ec_pubkey_serialize(context, serialized, &serializedSize, &pubkey, SECP256K1_EC_COMPRESSED) != 1) {
        return QString();
    }

    return toHex(serialized, static_cast<int>(serializedSize));
}

/**
 * @brief Parses pubkey.
 * @param hex
 * @param pubkey
 * @return
 */
bool parsePubkey(const QString &hex, secp256k1_pubkey *pubkey)
{
    const QByteArray bytes = fromHex(hex);
    if (bytes.size() != 33) {
        return false;
    }
    return secp256k1_ec_pubkey_parse(walletSecpContext(),
                                     pubkey,
                                     reinterpret_cast<const unsigned char *>(bytes.constData()),
                                     static_cast<size_t>(bytes.size())) == 1;
}

/**
 * @brief Builds pubkey.
 * @param pubkey
 * @return
 */
QString serializePubkey(const secp256k1_pubkey &pubkey)
{
    unsigned char serialized[33];
    size_t serializedSize = sizeof(serialized);
    if (secp256k1_ec_pubkey_serialize(walletSecpContext(),
                                      serialized,
                                      &serializedSize,
                                      &pubkey,
                                      SECP256K1_EC_COMPRESSED) != 1) {
        return QString();
    }
    return toHex(serialized, static_cast<int>(serializedSize));
}

/**
 * @brief Returns whether combine pubkeys.
 * @param hexPubkeys
 * @param combined
 * @return
 */
bool combinePubkeys(const QList<QString> &hexPubkeys, secp256k1_pubkey *combined)
{
    QVector<secp256k1_pubkey> parsed;
    QVector<const secp256k1_pubkey *> ptrs;
    for (int i = 0; i < hexPubkeys.size(); ++i) {
        secp256k1_pubkey pubkey;
        if (!parsePubkey(hexPubkeys.at(i), &pubkey)) {
            return false;
        }
        parsed.append(pubkey);
    }
    for (int i = 0; i < parsed.size(); ++i) {
        ptrs.append(&parsed[i]);
    }
    return !ptrs.isEmpty()
        && secp256k1_ec_pubkey_combine(walletSecpContext(),
                                       combined,
                                       ptrs.constData(),
                                       static_cast<size_t>(ptrs.size())) == 1;
}

/**
 * @brief Processes amount to nanogrin.
 * @param amount
 * @return
 */
quint64 amountToNanogrin(const QString &amount)
{
    const QString simplified = amount.trimmed();
    if (simplified.isEmpty()) {
        return 0;
    }

    const QStringList parts = simplified.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = (parts.size() == 2) ? parts.at(1) : QString();
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

/**
 * @brief Returns whether add scalars.
 * @param left
 * @param right
 * @param sumOut
 * @return
 */
bool addScalars(const QByteArray &left, const QByteArray &right, QByteArray *sumOut)
{
    if (!sumOut || left.size() != 32 || right.size() != 32) {
        return false;
    }
    QByteArray sum = left;
    if (secp256k1_ec_privkey_tweak_add(walletSecpContext(),
                                       reinterpret_cast<unsigned char *>(sum.data()),
                                       reinterpret_cast<const unsigned char *>(right.constData())) != 1) {
        return false;
    }
    *sumOut = sum;
    return true;
}

/**
 * @brief Returns whether subtract scalars.
 * @param left
 * @param right
 * @param differenceOut
 * @return
 */
bool subtractScalars(const QByteArray &left, const QByteArray &right, QByteArray *differenceOut)
{
    if (!differenceOut || left.size() != 32 || right.size() != 32) {
        return false;
    }
    QByteArray negated = right;
    if (secp256k1_ec_privkey_negate(walletSecpContext(),
                                    reinterpret_cast<unsigned char *>(negated.data())) != 1) {
        return false;
    }
    return addScalars(left, negated, differenceOut);
}

}
