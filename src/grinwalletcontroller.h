#ifndef GRINWALLETCONTROLLER_H
#define GRINWALLETCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QUrl>
#include <QString>
#include <QVariantList>
#include <functional>

#include "wallet/slatev4.h"
#include "wallet/walletcryptobackend.h"
#include "wallet/walletoutput.h"
#include "locatedtxkernel.h"
#include "nodeversion.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "poolentry.h"
#include "result.h"
#include "tip.h"

class NodeForeignApi;
class QTimer;
class GrinWalletNodeSyncService;
class GrinWalletWorkflowService;
class GrinWalletShortcutBridge;

class GrinWalletController : public QObject
{
    Q_OBJECT
    friend class GrinWalletNodeSyncService;
    friend class GrinWalletWorkflowService;
    Q_PROPERTY(bool walletExists READ walletExists NOTIFY walletChanged)
    Q_PROPERTY(bool walletUnlocked READ walletUnlocked NOTIFY walletChanged)
    Q_PROPERTY(QString walletName READ walletName NOTIFY walletChanged)
    Q_PROPERTY(QString mnemonicPreview READ mnemonicPreview NOTIFY walletChanged)
    Q_PROPERTY(QString seedFingerprint READ seedFingerprint NOTIFY walletChanged)
    Q_PROPERTY(QString selectedNetwork READ selectedNetwork NOTIFY nodeConfigChanged)
    Q_PROPERTY(QString nodeUrl READ nodeUrl NOTIFY nodeConfigChanged)
    Q_PROPERTY(QString storagePersistenceState READ storagePersistenceState NOTIFY statusChanged)
    Q_PROPERTY(qulonglong chainHeight READ chainHeight NOTIFY statusChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY statusChanged)
    Q_PROPERTY(QString totalBalance READ totalBalance NOTIFY statusChanged)
    Q_PROPERTY(QString spendableBalance READ spendableBalance NOTIFY statusChanged)
    Q_PROPERTY(QString lockedBalance READ lockedBalance NOTIFY statusChanged)
    Q_PROPERTY(QString immatureBalance READ immatureBalance NOTIFY statusChanged)
    Q_PROPERTY(QString awaitingConfirmationBalance READ awaitingConfirmationBalance NOTIFY statusChanged)
    Q_PROPERTY(QString awaitingFinalizationBalance READ awaitingFinalizationBalance NOTIFY statusChanged)
    Q_PROPERTY(qulonglong scanHeight READ scanHeight NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastInfo READ lastInfo NOTIFY lastInfoChanged)
    Q_PROPERTY(QString workflowId READ workflowId NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowState READ workflowState NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowMode READ workflowMode NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowSlatepack READ workflowSlatepack NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowDecoded READ workflowDecoded NOTIFY workflowChanged)
    Q_PROPERTY(bool autoLockOnAppDeactivate READ autoLockOnAppDeactivate WRITE setAutoLockOnAppDeactivate NOTIFY statusChanged)
    Q_PROPERTY(QVariantList walletOutputs READ walletOutputs NOTIFY statusChanged)
    Q_PROPERTY(QVariantList transactionHistory READ transactionHistory NOTIFY statusChanged)

public:
    explicit GrinWalletController(QObject *parent = nullptr);

