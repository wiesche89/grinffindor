#include "grinwalletcontroller.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QDebug>
#include <QUrl>
#include <QUuid>
#include <QVector>
#include <cstdlib>
#include <algorithm>

#include "../3rdparty/monocypher/monocypher.h"

#include "wallet/slatev4.h"
#include "wallet/walletoutput.h"
#include "wallet/walletscanner.h"
#include "wallet/walletselection.h"
#include "wallet/walletkeychain.h"
#include "wallet/wallettxbuilder.h"
#include "grinwalletstorage.h"
#include "grinwalletplatformhelpers.h"
#include "grinwalletcontrollerhelpers.h"
#include "grinwallethistoryhelpers.h"
#include "grinwalletseedcrypto.h"
#include "grinwalletshortcutbridge.h"
#include "grinwalletworkflow.h"
#include "grinwalletnodesync.h"
#include "grinwalletnodesyncservice.h"
#include "grinwallettransactionstore.h"
#include "grinwalletworkflowhelpers.h"
#include "grinwalletworkflowservice.h"
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

const int kSessionAutoLockIntervalMs = 15 * 60 * 1000;
const int kSeedCipherVersion = 3;
const char *kAppSettingsKey = "app_settings";
const char *kAutoLockOnDeactivateKey = "auto_lock_on_app_deactivate";

} // namespace

GrinWalletController::GrinWalletController(QObject *parent) :
    QObject(parent),
    m_nodeApi(0),
    m_nodeSyncService(new GrinWalletNodeSyncService(this)),
    m_workflowService(new GrinWalletWorkflowService(this)),
    m_shortcutBridge(new GrinWalletShortcutBridge(this)),
    m_autoRefreshTimer(0),
    m_sessionLockTimer(0),
    m_walletExists(false),
    m_walletUnlocked(false),
    m_selectedNetwork(GrinWalletControllerHelpers::defaultNetworkName()),
    m_storagePersistenceState(QStringLiteral("unknown")),
    m_chainHeight(0),
    m_nodeBlockHeaderVersion(0),
    m_syncStatus(QStringLiteral("Idle")),
    m_totalBalance(QStringLiteral("0.000000000")),
    m_spendableBalance(QStringLiteral("0.000000000")),
    m_lockedBalance(QStringLiteral("0.000000000")),
    m_immatureBalance(QStringLiteral("0.000000000")),
    m_awaitingConfirmationBalance(QStringLiteral("0.000000000")),
    m_awaitingFinalizationBalance(QStringLiteral("0.000000000")),
    m_scanHeight(0),
    m_autoLockOnAppDeactivate(false),
    m_walletScanInFlight(false),
    m_seedScanActive(false),
    m_seedScanNextIndex(1),
    m_pendingBroadcastInputLookup(false),
    m_broadcastStatusRefreshInFlight(false),
    m_kernelStatusCheckInFlight(false)
{
}

bool GrinWalletController::walletExists() const { return m_walletExists; }
bool GrinWalletController::walletUnlocked() const { return m_walletUnlocked; }
QString GrinWalletController::walletName() const { return m_walletName; }
QString GrinWalletController::mnemonicPreview() const { return m_mnemonicPreview; }
QString GrinWalletController::seedFingerprint() const { return m_seedFingerprint; }
QString GrinWalletController::selectedNetwork() const { return m_selectedNetwork; }
QString GrinWalletController::nodeUrl() const { return m_nodeUrl; }
QString GrinWalletController::storagePersistenceState() const { return m_storagePersistenceState; }
qulonglong GrinWalletController::chainHeight() const { return m_chainHeight; }
QString GrinWalletController::syncStatus() const { return m_syncStatus; }
QString GrinWalletController::totalBalance() const { return m_totalBalance; }
QString GrinWalletController::spendableBalance() const { return m_spendableBalance; }
QString GrinWalletController::lockedBalance() const { return m_lockedBalance; }
QString GrinWalletController::immatureBalance() const { return m_immatureBalance; }
QString GrinWalletController::awaitingConfirmationBalance() const { return m_awaitingConfirmationBalance; }
QString GrinWalletController::awaitingFinalizationBalance() const { return m_awaitingFinalizationBalance; }
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }
QString GrinWalletController::lastError() const { return m_lastError; }
QString GrinWalletController::lastInfo() const { return m_lastInfo; }
QString GrinWalletController::workflowId() const { return m_workflowId; }
QString GrinWalletController::workflowState() const { return m_workflowState; }
QString GrinWalletController::workflowMode() const { return m_workflowMode; }
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }
bool GrinWalletController::autoLockOnAppDeactivate() const { return m_autoLockOnAppDeactivate; }
QVariantList GrinWalletController::transactionHistory() const
{
    QVariantList history;
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    QList<QJsonObject> entries;
    entries.reserve(transactions.size());
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = GrinWalletHistoryHelpers::inferredConfirmedHeightForTransactionEntry(entry, outputs);
        const QString status = entry.value(QStringLiteral("status")).toString();
        qint64 confirmations = 0;
        if (confirmedHeight > 0 && m_chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<qint64>(m_chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
        }
        if (confirmedHeight > 0) {
            entry.insert(QStringLiteral("confirmed_height"), confirmedHeight);
        }
        entry.insert(QStringLiteral("confirmations"), confirmations);
        if (confirmedHeight > 0 && confirmations > 0 && status != QStringLiteral("cancelled")) {
            entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
        }
        const QString displayAmount = GrinWalletControllerHelpers::displayAmountForTransactionEntry(entry, outputs);
        if (!displayAmount.isEmpty()) {
            entry.insert(QStringLiteral("amount"), displayAmount);
        }
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), GrinWalletController::transactionEntryLessThan);

    history.reserve(entries.size());
    for (const QJsonObject &entry : entries) {
        history.append(entry.toVariantMap());
    }
    return history;
}

QVariantList GrinWalletController::walletOutputs() const
{
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    std::sort(outputs.begin(), outputs.end(), GrinWalletController::walletOutputLessThan);

    QVariantList list;
    list.reserve(outputs.size());
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        QJsonObject entry = output.toJson();

        QString status;
        if (output.spent) {
            status = QStringLiteral("spent");
        } else if (output.pending) {
            status = QStringLiteral("pending");
        } else if (output.locked) {
            status = QStringLiteral("locked");
        } else if (output.coinbase && (!output.onChain || output.height == 0 || m_chainHeight < output.height + 1000)) {
            status = QStringLiteral("immature");
        } else if (!output.onChain) {
            status = QStringLiteral("local");
        } else {
            status = QStringLiteral("spendable");
        }

        const bool mature = !output.coinbase || (output.height > 0 && m_chainHeight >= output.height + 1000);
        const bool confirmed = output.onChain && output.height > 0 && m_chainHeight >= output.height + 10;
        const qint64 confirmations =
            output.height > 0 && m_chainHeight >= output.height
                ? static_cast<qint64>(m_chainHeight - output.height + 1)
                : 0;
        const bool spendable = !output.spent && !output.locked && !output.pending && output.onChain && mature && confirmed;

        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("mature"), mature);
        entry.insert(QStringLiteral("confirmed"), confirmed);
        entry.insert(QStringLiteral("confirmations"), confirmations);
        entry.insert(QStringLiteral("spendable"), spendable);
        list.append(entry.toVariantMap());
    }

    return list;
}

