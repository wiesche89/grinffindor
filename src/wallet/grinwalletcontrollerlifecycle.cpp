#include "grinwalletcontroller.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QTimer>

#include "grinwalletcontrollerhelpers.h"
#include "grinwalletseedcrypto.h"
#include "grinwalletstorage.h"

namespace {

const char *kAppSettingsKey = "app_settings";
const char *kAutoLockOnDeactivateKey = "auto_lock_on_app_deactivate";
const char *kMinimumConfirmationsKey = "minimum_confirmations";

void wipeQString(QString *value)
{
    if (!value) {
        return;
    }
    if (!value->isEmpty()) {
        value->fill(QChar(u'\0'));
    }
    value->clear();
    value->squeeze();
}

} // namespace

// -------------------------------------------------------------------------------------------------------
// Wallet Creation, Import, And Restore
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Creates a new wallet, encrypts its seed, and persists it to local storage.
 * @param walletName User-facing wallet name.
 * @param password Password used to encrypt the mnemonic.
 */
void GrinWalletController::createWallet(const QString &walletName, const QString &password)
{
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }

    QString mnemonic = generateMnemonic();
    if (mnemonic.isEmpty()) {
        setLastError(QStringLiteral("Failed to generate a valid seed phrase."));
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject wallet;

    const QJsonObject encryptedSeed = GrinWalletSeedCrypto::encryptMnemonic(mnemonic, password);
    if (encryptedSeed.isEmpty()) {
        setLastError(QStringLiteral("Failed to encrypt wallet seed for local storage."));
        return;
    }
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), GrinWalletSeedCrypto::seedFingerprint(mnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptedSeed);
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("generated"));
    wallet.insert(QStringLiteral("network"), resolvedNetworkName());
    document.insert(QStringLiteral("wallet"), wallet);
    GrinWalletStorage::setWalletForNetwork(&document, resolvedNetworkName(), wallet);
    GrinWalletStorage::setWalletStateForNetwork(&document, resolvedNetworkName(), GrinWalletStorage::defaultWalletState());

    GrinWalletStorage::setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());

    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = mnemonic;
    m_sessionMnemonic.detach();
    m_mnemonicPreview.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    wipeQString(&mnemonic);
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
    setLastError(QString());

    setLastInfo(QStringLiteral("Wallet created locally. You can reveal the seed phrase later in Settings after confirming your password."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

/**
 * @brief Imports a wallet from mnemonic by delegating to restore flow.
 * @param walletName User-facing wallet name.
 * @param mnemonic Mnemonic phrase to import.
 * @param password Password used to encrypt the mnemonic.
 */
void GrinWalletController::importWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{
    restoreWallet(walletName, mnemonic, password);
}

/**
 * @brief Restores a wallet from mnemonic, resets network state, and persists it.
 * @param walletName User-facing wallet name.
 * @param mnemonic Mnemonic phrase to restore.
 * @param password Password used to encrypt the mnemonic.
 */
void GrinWalletController::restoreWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{

    QString normalizedMnemonic = GrinWalletSeedCrypto::normalizeMnemonic(mnemonic);
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }
    if (!GrinWalletSeedCrypto::isValidMnemonic(normalizedMnemonic)) {
        setLastError(QStringLiteral("Mnemonic is not valid BIP39 input."));
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject wallet;

    const QJsonObject encryptedSeed = GrinWalletSeedCrypto::encryptMnemonic(normalizedMnemonic, password);
    if (encryptedSeed.isEmpty()) {
        setLastError(QStringLiteral("Failed to encrypt wallet seed for local storage."));
        return;
    }
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), GrinWalletSeedCrypto::seedFingerprint(normalizedMnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptedSeed);
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("restored"));
    wallet.insert(QStringLiteral("network"), resolvedNetworkName());
    document.insert(QStringLiteral("wallet"), wallet);
    GrinWalletStorage::setWalletForNetwork(&document, resolvedNetworkName(), wallet);
    GrinWalletStorage::setWalletStateForNetwork(&document, resolvedNetworkName(), GrinWalletStorage::defaultWalletState());

    GrinWalletStorage::setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());

    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = normalizedMnemonic;
    m_sessionMnemonic.detach();
    m_mnemonicPreview.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    wipeQString(&normalizedMnemonic);
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
    setLastError(QString());

    setLastInfo(QStringLiteral("Wallet restored locally. Seed is encrypted in local storage and stays hidden after setup."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

// -------------------------------------------------------------------------------------------------------
// Wallet Session Lock And Seed Visibility
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Unlocks the active wallet using the provided password.
 * @param password Password used to decrypt the stored mnemonic.
 */
void GrinWalletController::unlockWallet(const QString &password)
{
    const QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject wallet = GrinWalletStorage::walletForNetwork(document, resolvedNetworkName());
    QString mnemonic;
    if (wallet.isEmpty()

        || !GrinWalletSeedCrypto::decryptMnemonic(wallet.value(QStringLiteral("encrypted_seed")).toObject(), password, &mnemonic)) {
        setLastError(QStringLiteral("Failed to unlock wallet. Password is invalid or local data is corrupted."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = wallet.value(QStringLiteral("name")).toString();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    m_sessionMnemonic = mnemonic;
    m_sessionMnemonic.detach();
    m_mnemonicPreview.clear();
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet unlocked locally."));
    wipeQString(&mnemonic);

    if (!resumePendingFullRescan() && m_scanHeight == 0) {
        rescanWallet();
    }
}

/**
 * @brief Locks the active wallet and clears in-memory seed material.
 */
void GrinWalletController::lockWallet()
{
    m_walletUnlocked = false;
    wipeQString(&m_sessionMnemonic);
    wipeQString(&m_mnemonicPreview);
    if (m_sessionLockTimer) {
        m_sessionLockTimer->stop();
    }
    emit walletChanged();
    setLastInfo(QStringLiteral("Wallet locked. Seed material cleared from the UI state."));
}

/**
 * @brief Hides mnemonic preview from UI state.
 */
void GrinWalletController::dismissMnemonicPreview()
{
    if (m_mnemonicPreview.isEmpty()) {
        return;
    }

    wipeQString(&m_mnemonicPreview);
    emit walletChanged();
    setLastInfo(QStringLiteral("Seed phrase hidden. Use your password to unlock the wallet next time."));
}

/**
 * @brief Reveals the seed phrase after password verification.
 * @param password Password used to decrypt the stored mnemonic.
 * @return True when seed phrase was successfully revealed.
 */
bool GrinWalletController::revealSeedPhrase(const QString &password)
{
    if (password.isEmpty()) {
        setLastError(QStringLiteral("Password is required to reveal the seed phrase."));
        return false;
    }

    const QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject wallet = GrinWalletStorage::walletForNetwork(document, resolvedNetworkName());
    QString mnemonic;
    if (wallet.isEmpty()

        || !GrinWalletSeedCrypto::decryptMnemonic(wallet.value(QStringLiteral("encrypted_seed")).toObject(), password, &mnemonic)) {
        setLastError(QStringLiteral("Failed to reveal seed phrase. Password is invalid or local data is corrupted."));
        return false;
    }

    m_mnemonicPreview = mnemonic;
    m_mnemonicPreview.detach();
    wipeQString(&mnemonic);
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Seed phrase revealed for the active wallet."));
    return true;
}

// -------------------------------------------------------------------------------------------------------
// Wallet Backup, Deletion, And Storage Load
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Deletes active-network wallet data and resets controller state.
 */
void GrinWalletController::deleteWallet()
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    document.insert(QStringLiteral("wallet"), GrinWalletStorage::defaultWalletMetadata());
    GrinWalletStorage::setWalletForNetwork(&document, resolvedNetworkName(), GrinWalletStorage::defaultWalletMetadata());
    GrinWalletStorage::setWalletStateForNetwork(&document, resolvedNetworkName(), GrinWalletStorage::defaultWalletState());
    GrinWalletStorage::setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());

    GrinWalletStorage::syncActiveNetworkView(&document, resolvedNetworkName());

    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to delete the local wallet configuration."));
        return;
    }

    clearWorkflow();
    loadFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Local wallet configuration deleted. You can now create or restore a wallet."));
}

/**
 * @brief Loads wallet and node configuration from persistent storage.
 */
void GrinWalletController::loadFromStorage()
{
    const QJsonObject document = GrinWalletStorage::loadDocument();
    const GrinWalletStorage::LoadedState state = GrinWalletStorage::loadState(document);

    m_walletExists = state.walletExists;
    m_walletUnlocked = false;
    m_walletName = state.walletName;
    wipeQString(&m_sessionMnemonic);
    m_seedFingerprint = state.seedFingerprint;
    wipeQString(&m_mnemonicPreview);
    m_selectedNetwork = state.selectedNetwork;
    m_nodeUrl = state.nodeUrl;
    m_autoLockOnAppDeactivate = state.autoLockOnDeactivate;
    m_minimumConfirmations = state.minimumConfirmations;

    emit walletChanged();
    emit nodeConfigChanged();
    refreshStateFromStorage();
}

// -------------------------------------------------------------------------------------------------------
// Runtime Settings And Node Configuration
// -------------------------------------------------------------------------------------------------------

/**
 * @brief Enables or disables automatic wallet locking on app deactivation.
 * @param enabled True to enable app-deactivation auto-lock.
 */
void GrinWalletController::setAutoLockOnAppDeactivate(bool enabled)
{
    if (m_autoLockOnAppDeactivate == enabled) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject appSettings = document.value(QLatin1String(kAppSettingsKey)).toObject();
    appSettings.insert(QLatin1String(kAutoLockOnDeactivateKey), enabled);

    document.insert(QLatin1String(kAppSettingsKey), appSettings);
    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist app settings."));
        return;
    }

    m_autoLockOnAppDeactivate = enabled;
    emit settingsChanged();
    emit statusChanged();
    setLastError(QString());
    setLastInfo(enabled
        ? QStringLiteral("Automatic wallet lock on app deactivation enabled.")
        : QStringLiteral("Automatic wallet lock on app deactivation disabled."));
}

void GrinWalletController::setMinimumConfirmations(int value)
{
    const qulonglong clamped = value > 0 ? static_cast<qulonglong>(value) : 10;
    if (m_minimumConfirmations == clamped) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject appSettings = document.value(QLatin1String(kAppSettingsKey)).toObject();
    appSettings.insert(QLatin1String(kMinimumConfirmationsKey), static_cast<int>(clamped));
    document.insert(QLatin1String(kAppSettingsKey), appSettings);
    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist app settings."));
        return;
    }

    m_minimumConfirmations = clamped;
    refreshStateFromStorage();
    emit settingsChanged();
    setLastError(QString());
}