    bool walletExists() const;
    bool walletUnlocked() const;
    QString walletName() const;
    QString mnemonicPreview() const;
    QString seedFingerprint() const;
    QString selectedNetwork() const;
    QString nodeUrl() const;
    QString storagePersistenceState() const;
    qulonglong chainHeight() const;
    QString syncStatus() const;
    QString totalBalance() const;
    QString spendableBalance() const;
    QString lockedBalance() const;
    QString immatureBalance() const;
    QString awaitingConfirmationBalance() const;
    QString awaitingFinalizationBalance() const;
    qulonglong scanHeight() const;
    QString lastError() const;
    QString lastInfo() const;
    QString workflowId() const;
    QString workflowState() const;
    QString workflowMode() const;
    QString workflowSlatepack() const;
    QString workflowDecoded() const;
    bool autoLockOnAppDeactivate() const;
    QVariantList walletOutputs() const;
    QVariantList transactionHistory() const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE QString generateMnemonic() const;
    Q_INVOKABLE bool validateMnemonic(const QString &mnemonic) const;
    Q_INVOKABLE void createWallet(const QString &walletName, const QString &password);
    Q_INVOKABLE void importWallet(const QString &walletName, const QString &mnemonic, const QString &password);
    Q_INVOKABLE void restoreWallet(const QString &walletName, const QString &mnemonic, const QString &password);
    Q_INVOKABLE void unlockWallet(const QString &password);
    Q_INVOKABLE void lockWallet();
    Q_INVOKABLE void clearLastError();
    Q_INVOKABLE void dismissMnemonicPreview();
    Q_INVOKABLE bool revealSeedPhrase(const QString &password);
    Q_INVOKABLE void deleteWallet();
    Q_INVOKABLE QString exportEncryptedWalletBackup() const;
    Q_INVOKABLE bool importEncryptedWalletBackup(const QString &backupJson);
    Q_INVOKABLE bool setSelectedNetwork(const QString &networkName);
    Q_INVOKABLE bool setNodeUrl(const QString &nodeUrl);
    Q_INVOKABLE void resetNodeUrl();
    Q_INVOKABLE void refreshNodeStatus();
    Q_INVOKABLE void syncWallet();
    Q_INVOKABLE void rescanWallet();
    Q_INVOKABLE QString requestPasteText() const;
    Q_INVOKABLE bool copyTextToClipboard(const QString &text) const;
    Q_INVOKABLE bool downloadTextFile(const QString &suggestedName, const QString &text) const;
    Q_INVOKABLE void requestPersistentBrowserStorage();
    Q_INVOKABLE void updateBrowserShortcutContext(const QString &text,
                                                  const QString &selectedText = QString(),
                                                  bool focused = false) const;
    Q_INVOKABLE bool isValidNodeUrl(const QString &nodeUrl) const;
    Q_INVOKABLE QString createSlatepackTemplate(const QString &sender = QString()) const;
    Q_INVOKABLE void startSendWorkflow(const QString &amount, const QString &note = QString());
    Q_INVOKABLE void startReceiveWorkflow(const QString &amount, const QString &note = QString());
    Q_INVOKABLE void processWorkflowSlatepack(const QString &slatepack);
    Q_INVOKABLE void broadcastCurrentWorkflowTransaction();
    Q_INVOKABLE void broadcastTransaction(const QString &workflowId);
    Q_INVOKABLE void cancelTransaction(const QString &workflowId);
    Q_INVOKABLE void clearWorkflow();
    Q_INVOKABLE void cleanupLocalAndCancelledItems();
    Q_INVOKABLE void setAutoLockOnAppDeactivate(bool enabled);
    Q_INVOKABLE QString encodeSlatepack(const QString &slateJson, const QString &sender = QString()) const;
    Q_INVOKABLE QString decodeSlatepack(const QString &slatepack) const;
signals:
    void walletChanged();
    void nodeConfigChanged();
    void statusChanged();
    void lastErrorChanged();
    void lastInfoChanged();
    void workflowChanged();

private:
    void loadFromStorage();
    void setLastError(const QString &error);
    void setLastInfo(const QString &info);
    void connectNodeClient();
    void refreshStateFromStorage();
    void startAutoRefresh();
    void setWorkflow(const QString &id, const QString &mode, const QString &state, const QString &slatepack, const QString &decoded);
    void storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit);
    void storeOwnedOutput(const WalletOutput &output);
    bool buildOwnedOutput(const QString &source,
                          const QString &amount,
                          WalletOutput *outputOut,
                          SlateV4::Commit *commitOut,
                          QString *errorOut = 0) const;
    void persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted);
    void finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted);
    void storeWorkflowContext(const QString &workflowId, const QJsonObject &context);
    QJsonObject workflowContext(const QString &workflowId) const;
    void requestWalletScan();
    void startSeedScan();
    void finishSeedScan(const QString &message);
    void updateTransactionEntry(const QString &workflowId, const std::function<void(QJsonObject &)> &updater);
    void refreshBroadcastStatuses();
    void startNextKernelStatusCheck();
    void refreshStoragePersistenceState();
    QString kernelExcessFromEntry(const QJsonObject &entry) const;
    void refreshTransactionConfirmations();
    void touchWalletSession();
    void recoverPendingBroadcasts();
    void finalizeBroadcastedWorkflow(const QString &workflowId);
    QJsonArray rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                   const QJsonArray &existingTransactions) const;
    bool ensureWorkflowSelectionContext(const QString &workflowId,
                                        const QString &amount,
                                        QString *feeOut = 0,
                                        QString *errorOut = 0);
    bool ensureReceiverOutputContext(const QString &workflowId,
                                     const QString &amount,
                                     const QString &source,
                                     WalletOutput *outputOut,
                                     SlateV4::Commit *commitOut,
                                     QString *errorOut = 0);
    bool prepareInvoiceSenderContext(const QString &workflowId,
                                     SlateV4 *slate,
                                     WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
                                     QString *errorOut = 0);
    bool prepareStandardSenderContext(const QString &workflowId,
                                      SlateV4 *slate,
                                      WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
                                      QString *errorOut = 0);
    void compactInvoiceSlateForReturn(const QString &workflowId, SlateV4 *slate);
    void compactStandardSlateForReturn(const QString &workflowId, SlateV4 *slate);
    QString currentSlatepackAddress() const;
    QString currentPaymentProofAddress() const;
    QByteArray currentSlatepackSecret() const;
    QString resolvedNetworkName() const;
    void alignSlateVersionWithNode(SlateV4 *slate) const;
    void beginBroadcastWithInputPreflight(const QString &workflowId,
                                          const QJsonObject &txSkeleton);
    QString resolveWorkflowIdBySlateId(const SlateV4 &slate) const;
    quint64 resolveWorkflowAmountNano(const QString &workflowId,
                                      const QJsonObject &localContext,
                                      const QString &amount) const;
    static QJsonObject legacyInvoiceParticipantFromContext(const QJsonObject &localContext);
    static bool transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right);
    static bool walletOutputLessThan(const WalletOutput &left, const WalletOutput &right);
    void markTransactionBroadcastPending(const QString &workflowId);
    void markTransactionBroadcastFailed(const QString &workflowId, const QString &message);
    void markTransactionKernelConfirmed(const QString &workflowId, qulonglong confirmedHeight);
    void markTransactionKernelBroadcasted(const QString &workflowId);
    void markTransactionBroadcastRejected(const QString &workflowId, const QString &message);
    void markTransactionBroadcastSucceeded(const QString &workflowId);
    void onSessionLockTimeout();
    void onApplicationStateChanged(Qt::ApplicationState state);
    QJsonObject loadDocumentForService() const;
    bool saveDocumentForService(const QJsonObject &document) const;
    quint32 nextChildIndexFromStateForService(const QJsonObject &walletState) const;
    QJsonObject filterWorkflowContextsForTransactionsForService(const QJsonObject &contexts,
                                                               const QJsonArray &transactions) const;

    NodeForeignApi *m_nodeApi;
    GrinWalletNodeSyncService *m_nodeSyncService;
    GrinWalletWorkflowService *m_workflowService;
    GrinWalletShortcutBridge *m_shortcutBridge;
    QTimer *m_autoRefreshTimer;
    QTimer *m_sessionLockTimer;
    bool m_walletExists;
    bool m_walletUnlocked;
    QString m_walletName;
    QString m_sessionMnemonic;
    QString m_mnemonicPreview;
    QString m_seedFingerprint;
    QString m_selectedNetwork;
    QString m_nodeUrl;
    QString m_storagePersistenceState;
    qulonglong m_chainHeight;
    int m_nodeBlockHeaderVersion;
    QString m_syncStatus;
    QString m_totalBalance;
    QString m_spendableBalance;
    QString m_lockedBalance;
    QString m_immatureBalance;
    QString m_awaitingConfirmationBalance;
    QString m_awaitingFinalizationBalance;
    qulonglong m_scanHeight;
    QString m_lastError;
    QString m_lastInfo;
    QString m_workflowId;
    QString m_workflowState;
    QString m_workflowMode;
    QString m_workflowSlatepack;
    QString m_workflowDecoded;
    bool m_autoLockOnAppDeactivate;
    bool m_walletScanInFlight;
    bool m_seedScanActive;
    qulonglong m_seedScanNextIndex;
    QList<WalletOutput> m_seedScanDiscovered;
    QString m_pendingBroadcastWorkflowId;
    bool m_pendingBroadcastInputLookup;
    QJsonObject m_pendingBroadcastTxSkeleton;
    QJsonArray m_pendingBroadcastInputCommits;
    bool m_broadcastStatusRefreshInFlight;
    bool m_kernelStatusCheckInFlight;
    QList<QPair<QString, QString> > m_kernelStatusQueue;
    QString m_currentKernelWorkflowId;
    QString m_currentKernelExcess;
};

#endif // GRINWALLETCONTROLLER_H