void GrinWalletController::initialize()
{
    m_shortcutBridge->install();
    loadFromStorage();
    connectNodeClient();
    startAutoRefresh();
    refreshStoragePersistenceState();
    refreshNodeStatus();
}

QString GrinWalletController::generateMnemonic() const
{
    return GrinWalletSeedCrypto::generateMnemonic();
}

bool GrinWalletController::validateMnemonic(const QString &mnemonic) const
{
    return GrinWalletSeedCrypto::isValidMnemonic(mnemonic);
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
    m_mnemonicPreview = mnemonic;
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
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
    const QString normalizedMnemonic = GrinWalletSeedCrypto::normalizeMnemonic(mnemonic);
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
    m_mnemonicPreview.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet restored locally. Seed is encrypted in local storage and stays hidden after setup."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::unlockWallet(const QString &password)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject wallet = GrinWalletStorage::walletForNetwork(document, resolvedNetworkName());
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
    m_mnemonicPreview.clear();
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet unlocked locally."));

    const QJsonObject encryptedSeed = wallet.value(QStringLiteral("encrypted_seed")).toObject();
    if (encryptedSeed.value(QStringLiteral("version")).toInt(1) < kSeedCipherVersion) {
        const QJsonObject upgradedSeed = GrinWalletSeedCrypto::encryptMnemonic(mnemonic, password);
        if (!upgradedSeed.isEmpty()) {
            wallet.insert(QStringLiteral("encrypted_seed"), upgradedSeed);
            document.insert(QStringLiteral("wallet"), wallet);
            GrinWalletStorage::setWalletForNetwork(&document, resolvedNetworkName(), wallet);
            GrinWalletStorage::saveDocument(document);
        }
    }

    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::lockWallet()
{
    m_walletUnlocked = false;
    m_sessionMnemonic.clear();
    m_mnemonicPreview.clear();
    if (m_sessionLockTimer) {
        m_sessionLockTimer->stop();
    }
    emit walletChanged();
    setLastInfo(QStringLiteral("Wallet locked. Seed material cleared from the UI state."));
}

void GrinWalletController::clearLastError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    setLastError(QString());
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
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Seed phrase revealed for the active wallet."));
    return true;
}

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

QString GrinWalletController::exportEncryptedWalletBackup() const
{
    const QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject wallet = GrinWalletStorage::walletForNetwork(document, resolvedNetworkName());
    if (wallet.isEmpty()) {
        return QString();
    }

    QJsonObject backup;
    backup.insert(QStringLiteral("backup_kind"), QStringLiteral("grinffindor.encrypted_wallet_backup"));
    backup.insert(QStringLiteral("backup_version"), 1);
    backup.insert(QStringLiteral("exported_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    backup.insert(QStringLiteral("wallet_name"), wallet.value(QStringLiteral("name")).toString());
    backup.insert(QStringLiteral("seed_fingerprint"), wallet.value(QStringLiteral("seed_fingerprint")).toString());
    backup.insert(QStringLiteral("document"), document);
    return QString::fromUtf8(QJsonDocument(backup).toJson(QJsonDocument::Indented));
}

bool GrinWalletController::importEncryptedWalletBackup(const QString &backupJson)
{
    if (m_walletExists) {
        setLastError(QStringLiteral("Delete the current local wallet for this network before importing an encrypted backup."));
        return false;
    }

    const QString trimmed = backupJson.trimmed();
    if (trimmed.isEmpty()) {
        setLastError(QStringLiteral("Encrypted wallet backup text is required."));
        return false;
    }

    QString validationError;
    const QJsonObject imported = GrinWalletStorage::extractImportedBackupDocument(trimmed.toUtf8(), &validationError);
    if (imported.isEmpty()) {
        setLastError(validationError.isEmpty()
                         ? QStringLiteral("Encrypted wallet backup is invalid.")
                         : validationError);
        return false;
    }

    if (!GrinWalletStorage::saveDocument(imported)) {
        setLastError(QStringLiteral("Failed to persist imported wallet backup."));
        return false;
    }

    clearWorkflow();
    loadFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Encrypted wallet backup imported. Unlock it with the backup password."));
    return true;
}

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

void GrinWalletController::resetNodeUrl()
{
    setNodeUrl(GrinWalletControllerHelpers::defaultNodeUrlForNetwork(resolvedNetworkName()));
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
    m_nodeApi->getVersionAsync();
}

void GrinWalletController::syncWallet()
{
    touchWalletSession();
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
    touchWalletSession();
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before starting a full rescan."));
        setLastInfo(QStringLiteral("Full rescan was skipped because the wallet is locked."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastError(QStringLiteral("A wallet scan is already running."));
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("transaction_rescan_backup"),
                       walletState.value(QStringLiteral("transactions")).toArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 0);
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("full-rescan"));
    walletState.insert(QStringLiteral("last_synced_at"), QString());
    document.insert(QStringLiteral("wallet_state"), walletState);
    GrinWalletStorage::saveDocument(document);
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
    return GrinWalletPlatformHelpers::requestPasteText();
}

bool GrinWalletController::copyTextToClipboard(const QString &text) const
{
    return GrinWalletPlatformHelpers::copyTextToClipboard(text);
}

bool GrinWalletController::downloadTextFile(const QString &suggestedName, const QString &text) const
{
    return GrinWalletPlatformHelpers::downloadTextFile(suggestedName, text);
}

void GrinWalletController::requestPersistentBrowserStorage()
{
    GrinWalletPlatformHelpers::requestPersistentBrowserStorage();
#ifdef Q_OS_WASM
    setLastInfo(QStringLiteral("Browser persistent-storage request sent. Re-check the storage status after the browser responds."));
#else
    setLastInfo(QStringLiteral("Persistent browser storage is only relevant for the WASM/browser wallet."));
#endif
    refreshStoragePersistenceState();
}

void GrinWalletController::updateBrowserShortcutContext(const QString &text,
                                                        const QString &selectedText,
                                                        bool focused) const
{
    m_shortcutBridge->updateBrowserShortcutContext(text, selectedText, focused);
}

bool GrinWalletController::isValidNodeUrl(const QString &nodeUrl) const
{
    return GrinWalletControllerHelpers::isNodeUrlAccepted(nodeUrl);
}

QString GrinWalletController::createSlatepackTemplate(const QString &sender) const
{
    QJsonObject slate;
    slate.insert(QStringLiteral("ver"), QStringLiteral("4:3"));
    slate.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    slate.insert(QStringLiteral("sta"), QStringLiteral("S1"));
    slate.insert(QStringLiteral("amt"), QStringLiteral("0.000000000"));
    slate.insert(QStringLiteral("wallet"), m_walletName);
    slate.insert(QStringLiteral("network"), resolvedNetworkName());
    return encodeSlatepack(QString::fromUtf8(QJsonDocument(slate).toJson(QJsonDocument::Compact)),
                           sender.trimmed());
}

void GrinWalletController::startSendWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startSendWorkflow(amount, note);
}

void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    m_workflowService->startReceiveWorkflow(amount, note);
}

