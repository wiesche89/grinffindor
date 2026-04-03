#include "grinwalletcontroller.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QStringList>
#include <QUuid>
#include <algorithm>

#include "../3rdparty/monocypher/monocypher.h"

#include "slatev4.h"
#include "walletoutput.h"
#include "walletscanner.h"
#include "walletselection.h"
#include "walletkeychain.h"
#include "wallettxbuilder.h"
#include "grinwalletstorage.h"
#include "grinwalletplatformhelpers.h"
#include "grinwalletcontrollerhelpers.h"
#include "grinwallethistoryhelpers.h"
#include "grinwalletseedcrypto.h"
#include "grinwalletshortcutbridge.h"
#include "grinwalletnodesync.h"
#include "grinwalletnodesyncservice.h"
#include "grinwalletworkflowhelpers.h"
#include "grinwalletworkflowservice.h"
#include "binaryslatev4reader.h"
#include "binaryslatev4writer.h"
#include "walletcryptobackend.h"
#include "nodeforeignapi.h"
#include "result.h"
#include "tip.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "transaction.h"

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

void GrinWalletController::clearLastError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    setLastError(QString());
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













