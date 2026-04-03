#ifndef GRINWALLETSEEDCRYPTO_H
#define GRINWALLETSEEDCRYPTO_H

#include <QJsonObject>
#include <QString>

class GrinWalletSeedCrypto
{
public:
    static QString generateMnemonic();
    static QString normalizeMnemonic(const QString &mnemonic);
    static bool isValidMnemonic(const QString &mnemonic);
    static QJsonObject encryptMnemonic(const QString &mnemonic, const QString &password);
    static bool decryptMnemonic(const QJsonObject &encrypted, const QString &password, QString *mnemonicOut);
    static QString seedFingerprint(const QString &mnemonic);
};

#endif
