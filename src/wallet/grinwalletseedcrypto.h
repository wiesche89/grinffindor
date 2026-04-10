#ifndef GRINWALLETSEEDCRYPTO_H
#define GRINWALLETSEEDCRYPTO_H

#include <QJsonObject>
#include <QString>

class GrinWalletSeedCrypto
{
public:
/**
 * @brief Processes generate mnemonic.
 */
    static QString generateMnemonic();
/**
 * @brief Returns normalize mnemonic.
 */
    static QString normalizeMnemonic(const QString &mnemonic);
/**
 * @brief Returns whether valid mnemonic.
 */
    static bool isValidMnemonic(const QString &mnemonic);
/**
 * @brief Returns encrypt mnemonic.
 */
    static QJsonObject encryptMnemonic(const QString &mnemonic, const QString &password);
/**
 * @brief Returns decrypt mnemonic.
 */
    static bool decryptMnemonic(const QJsonObject &encrypted, const QString &password, QString *mnemonicOut);
/**
 * @brief Returns seed fingerprint.
 */
    static QString seedFingerprint(const QString &mnemonic);
};

#endif