void GrinWalletController::processWorkflowSlatepack(const QString &slatepack)
{
    m_workflowService->processWorkflowSlatepack(slatepack);
}

void GrinWalletController::clearWorkflow()
{
    m_workflowService->clearWorkflow();
}

void GrinWalletController::cleanupLocalAndCancelledItems()
{
    m_workflowService->cleanupLocalAndCancelledItems();
}

void GrinWalletController::broadcastCurrentWorkflowTransaction()
{
    m_workflowService->broadcastCurrentWorkflowTransaction();
}

void GrinWalletController::broadcastTransaction(const QString &workflowId)
{
    m_workflowService->broadcastTransaction(workflowId);
}

void GrinWalletController::beginBroadcastWithInputPreflight(const QString &workflowId,
                                                            const QJsonObject &txSkeleton)
{
    m_nodeSyncService->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
}

void GrinWalletController::cancelTransaction(const QString &workflowId)
{
    m_workflowService->cancelTransaction(workflowId);
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
            if (BinarySlateV4Writer::encodeSlatepack(
                    slate,
                    &binarySlatepack,
                    &writerError,
                    sender.trimmed(),
                    QStringList(),
                    currentSlatepackSecret())) {
                return binarySlatepack;
            }
        }
    }

    return GrinWalletWorkflowHelpers::encodeSlatepackArmor(trimmed, sender.trimmed());
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
    return GrinWalletWorkflowHelpers::decodeIncomingSlatepack(slatepack, decryptionKey);
}

QString GrinWalletController::currentSlatepackAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::slatepackAddress(keychain, m_selectedNetwork) : QString();
}

QString GrinWalletController::currentPaymentProofAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::paymentProofAddress(keychain) : QString();
}

QByteArray GrinWalletController::currentSlatepackSecret() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QByteArray();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? keychain.slatepackSecretKey() : QByteArray();
}

void GrinWalletController::alignSlateVersionWithNode(SlateV4 *slate) const
{
    if (!slate) {
        return;
    }

    if (slate->ver.slateVersion <= 0) {
        slate->ver.slateVersion = 4;
    }

    const bool preserveIncomingInvoiceVersion =
        slate->metadata.value(QStringLiteral("external_binary")).toBool()
        && slate->modeCode() == QStringLiteral("invoice");
    if (preserveIncomingInvoiceVersion) {
        return;
    }

    if (m_nodeBlockHeaderVersion > 0 && slate->ver.blockHeaderVersion != m_nodeBlockHeaderVersion) {
        slate->ver.blockHeaderVersion = m_nodeBlockHeaderVersion;
    }
}

void GrinWalletController::loadFromStorage()
{
    const QJsonObject document = GrinWalletStorage::loadDocument();
    const GrinWalletStorage::LoadedState state = GrinWalletStorage::loadState(document);

    m_walletExists = state.walletExists;
    m_walletUnlocked = false;
    m_walletName = state.walletName;
    m_sessionMnemonic.clear();
    m_seedFingerprint = state.seedFingerprint;
    m_mnemonicPreview.clear();
    m_selectedNetwork = state.selectedNetwork;
    m_nodeUrl = state.nodeUrl;
    m_autoLockOnAppDeactivate = state.autoLockOnDeactivate;

    emit walletChanged();
    emit nodeConfigChanged();
    refreshStateFromStorage();
}

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
    emit statusChanged();
    setLastError(QString());
    setLastInfo(enabled
        ? QStringLiteral("Automatic wallet lock on app deactivation enabled.")
        : QStringLiteral("Automatic wallet lock on app deactivation disabled."));
}

QString GrinWalletController::resolvedNetworkName() const
{
    return GrinWalletControllerHelpers::isAcceptedNetworkName(m_selectedNetwork) ? m_selectedNetwork : GrinWalletControllerHelpers::defaultNetworkName();
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

QString GrinWalletController::resolveWorkflowIdBySlateId(const SlateV4 &slate) const
{
    if (slate.id.trimmed().isEmpty()) {
        return QString();
    }

    const QJsonArray transactions = GrinWalletStorage::loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("slate_id")).toString() == slate.id) {
            const QString resolvedWorkflowId =
                tx.value(QStringLiteral("workflow_id")).toString().trimmed();
            if (!resolvedWorkflowId.isEmpty()) {
                return resolvedWorkflowId;
            }
        }
    }

    return QString();
}

quint64 GrinWalletController::resolveWorkflowAmountNano(const QString &workflowId,
                                                        const QJsonObject &localContext,
                                                        const QString &amount) const
{
    quint64 resolvedAmount = GrinWalletWorkflowHelpers::amountToNanogrin(amount);
    if (resolvedAmount == 0) {
        resolvedAmount = localContext.value(QStringLiteral("amount_nano")).toVariant().toULongLong();
    }
    if (resolvedAmount != 0) {
        return resolvedAmount;
    }

    const QJsonArray transactions = GrinWalletStorage::loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() == workflowId) {
            return GrinWalletWorkflowHelpers::amountToNanogrin(tx.value(QStringLiteral("amount")).toString());
        }
    }

    return 0;
}

QJsonObject GrinWalletController::legacyInvoiceParticipantFromContext(const QJsonObject &localContext)
{
    QJsonObject json;
    const QString secKey = localContext.value(QStringLiteral("invoice_sender_blind_secret")).toString();
    const QString secNonce = localContext.value(QStringLiteral("invoice_sender_nonce_secret")).toString();
    const QString pubKey = localContext.value(QStringLiteral("invoice_sender_blind_public")).toString();
    const QString pubNonce = localContext.value(QStringLiteral("invoice_sender_nonce_public")).toString();
    if (secKey.isEmpty() || secNonce.isEmpty() || pubKey.isEmpty() || pubNonce.isEmpty()) {
        return QJsonObject();
    }

    json.insert(QStringLiteral("sec_key"), secKey);
    json.insert(QStringLiteral("sec_nonce"), secNonce);
    json.insert(QStringLiteral("pub_key"), pubKey);
    json.insert(QStringLiteral("pub_nonce"), pubNonce);
    return json;
}

bool GrinWalletController::transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right)
{
    return GrinWalletHistoryHelpers::transactionEntryLessThan(left, right);
}

