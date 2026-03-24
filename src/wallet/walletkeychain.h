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

    explicit WalletKeychain(const QString &mnemonic = QString());

    bool isValid() const;
    QString masterPublicKey() const;
    RewindResult rewindOutputProof(const QString &commitment, const QString &proof) const;
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
