#ifndef WALLETKEYCHAIN_H
#define WALLETKEYCHAIN_H

#include <QByteArray>
#include <QString>

class WalletKeychain
{
public:
    struct RewindResult {
        bool success = false;
        quint64 amount = 0;
        QString blindingFactor;
        QString keyPath;
        quint32 childIndex = 0;
    };

    struct OutputSecrets {
        bool success = false;
        QString keyPath;
        quint32 childIndex = 0;
        QByteArray blindingFactor;
        QByteArray privateNonceHash;
        QByteArray rewindNonceHash;
        QByteArray proofMessage;
    };

/**
 * @brief Returns wallet keychain.
 */
    explicit WalletKeychain(const QString &mnemonic = QString());

/**
 * @brief Returns whether valid.
 */
    bool isValid() const;
/**
 * @brief Returns master public key.
 */
    QString masterPublicKey() const;
/**
 * @brief Returns slatepack secret key.
 */
    QByteArray slatepackSecretKey() const;
/**
 * @brief Returns rewind output proof.
 */
    RewindResult rewindOutputProof(const QString &commitment, const QString &proof) const;
/**
 * @brief Computes derive output secrets.
 */
    OutputSecrets deriveOutputSecrets(quint32 childIndex, quint64 amount) const;

private:
    QString m_mnemonic;
    QByteArray m_masterSecret;
    QByteArray m_masterChainCode;
    QByteArray m_masterPublicKey;
    QByteArray m_bulletProofNonce;
    QByteArray m_rewindNonceHash;
};

#endif // WALLETKEYCHAIN_H