bool GrinWalletController::walletOutputLessThan(const WalletOutput &left, const WalletOutput &right)
{
    if (left.spent != right.spent) {
        return !left.spent && right.spent;
    }
    if (left.locked != right.locked) {
        return left.locked && !right.locked;
    }
    if (left.pending != right.pending) {
        return left.pending && !right.pending;
    }
    if (left.onChain != right.onChain) {
        return left.onChain && !right.onChain;
    }
    if (left.height != right.height) {
        return left.height > right.height;
    }
    return left.commitment < right.commitment;
}

void GrinWalletController::markTransactionBroadcastPending(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletTransactionStore::markBroadcastPending(&document, workflowId)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::markTransactionBroadcastFailed(const QString &workflowId, const QString &message)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletTransactionStore::markBroadcastFailed(&document, workflowId, message)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::markTransactionKernelConfirmed(const QString &workflowId, qulonglong confirmedHeight)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletTransactionStore::markKernelConfirmed(&document, workflowId, m_chainHeight, confirmedHeight)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::markTransactionKernelBroadcasted(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletTransactionStore::markKernelBroadcasted(&document, workflowId)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::markTransactionBroadcastRejected(const QString &workflowId, const QString &message)
{
    markTransactionBroadcastFailed(workflowId, message);
}

void GrinWalletController::markTransactionBroadcastSucceeded(const QString &workflowId)
{
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletTransactionStore::markBroadcastSucceeded(&document, workflowId)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::connectNodeClient()
{
    m_nodeSyncService->connectNodeClient();
}

void GrinWalletController::refreshStateFromStorage()
{
    GrinWalletStorage::RefreshedState state = GrinWalletStorage::refreshState(GrinWalletStorage::loadDocument(), m_chainHeight);
    if (state.balancesChanged) {
        GrinWalletStorage::saveDocument(state.document);
    }

    m_scanHeight = state.scanHeight;
    m_totalBalance = state.totalBalance;
    m_spendableBalance = state.spendableBalance;
    m_lockedBalance = state.lockedBalance;
    m_immatureBalance = state.immatureBalance;
    m_awaitingConfirmationBalance = state.awaitingConfirmationBalance;
    m_awaitingFinalizationBalance = state.awaitingFinalizationBalance;
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

    if (!m_sessionLockTimer) {
        m_sessionLockTimer = new QTimer(this);
        m_sessionLockTimer->setInterval(kSessionAutoLockIntervalMs);
        m_sessionLockTimer->setSingleShot(true);
        connect(m_sessionLockTimer, &QTimer::timeout, this, &GrinWalletController::onSessionLockTimeout);
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this, &GrinWalletController::onApplicationStateChanged);
}

void GrinWalletController::onSessionLockTimeout()
{
    if (!m_walletUnlocked) {
        return;
    }

    lockWallet();
    setLastInfo(QStringLiteral("Wallet locked after inactivity."));
}

void GrinWalletController::onApplicationStateChanged(Qt::ApplicationState state)
{
    if (!m_walletUnlocked) {
        return;
    }
    if (!m_autoLockOnAppDeactivate) {
        return;
    }
    if (state == Qt::ApplicationHidden || state == Qt::ApplicationInactive) {
        lockWallet();
        setLastInfo(QStringLiteral("Wallet locked because the browser tab or app became inactive."));
    }
}

QJsonObject GrinWalletController::loadDocumentForService() const
{
    return GrinWalletStorage::loadDocument();
}

bool GrinWalletController::saveDocumentForService(const QJsonObject &document) const
{
    return GrinWalletStorage::saveDocument(document);
}

quint32 GrinWalletController::nextChildIndexFromStateForService(const QJsonObject &walletState) const
{
    return static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
}

QJsonObject GrinWalletController::filterWorkflowContextsForTransactionsForService(const QJsonObject &contexts,
                                                                                  const QJsonArray &transactions) const
{
    return GrinWalletControllerHelpers::filterWorkflowContextsForTransactions(contexts, transactions);
}

void GrinWalletController::storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit)
{
    if (commit.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
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

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == output.commitment) {
            outputs[i] = output;
            walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
            walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
            if (output.childIndex + 1 > static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt())) {
                walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
            }
            document.insert(QStringLiteral("wallet_state"), walletState);
            GrinWalletStorage::saveDocument(document);
            refreshStateFromStorage();
            return;
        }
    }

    outputs.append(output);
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    if (output.childIndex + 1 > static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt())) {
        walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
    }
    document.insert(QStringLiteral("wallet_state"), walletState);
    GrinWalletStorage::saveDocument(document);
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

    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const quint32 childIndex = static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
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