/**
 * @brief Updates external node URL and reconnects the node client.
 * @param nodeUrl Node endpoint URL.
 * @return True when node URL update succeeds.
 */
bool GrinWalletController::setNodeUrl(const QString &nodeUrl)
{

    const QString trimmed = nodeUrl.trimmed();
    if (!GrinWalletControllerHelpers::isNodeUrlAccepted(trimmed)) {
        setLastError(QStringLiteral("Node URL must be a valid http or https endpoint."));
        return false;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("network"),
                GrinWalletControllerHelpers::inferNetworkName(node.value(QStringLiteral("network")).toString(), trimmed));
    node.insert(QStringLiteral("url"), trimmed);

    document.insert(QStringLiteral("node"), node);
    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist node settings."));
        return false;
    }

    m_selectedNetwork = node.value(QStringLiteral("network")).toString(GrinWalletControllerHelpers::defaultNetworkName());
    m_nodeUrl = trimmed;
    emit nodeConfigChanged();
    connectNodeClient();
    setLastError(QString());
    setLastInfo(QStringLiteral("External node updated. Reconnecting to %1").arg(trimmed));
    refreshNodeStatus();
    return true;
}

/**
 * @brief Switches active wallet network and reloads network-scoped storage view.
 * @param networkName Target network name.
 * @return True when network switch succeeds.
 */
bool GrinWalletController::setSelectedNetwork(const QString &networkName)
{

    const QString normalized = networkName.trimmed().toLower();
    if (!GrinWalletControllerHelpers::isAcceptedNetworkName(normalized)) {
        setLastError(QStringLiteral("Wallet network must be either mainnet or testnet."));
        return false;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    GrinWalletStorage::persistActiveNetworkView(&document, resolvedNetworkName());
    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("network"), normalized);
    node.insert(QStringLiteral("url"), GrinWalletControllerHelpers::defaultNodeUrlForNetwork(normalized));
    document.insert(QStringLiteral("node"), node);

    GrinWalletStorage::syncActiveNetworkView(&document, normalized);
    if (!GrinWalletStorage::saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet network settings."));
        return false;
    }

    loadFromStorage();
    connectNodeClient();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet network switched to %1. Reconnecting to %2")
                    .arg(normalized, m_nodeUrl));
    refreshNodeStatus();
    return true;
}

/**
 * @brief Resets node URL to the default URL of the active network.
 */
void GrinWalletController::resetNodeUrl()
{
    setNodeUrl(GrinWalletControllerHelpers::defaultNodeUrlForNetwork(resolvedNetworkName()));
}