bool GrinWalletController::ensureWorkflowSelectionContext(const QString &workflowId,
                                                          const QString &amount,
                                                          QString *feeOut,
                                                          QString *errorOut)
{
    if (workflowId.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow id is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    if (!localContext.value(QStringLiteral("selected_input_commits")).toArray().isEmpty()) {
        const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
        const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
        bool hasTransactionEntry = false;
        for (int i = 0; i < transactions.size(); ++i) {
            if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == workflowId) {
                hasTransactionEntry = true;
                break;
            }
        }
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        bool hasMatchingSelection = false;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const WalletOutput output =
                GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, selectedCommitments.at(i).toString());
            if (!output.commitment.isEmpty()) {
                hasMatchingSelection = true;
                if (output.locked) {
                    break;
                }
            }
        }

        // Inputs selected for a SEND workflow intentionally keep their original
        // workflow id (the workflow that created the UTXO). Requiring
        // output.workflowId == workflowId here can incorrectly discard a valid
        // S1 selection before S3 finalization.
        if (!hasMatchingSelection) {
            localContext.remove(QStringLiteral("selected_inputs"));
            localContext.remove(QStringLiteral("selected_total"));
            localContext.remove(QStringLiteral("selected_input_commits"));
            localContext.remove(QStringLiteral("selected_input_coinbase"));
            localContext.remove(QStringLiteral("change_amount"));
            localContext.remove(QStringLiteral("amount_nano"));
            localContext.remove(QStringLiteral("amount_display"));
            localContext.remove(QStringLiteral("fee_nano"));
            localContext.remove(QStringLiteral("fee_amount_display"));
            localContext.remove(QStringLiteral("change_commit"));
            localContext.remove(QStringLiteral("change_proof"));
            localContext.remove(QStringLiteral("change_amount_display"));
            localContext.remove(QStringLiteral("change_child_index"));
            localContext.remove(QStringLiteral("change_key_path"));
            storeWorkflowContext(workflowId, localContext);
        } else {
        if (!hasTransactionEntry) {
            SlateV4 placeholder;
            placeholder.id = workflowId;
            placeholder.metadata.insert(QStringLiteral("workflow_id"), workflowId);
            placeholder.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
            placeholder.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
            placeholder.setStateFromCode(QStringLiteral("I1"));
            placeholder.amount = localContext.value(QStringLiteral("amount_display")).toString();
            placeholder.fee = localContext.value(QStringLiteral("fee_amount_display")).toString();
            placeholder.offset = localContext.value(QStringLiteral("offset")).toString();
            persistWorkflowTransaction(placeholder, false);
        }
        QJsonObject selectedInputCoinbase;
        quint64 selectedTotal = 0;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const QString commitment = selectedCommitments.at(i).toString();
            const WalletOutput output = GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, commitment);
            if (!output.commitment.isEmpty()) {
                selectedInputCoinbase.insert(output.commitment, output.coinbase);
                selectedTotal += GrinWalletWorkflowHelpers::amountToNanogrin(output.amount);
            } else {
                const QJsonValue persistedValue =
                    localContext.value(QStringLiteral("selected_input_coinbase")).toObject().value(commitment);
                if (!persistedValue.isUndefined()) {
                    selectedInputCoinbase.insert(commitment, persistedValue.toBool());
                }
            }
        }
        if (!selectedInputCoinbase.isEmpty()
            && selectedInputCoinbase != localContext.value(QStringLiteral("selected_input_coinbase")).toObject()) {
            localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
            storeWorkflowContext(workflowId, localContext);
        }
        if (!localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).isObject()) {
            const QJsonObject legacyParticipant = legacyInvoiceParticipantFromContext(localContext);
            if (!legacyParticipant.isEmpty()) {
                localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")), legacyParticipant);
                localContext.remove(QStringLiteral("invoice_sender_blind_secret"));
                localContext.remove(QStringLiteral("invoice_sender_nonce_secret"));
                localContext.remove(QStringLiteral("invoice_sender_blind_public"));
                localContext.remove(QStringLiteral("invoice_sender_nonce_public"));
                storeWorkflowContext(workflowId, localContext);
            }
        }
        quint64 amountNano = localContext.value(QStringLiteral("amount_nano")).toVariant().toULongLong();
        if (amountNano == 0) {
            amountNano = resolveWorkflowAmountNano(workflowId, localContext, amount);
            if (amountNano > 0) {
                localContext.insert(QStringLiteral("amount_nano"), QString::number(amountNano));
                if (localContext.value(QStringLiteral("amount_display")).toString().trimmed().isEmpty()) {
                    localContext.insert(QStringLiteral("amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(amountNano));
                }
                storeWorkflowContext(workflowId, localContext);
            }
        }
        if (amountNano == 0) {
            if (errorOut) {
                *errorOut = QStringLiteral("Workflow amount context is missing. Restart the send workflow from S1.");
            }
            return false;
        }
        const quint64 feeWithoutChange =
            WalletSelection::estimateFee(selectedCommitments.size(), 1, 1);
        const quint64 feeWithChange =
            WalletSelection::estimateFee(selectedCommitments.size(), 2, 1);
        const bool exactNoChange = (selectedTotal == amountNano + feeWithoutChange);
        const quint64 recalculatedFee = exactNoChange ? feeWithoutChange : feeWithChange;
        const quint64 recalculatedChange =
            (selectedTotal >= amountNano + recalculatedFee)
                ? (selectedTotal - amountNano - recalculatedFee)
                : 0;
        const quint64 persistedFee = localContext.value(QStringLiteral("fee_nano")).toVariant().toULongLong();
        const QString existingChangeAmountDisplay = localContext.value(QStringLiteral("change_amount_display")).toString();
        const QString recalculatedChangeDisplay =
            recalculatedChange > 0 ? GrinWalletWorkflowHelpers::formatNanogrin(recalculatedChange) : QString();
        if (persistedFee != recalculatedFee
            || localContext.value(QStringLiteral("selected_total")).toString() != QString::number(selectedTotal)
            || localContext.value(QStringLiteral("change_amount")).toString() != QString::number(recalculatedChange)
            || existingChangeAmountDisplay != recalculatedChangeDisplay) {
            localContext.insert(QStringLiteral("selected_total"), QString::number(selectedTotal));
            localContext.insert(QStringLiteral("fee_nano"), QString::number(recalculatedFee));
            localContext.insert(QStringLiteral("fee_amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(recalculatedFee));
            localContext.insert(QStringLiteral("change_amount"), QString::number(recalculatedChange));
            if (recalculatedChange > 0) {
                WalletOutput changeOutput;
                SlateV4::Commit changeCommit;
                QString outputError;
                if (buildOwnedOutput(QStringLiteral("change"),
                                     recalculatedChangeDisplay,
                                     &changeOutput,
                                     &changeCommit,
                                     &outputError)) {
                    changeOutput.workflowId = workflowId;
                    storeOwnedOutput(changeOutput);
                    localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
                    localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
                    localContext.insert(QStringLiteral("change_amount_display"), recalculatedChangeDisplay);
                    localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
                    localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
                } else if (errorOut) {
                    *errorOut = outputError.isEmpty()
                        ? QStringLiteral("Failed to rebuild change output.")
                        : outputError;
                    return false;
                }
            } else {
                localContext.remove(QStringLiteral("change_commit"));
                localContext.remove(QStringLiteral("change_proof"));
                localContext.remove(QStringLiteral("change_amount_display"));
                localContext.remove(QStringLiteral("change_child_index"));
                localContext.remove(QStringLiteral("change_key_path"));
            }
            storeWorkflowContext(workflowId, localContext);
        }
        if (feeOut) {
            *feeOut = recalculatedFee > 0
                ? GrinWalletWorkflowHelpers::formatNanogrin(recalculatedFee)
                : localContext.value(QStringLiteral("fee_amount_display")).toString();
        }
        return true;
        }
    }

    quint64 requestedAmount = resolveWorkflowAmountNano(workflowId, localContext, amount);
    if (requestedAmount == 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow amount must be greater than zero.");
        }
        return false;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const WalletSelection::Result selection =
        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, m_chainHeight);
    if (!selection.success) {
        if (errorOut) {
            *errorOut = selection.error;
        }
        return false;
    }

    QJsonArray selectedCommitments;
    QJsonObject selectedInputCoinbase;
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs.at(i).commitment != selection.selectedOutputs.at(j).commitment) {
                continue;
            }

            outputs[i].locked = true;
            outputs[i].pending = false;
            // Preserve the UTXO's original workflowId (e.g. from the receive/invoice
            // that created it). The send workflow tracks inputs via selected_input_commits.
            selectedCommitments.append(outputs.at(i).commitment);
            selectedInputCoinbase.insert(outputs.at(i).commitment, outputs.at(i).coinbase);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    if (!GrinWalletStorage::saveDocument(document)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to persist sender selection context.");
        }
        return false;
    }

    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    localContext.insert(QStringLiteral("amount_nano"), QString::number(requestedAmount));
    localContext.insert(QStringLiteral("amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("fee_nano"), QString::number(selection.fee));
    localContext.insert(QStringLiteral("fee_amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(selection.fee));
    if (!localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).isObject()) {
        localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(
                                WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"))));
    }

    if (selection.change > 0 && localContext.value(QStringLiteral("change_commit")).toString().isEmpty()) {
        const QString changeAmount = GrinWalletWorkflowHelpers::formatNanogrin(selection.change);
        WalletOutput changeOutput;
        SlateV4::Commit changeCommit;
        QString outputError;
        if (buildOwnedOutput(QStringLiteral("change"), changeAmount, &changeOutput, &changeCommit, &outputError)) {
            changeOutput.workflowId = workflowId;
            storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (errorOut) {
            *errorOut = outputError.isEmpty()
                ? QStringLiteral("Failed to derive change output.")
                : outputError;
            return false;
        }
    }

    storeWorkflowContext(workflowId, localContext);
    refreshStateFromStorage();
    if (feeOut) {
        *feeOut = GrinWalletWorkflowHelpers::formatNanogrin(selection.fee);
    }
    return true;
}

bool GrinWalletController::prepareInvoiceSenderContext(
    const QString &workflowId,
    SlateV4 *slate,
    WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
    QString *errorOut)
{
    if (!slate || !signatureOverrideOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invoice sender context target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    WalletCryptoBackend::ParticipantContext senderAggsig =
        GrinWalletWorkflowHelpers::participantContextFromJson(localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).toObject(),
                                   QStringLiteral("sender"));
    if (senderAggsig.blindSecret.isEmpty()
        || senderAggsig.nonceSecret.isEmpty()
        || senderAggsig.blindPublic.isEmpty()
        || senderAggsig.noncePublic.isEmpty()) {
        senderAggsig = WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
        localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(senderAggsig));
        storeWorkflowContext(workflowId, localContext);
    }

    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
    if (!m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < trackedOutputs.size(); ++i) {
                trackedOutputs[i] = GrinWalletControllerHelpers::normalizedTrackedOutput(trackedOutputs.at(i), keychain);
            }
        }
    }

    QStringList positiveBlinds;
    const QString priorOffset = slate->offset.trimmed();
    if (!priorOffset.isEmpty() && priorOffset != QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000")) {
        positiveBlinds.append(priorOffset);
    }
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommit.isEmpty()) {
        const WalletOutput changeOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommit);
        if (!changeOutput.blindingFactor.isEmpty()) {
            positiveBlinds.append(changeOutput.blindingFactor);
        }
    }

    QStringList negativeBlinds;
    negativeBlinds.append(senderAggsig.blindSecret);
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (!input.blindingFactor.isEmpty()) {
            negativeBlinds.append(input.blindingFactor);
        }
    }

    QString cryptoError;
    const QString adjustedOffset = WalletCryptoBackend::combineBlindingFactors(
        positiveBlinds,
        negativeBlinds,
        &cryptoError);
    if (adjustedOffset.isEmpty()) {
        if (errorOut) {
            *errorOut = cryptoError.isEmpty()
                ? QStringLiteral("Failed to derive invoice sender offset.")
                : cryptoError;
        }
        return false;
    }

    slate->offset = adjustedOffset;
    *signatureOverrideOut = senderAggsig;

    QList<SlateV4::Commit> rebuiltCommitments = slate->commitments;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (input.commitment.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (int j = 0; j < rebuiltCommitments.size(); ++j) {
            if (rebuiltCommitments.at(j).commitment == input.commitment) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        SlateV4::Commit commit;
        const bool inputCoinbase =
            selectedInputCoinbase.value(input.commitment).toBool(input.coinbase);
        commit.feature = inputCoinbase ? 1 : 0;
        commit.commitment = input.commitment;
        rebuiltCommitments.append(commit);
    }

    const QString changeCommitment = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommitment.isEmpty()) {
        const WalletOutput changeOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommitment);
        if (!changeOutput.commitment.isEmpty()) {
            bool exists = false;
            for (int i = 0; i < rebuiltCommitments.size(); ++i) {
                if (rebuiltCommitments.at(i).commitment == changeOutput.commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                SlateV4::Commit commit;
                commit.feature = 0;
                commit.commitment = changeOutput.commitment;
                commit.proof = changeOutput.proof;
                rebuiltCommitments.append(commit);
            }
        }
    }

    slate->commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(rebuiltCommitments);
    return true;
}

bool GrinWalletController::prepareStandardSenderContext(
    const QString &workflowId,
    SlateV4 *slate,
    WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
    QString *errorOut)
{
    if (!slate || !signatureOverrideOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Standard sender context target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    WalletCryptoBackend::ParticipantContext senderAggsig =
        GrinWalletWorkflowHelpers::participantContextFromJson(localContext.value(GrinWalletWorkflowHelpers::standardContextKey(QStringLiteral("participant"))).toObject(),
                                   QStringLiteral("sender"));
    if (senderAggsig.blindSecret.isEmpty()
        || senderAggsig.nonceSecret.isEmpty()
        || senderAggsig.blindPublic.isEmpty()
        || senderAggsig.noncePublic.isEmpty()) {
        senderAggsig = WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
        localContext.insert(GrinWalletWorkflowHelpers::standardContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(senderAggsig));
        storeWorkflowContext(workflowId, localContext);
    }

    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
    if (!m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < trackedOutputs.size(); ++i) {
                trackedOutputs[i] = GrinWalletControllerHelpers::normalizedTrackedOutput(trackedOutputs.at(i), keychain);
            }
        }
    }

    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    const WalletOutput changeOutput = changeCommit.isEmpty()
        ? WalletOutput()
        : GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommit);

    QString canonicalOffset = localContext.value(QStringLiteral("standard_sender_offset")).toString().trimmed();
    if (canonicalOffset.isEmpty()) {
        canonicalOffset = slate->offset.trimmed();
        if (canonicalOffset.isEmpty()) {
            canonicalOffset = localContext.value(QStringLiteral("incoming_s2_offset")).toString().trimmed();
        }
        if (canonicalOffset.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to recover canonical standard sender offset.");
            }
            return false;
        }

        localContext.insert(QStringLiteral("standard_sender_offset"), canonicalOffset);
        storeWorkflowContext(workflowId, localContext);
    }

    const QString incomingOffset = slate->offset.trimmed();
    localContext.insert(QStringLiteral("incoming_s2_offset"), incomingOffset);

    QString effectiveOffset = canonicalOffset;
    if (!incomingOffset.isEmpty()) {
        if (incomingOffset != canonicalOffset) {
            // grin-wallet updates the shared transaction offset during S2.
            // Compact external S2 slates do not reliably preserve our local metadata,
            // so the sender must treat the imported S2 offset as canonical here.
            effectiveOffset = incomingOffset;
        }
    }

    slate->offset = effectiveOffset;
    localContext.insert(QStringLiteral("standard_sender_offset"), canonicalOffset);
    storeWorkflowContext(workflowId, localContext);

    *signatureOverrideOut = senderAggsig;

    QList<SlateV4::Commit> rebuiltCommitments = slate->commitments;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (input.commitment.isEmpty()) {
            continue;
        }
        bool exists = false;
        for (int j = 0; j < rebuiltCommitments.size(); ++j) {
            if (rebuiltCommitments.at(j).commitment == input.commitment) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            SlateV4::Commit commit;
            commit.feature = selectedInputCoinbase.value(input.commitment).toBool(input.coinbase) ? 1 : 0;
            commit.commitment = input.commitment;
            rebuiltCommitments.append(commit);
        }
    }

    if (!changeCommit.isEmpty()) {
        if (!changeOutput.commitment.isEmpty()) {
            bool exists = false;
            for (int i = 0; i < rebuiltCommitments.size(); ++i) {
                if (rebuiltCommitments.at(i).commitment == changeOutput.commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                SlateV4::Commit commit;
                commit.feature = 0;
                commit.commitment = changeOutput.commitment;
                commit.proof = changeOutput.proof;
                rebuiltCommitments.append(commit);
            }
        }
    }

    slate->commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(rebuiltCommitments);
    return true;
}

void GrinWalletController::compactInvoiceSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    WalletCryptoBackend::ParticipantContext senderContext = GrinWalletWorkflowHelpers::participantContextFromJson(
        workflowContext(workflowId).value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).toObject(),
        QStringLiteral("sender"));
    if (senderContext.blindPublic.isEmpty() || senderContext.noncePublic.isEmpty()) {
        senderContext = WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == senderContext.blindPublic && sig.nonce == senderContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }
    slate->numParticipants = 2;
    slate->amount.clear();
    slate->metadata.remove(QStringLiteral("message_hash"));
    slate->metadata.remove(QStringLiteral("pubkey_total"));
    slate->metadata.remove(QStringLiteral("pubnonce_total"));
    slate->metadata.remove(QStringLiteral("signature_status"));
    slate->metadata.remove(QStringLiteral("processed_by"));
    slate->metadata.remove(QStringLiteral("processed_at"));
    slate->metadata.remove(QStringLiteral("tx_ready"));
    slate->metadata.remove(QStringLiteral("network"));
}

void GrinWalletController::compactStandardSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    const QString receiverBlind = slate->metadata.value(QStringLiteral("receiver_blind")).toString().trimmed();
    WalletCryptoBackend::ParticipantContext receiverContext;
    if (!receiverBlind.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipantFromBlindSecret(
            receiverBlind,
            m_seedFingerprint,
            workflowId,
            QStringLiteral("receiver"));
    }
    if (receiverContext.blindSecret.isEmpty()
        || receiverContext.nonceSecret.isEmpty()
        || receiverContext.blindPublic.isEmpty()
        || receiverContext.noncePublic.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    }

    if (!receiverBlind.isEmpty() && !receiverContext.blindSecret.isEmpty()) {
        QStringList positiveBlinds;
        positiveBlinds.append(slate->offset);
        positiveBlinds.append(receiverBlind);

        QStringList negativeBlinds;
        negativeBlinds.append(receiverContext.blindSecret);

        QString offsetError;
        const QString adjustedOffset =
            WalletCryptoBackend::combineBlindingFactors(positiveBlinds, negativeBlinds, &offsetError);
        if (!adjustedOffset.isEmpty()) {
            slate->offset = adjustedOffset;
        }
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == receiverContext.blindPublic && sig.nonce == receiverContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }

    slate->amount.clear();
    slate->fee.clear();
}

bool GrinWalletController::ensureReceiverOutputContext(const QString &workflowId,
                                                       const QString &amount,
                                                       const QString &source,
                                                       WalletOutput *outputOut,
                                                       SlateV4::Commit *commitOut,
                                                       QString *errorOut)
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Receiver output target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    const QString existingCommitment = localContext.value(QStringLiteral("receiver_commit")).toString();
    if (!existingCommitment.isEmpty()) {
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(
            GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject());
        WalletOutput existingOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, existingCommitment);
        if (existingOutput.commitment.isEmpty()) {
            existingOutput.commitment = existingCommitment;
            existingOutput.proof = localContext.value(QStringLiteral("receiver_proof")).toString();
            existingOutput.amount = localContext.value(QStringLiteral("receiver_amount_display")).toString(amount.trimmed());
            existingOutput.source = source;
            existingOutput.keyPath = localContext.value(QStringLiteral("receiver_key_path")).toString();
            existingOutput.blindingFactor = localContext.value(QStringLiteral("receiver_blind")).toString();
            existingOutput.childIndex = static_cast<quint32>(
                localContext.value(QStringLiteral("receiver_child_index")).toInt());
            existingOutput.workflowId = workflowId;
            existingOutput.pending = true;
            existingOutput.locked = false;
            existingOutput.onChain = false;
            existingOutput.spent = false;
            storeOwnedOutput(existingOutput);
        }

        *outputOut = existingOutput;
        commitOut->commitment = existingOutput.commitment;
        commitOut->proof = existingOutput.proof;
        return !existingOutput.commitment.isEmpty();
    }

    WalletOutput output;
    SlateV4::Commit commit;
    if (!buildOwnedOutput(source, amount, &output, &commit, errorOut)) {
        return false;
    }

    output.workflowId = workflowId;
    output.pending = true;
    output.locked = false;
    output.onChain = false;
    output.spent = false;
    storeOwnedOutput(output);

    localContext.insert(QStringLiteral("receiver_commit"), commit.commitment);
    localContext.insert(QStringLiteral("receiver_proof"), commit.proof);
    localContext.insert(QStringLiteral("receiver_amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("receiver_child_index"), static_cast<int>(output.childIndex));
    localContext.insert(QStringLiteral("receiver_key_path"), output.keyPath);
    localContext.insert(QStringLiteral("receiver_blind"), output.blindingFactor);
    storeWorkflowContext(workflowId, localContext);

    *outputOut = output;
    *commitOut = commit;
    return true;
}

void GrinWalletController::persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletWorkflow::persistTransaction(&document, slate, broadcasted)) {
        GrinWalletStorage::saveDocument(document);
    }
}

void GrinWalletController::updateTransactionEntry(const QString &workflowId, const std::function<void (QJsonObject &)> &updater)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
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
    GrinWalletStorage::saveDocument(document);
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
    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletStorage::refreshTransactionConfirmations(&document, m_chainHeight)) {
        GrinWalletStorage::saveDocument(document);
    }
}

void GrinWalletController::refreshStoragePersistenceState()
{
    m_storagePersistenceState = GrinWalletPlatformHelpers::storagePersistenceState();
    emit statusChanged();
}

void GrinWalletController::touchWalletSession()
{
    if (!m_walletUnlocked) {
        return;
    }

    if (!m_sessionLockTimer) {
        startAutoRefresh();
    }
    if (m_sessionLockTimer) {
        m_sessionLockTimer->start();
    }
}

void GrinWalletController::recoverPendingBroadcasts()
{
    m_nodeSyncService->recoverPendingBroadcasts();
}

QJsonArray GrinWalletController::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                                     const QJsonArray &existingTransactions) const
{
    QHash<QString, QJsonObject> knownTransactionsByWorkflow;
    QHash<QString, QString> workflowByCommitment;
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty()) {
            knownTransactionsByWorkflow.insert(workflowId, entry);
        }

        const QStringList commitments = GrinWalletHistoryHelpers::transactionOutputCommitments(entry);
        for (int j = 0; j < commitments.size(); ++j) {
            workflowByCommitment.insert(commitments.at(j),
                                        workflowId.isEmpty()
                                            ? GrinWalletHistoryHelpers::syntheticWorkflowIdForCommitment(commitments.at(j))
                                            : workflowId);
        }
    }

    QHash<QString, QList<WalletOutput> > groupedOutputs;
    QStringList orderedWorkflowIds;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.commitment.isEmpty()) {
            continue;
        }

        QString workflowId = output.workflowId.trimmed();
        if (workflowId.isEmpty()) {
            workflowId = workflowByCommitment.value(output.commitment);
        }
        if (workflowId.isEmpty()) {
            workflowId = GrinWalletHistoryHelpers::syntheticWorkflowIdForCommitment(output.commitment);
        }

        if (!groupedOutputs.contains(workflowId)) {
            orderedWorkflowIds.append(workflowId);
        }
        groupedOutputs[workflowId].append(output);
    }

    QJsonArray transactions;
    QSet<QString> appendedExistingWorkflows;
    for (int i = 0; i < orderedWorkflowIds.size(); ++i) {
        const QString workflowId = orderedWorkflowIds.at(i);
        const QList<WalletOutput> grouped = groupedOutputs.value(workflowId);
        if (grouped.isEmpty()) {
            continue;
        }

        QJsonObject entry = knownTransactionsByWorkflow.value(workflowId);
        appendedExistingWorkflows.insert(workflowId);

        quint64 confirmedHeight = 0;
        bool anySpent = false;
        bool anyPending = false;
        bool anyLocked = false;
        quint64 displayAmount = 0;
        QString primaryCommitment;
        QJsonArray outputCommitments;

        for (int j = 0; j < grouped.size(); ++j) {
            const WalletOutput &output = grouped.at(j);
            outputCommitments.append(output.commitment);
            if (primaryCommitment.isEmpty()) {
                primaryCommitment = output.commitment;
            }
            // Only use unspent outputs for height inference: spent = old input UTXOs
            if (!output.spent && output.height > 0) {
                if (confirmedHeight == 0 || output.height < confirmedHeight) {
                    confirmedHeight = output.height;
                }
            }
            anySpent = anySpent || output.spent;
            anyPending = anyPending || output.pending;
            anyLocked = anyLocked || output.locked;
            if (displayAmount == 0 && output.source != QStringLiteral("change")) {
                displayAmount = GrinWalletWorkflowHelpers::amountToNanogrin(output.amount);
            }
        }

        if (displayAmount == 0) {
            displayAmount = GrinWalletWorkflowHelpers::amountToNanogrin(grouped.first().amount);
        }

        const QString mode = GrinWalletHistoryHelpers::modeFromOutputs(grouped, entry.value(QStringLiteral("mode")).toString());
        const QString existingStatus = entry.value(QStringLiteral("status")).toString();
        const bool existingBroadcasted = entry.value(QStringLiteral("broadcasted")).toBool();
        QString status = existingStatus;
        if (status.isEmpty() || status == QStringLiteral("cancelled")) {
            status = anySpent && mode == QStringLiteral("send")
                ? QStringLiteral("spent")
                : QStringLiteral("confirmed");
        }
        if (anyPending || anyLocked) {
            status = QStringLiteral("in_progress");
        }
        const bool broadcasted = existingBroadcasted
            || (mode == QStringLiteral("send") && status != QStringLiteral("cancelled"));

        entry.insert(QStringLiteral("workflow_id"), workflowId);
        entry.insert(QStringLiteral("mode"), mode);
        entry.insert(QStringLiteral("state"),
                     entry.value(QStringLiteral("state")).toString().isEmpty()
                         ? QStringLiteral("chain")
                         : entry.value(QStringLiteral("state")).toString());
        if (entry.value(QStringLiteral("amount")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("amount"), GrinWalletWorkflowHelpers::formatNanogrin(displayAmount));
        }
        if (entry.value(QStringLiteral("fee")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("fee"), QStringLiteral("0.000000000"));
        }
        entry.insert(QStringLiteral("broadcasted"), broadcasted);
        entry.insert(QStringLiteral("tx_ready"), true);
        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("commitment"), primaryCommitment);
        entry.insert(QStringLiteral("output_commitments"), outputCommitments);
        entry.insert(QStringLiteral("source"),
                     grouped.first().source.isEmpty() ? QStringLiteral("scan") : grouped.first().source);
        entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(confirmedHeight));
        entry.insert(QStringLiteral("confirmations"),
                     confirmedHeight > 0 && m_chainHeight >= confirmedHeight
                         ? static_cast<qint64>(m_chainHeight - confirmedHeight + 1)
                         : 0);
        entry.insert(QStringLiteral("rescan_rebuilt"), true);
        if (entry.value(QStringLiteral("timestamp")).toString().isEmpty()) {
            entry.insert(QStringLiteral("timestamp"), QString());
        }
        transactions.append(entry);
    }

    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (entry.value(QStringLiteral("rescan_rebuilt")).toBool()
            && !workflowId.isEmpty()
            && !appendedExistingWorkflows.contains(workflowId)) {
            continue;
        }
        if (!workflowId.isEmpty() && !appendedExistingWorkflows.contains(workflowId)) {
            transactions.append(entry);
        }
    }

    return transactions;
}

void GrinWalletController::refreshBroadcastStatuses()
{
    m_nodeSyncService->refreshBroadcastStatuses();
}

void GrinWalletController::startNextKernelStatusCheck()
{
    m_nodeSyncService->startNextKernelStatusCheck();
}

void GrinWalletController::finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject localContext = workflowContext(slate.workflowId());
    if (GrinWalletWorkflow::finalizeOutputs(&document, slate, broadcasted, m_chainHeight, localContext)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::finalizeBroadcastedWorkflow(const QString &workflowId)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    const QJsonObject localContext = workflowContext(workflowId);
    if (GrinWalletWorkflow::finalizeBroadcastedWorkflow(&document, workflowId, m_chainHeight, localContext)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

void GrinWalletController::storeWorkflowContext(const QString &workflowId, const QJsonObject &context)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletStorage::storeWorkflowContext(&document, workflowId, context)) {
        GrinWalletStorage::saveDocument(document);
    }
}

QJsonObject GrinWalletController::workflowContext(const QString &workflowId) const
{
    return GrinWalletStorage::workflowContext(GrinWalletStorage::loadDocument(), workflowId);
}

void GrinWalletController::startSeedScan()
{
    m_nodeSyncService->startSeedScan();
}

void GrinWalletController::finishSeedScan(const QString &message)
{
    m_nodeSyncService->finishSeedScan(message);
}

void GrinWalletController::requestWalletScan()
{
    m_nodeSyncService->requestWalletScan();
}













