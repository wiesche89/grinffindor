#ifndef GRINWALLETCONTROLLER_H
#define GRINWALLETCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariantList>
#include <functional>

#include "slatev4.h"
#include "walletcryptobackend.h"
#include "walletoutput.h"
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
    Q_PROPERTY(bool walletExists READ walletExists NOTIFY walletChanged)
    Q_PROPERTY(bool walletUnlocked READ walletUnlocked NOTIFY walletChanged)
    Q_PROPERTY(QString walletName READ walletName NOTIFY walletChanged)
    Q_PROPERTY(QString mnemonicPreview READ mnemonicPreview NOTIFY walletChanged)
    Q_PROPERTY(QString seedFingerprint READ seedFingerprint NOTIFY walletChanged)
    Q_PROPERTY(QString selectedNetwork READ selectedNetwork NOTIFY nodeConfigChanged)
    Q_PROPERTY(QString nodeUrl READ nodeUrl NOTIFY nodeConfigChanged)
   Q_PROPERTY(QString storagePersistenceState READ storagePersistenceState NOTIFY storageStateChanged)
   Q_PROPERTY(qulonglong chainHeight READ chainHeight NOTIFY nodeStatusChanged)
   Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY nodeStatusChanged)
   Q_PROPERTY(QString totalBalance READ totalBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString spendableBalance READ spendableBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString lockedBalance READ lockedBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString immatureBalance READ immatureBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString awaitingConfirmationBalance READ awaitingConfirmationBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString awaitingFinalizationBalance READ awaitingFinalizationBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(QString revertedBalance READ revertedBalance NOTIFY walletSummaryChanged)
   Q_PROPERTY(qulonglong scanHeight READ scanHeight NOTIFY walletSummaryChanged)
   Q_PROPERTY(bool fullRescanInFlight READ fullRescanInFlight NOTIFY nodeStatusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastInfo READ lastInfo NOTIFY lastInfoChanged)
    Q_PROPERTY(QString workflowId READ workflowId NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowState READ workflowState NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowMode READ workflowMode NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowSlatepack READ workflowSlatepack NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowDecoded READ workflowDecoded NOTIFY workflowChanged)
   Q_PROPERTY(bool autoLockOnAppDeactivate READ autoLockOnAppDeactivate WRITE setAutoLockOnAppDeactivate NOTIFY settingsChanged)
   Q_PROPERTY(int minimumConfirmations READ minimumConfirmations WRITE setMinimumConfirmations NOTIFY settingsChanged)
   Q_PROPERTY(QVariantList walletOutputs READ walletOutputs NOTIFY walletOutputsChanged)
   Q_PROPERTY(QVariantList transactionHistory READ transactionHistory NOTIFY transactionHistoryChanged)

public:
/**
 * @brief Coordinates wallet state, workflow orchestration, and UI-facing actions.
 */
    explicit GrinWalletController(QObject *parent = nullptr);

/**
 * @brief Returns whether a wallet is configured for the active network.
 */
    bool walletExists() const;
/**
 * @brief Returns whether the active wallet session is unlocked.
 */
    bool walletUnlocked() const;
/**
 * @brief Returns the active wallet display name.
 */
    QString walletName() const;
/**
 * @brief Returns the currently shown mnemonic preview text.
 */
    QString mnemonicPreview() const;
/**
 * @brief Returns the fingerprint of the active wallet seed.
 */
    QString seedFingerprint() const;
/**
 * @brief Returns the currently selected network name.
 */
    QString selectedNetwork() const;
/**
 * @brief Returns the configured node URL.
 */
    QString nodeUrl() const;
/**
 * @brief Returns storage persistence state.
 */
    QString storagePersistenceState() const;
/**
 * @brief Returns chain height.
 */
    qulonglong chainHeight() const;
/**
 * @brief Returns the current synchronization status text.
 */
    QString syncStatus() const;
/**
 * @brief Returns total balance.
 */
    QString totalBalance() const;
/**
 * @brief Returns spendable balance.
 */
    QString spendableBalance() const;
/**
 * @brief Returns the currently locked wallet balance.
 */
    QString lockedBalance() const;
/**
 * @brief Returns immature balance.
 */
    QString immatureBalance() const;
/**
 * @brief Returns awaiting confirmation balance.
 */
    QString awaitingConfirmationBalance() const;
/**
 * @brief Returns awaiting finalization balance.
 */
    QString awaitingFinalizationBalance() const;
/**
 * @brief Returns minimum confirmations required for spendable outputs.
 */
    int minimumConfirmations() const;
/**
 * @brief Returns reverted balance (outputs lost to chain reorg).
 */
    QString revertedBalance() const;
/**
 * @brief Returns scan height.
 */
    qulonglong scanHeight() const;
/**
 * @brief Returns whether a full wallet rescan is currently active.
 */
    bool fullRescanInFlight() const;
/**
 * @brief Returns last error.
 */
    QString lastError() const;
/**
 * @brief Returns last info.
 */
    QString lastInfo() const;
/**
 * @brief Returns workflow id.
 */
    QString workflowId() const;
/**
 * @brief Returns workflow state.
 */
    QString workflowState() const;
/**
 * @brief Returns workflow mode.
 */
    QString workflowMode() const;
/**
 * @brief Returns workflow slatepack.
 */
    QString workflowSlatepack() const;
/**
 * @brief Returns workflow decoded.
 */
    QString workflowDecoded() const;
/**
 * @brief Returns auto lock on app deactivate.
 */
    bool autoLockOnAppDeactivate() const;
/**
 * @brief Returns wallet outputs.
 */
    QVariantList walletOutputs() const;
/**
 * @brief Returns transaction history.
 */
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
    Q_INVOKABLE bool deleteWallet(const QString &password);
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
    Q_INVOKABLE void setMinimumConfirmations(int value);
    Q_INVOKABLE QString encodeSlatepack(const QString &slateJson, const QString &sender = QString()) const;
    Q_INVOKABLE QString decodeSlatepack(const QString &slatepack) const;

    // Internal service API kept explicit so workflow/node-sync services do not
    // require friend access to controller internals.
/**
 * @brief Returns derived session keychain.
 */
    WalletKeychain sessionKeychain() const;
/**
 * @brief Returns whether unlocked session.
 */
    bool hasUnlockedSession() const;
/**
 * @brief Returns node api.
 */
    NodeForeignApi *nodeApi() const;
/**
 * @brief Sets node api.
 */
    void setNodeApi(NodeForeignApi *nodeApi);
/**
 * @brief Sets sync status message.
 */
    void setSyncStatusMessage(const QString &status);
/**
 * @brief Processes notify status changed.
 */
    void notifyStatusChanged();
/**
 * @brief Sets chain height value.
 */
    void setChainHeightValue(qulonglong height);
/**
 * @brief Sets node block header version value.
 */
    void setNodeBlockHeaderVersionValue(int version);
/**
 * @brief Returns wallet scan in flight.
 */
    bool walletScanInFlight() const;
/**
 * @brief Sets wallet scan in flight.
 */
    void setWalletScanInFlight(bool inFlight);
/**
 * @brief Returns seed scan active.
 */
    bool seedScanActive() const;
/**
 * @brief Sets seed scan active.
 */
    void setSeedScanActive(bool active);
/**
 * @brief Sets full rescan in flight.
 */
    void setFullRescanInFlight(bool inFlight);
/**
 * @brief Returns seed scan next index.
 */
    qulonglong seedScanNextIndex() const;
/**
 * @brief Sets seed scan next index.
 */
    void setSeedScanNextIndex(qulonglong nextIndex);
    const QList<WalletOutput> &seedScanDiscovered() const;
/**
 * @brief Clears seed scan discovered.
 */
    void clearSeedScanDiscovered();
/**
 * @brief Returns append seed scan discovered.
 */
    void appendSeedScanDiscovered(const WalletOutput &output);
/**
 * @brief Returns pending broadcast workflow id.
 */
    QString pendingBroadcastWorkflowId() const;
/**
 * @brief Sets pending broadcast workflow id.
 */
    void setPendingBroadcastWorkflowId(const QString &workflowId);
/**
 * @brief Returns pending broadcast input lookup.
 */
    bool pendingBroadcastInputLookup() const;
/**
 * @brief Sets pending broadcast input lookup.
 */
    void setPendingBroadcastInputLookup(bool pending);
/**
 * @brief Returns pending broadcast tx skeleton.
 */
    QJsonObject pendingBroadcastTxSkeleton() const;
/**
 * @brief Sets pending broadcast tx skeleton.
 */
    void setPendingBroadcastTxSkeleton(const QJsonObject &txSkeleton);
/**
 * @brief Returns pending broadcast input commits.
 */
    QJsonArray pendingBroadcastInputCommits() const;
/**
 * @brief Sets pending broadcast input commits.
 */
    void setPendingBroadcastInputCommits(const QJsonArray &commits);
/**
 * @brief Returns whether pending broadcast workflow.
 */
    bool hasPendingBroadcastWorkflow() const;
/**
 * @brief Returns broadcast status refresh in flight.
 */
    bool broadcastStatusRefreshInFlight() const;
/**
 * @brief Sets broadcast status refresh in flight.
 */
    void setBroadcastStatusRefreshInFlight(bool inFlight);
/**
 * @brief Returns kernel status check in flight.
 */
    bool kernelStatusCheckInFlight() const;
/**
 * @brief Sets kernel status check in flight.
 */
    void setKernelStatusCheckInFlight(bool inFlight);
/**
 * @brief Clears kernel status queue.
 */
    void clearKernelStatusQueue();
/**
 * @brief Returns whether pending kernel status checks.
 */
    bool hasPendingKernelStatusChecks() const;
/**
 * @brief Returns append kernel status check.
 */
    void appendKernelStatusCheck(const QString &workflowId, const QString &excess);
    QPair<QString, QString> takeNextKernelStatusCheck();
/**
 * @brief Returns current kernel workflow id internal.
 */
    QString currentKernelWorkflowIdInternal() const;
/**
 * @brief Returns current kernel excess internal.
 */
    QString currentKernelExcessInternal() const;
/**
 * @brief Sets current kernel check.
 */
    void setCurrentKernelCheck(const QString &workflowId, const QString &excess);
/**
 * @brief Clears current kernel check.
 */
    void clearCurrentKernelCheck();
/**
 * @brief Sets last error.
 */
    void setLastError(const QString &error);
/**
 * @brief Sets last info.
 */
    void setLastInfo(const QString &info);
/**
 * @brief Connects node client.
 */
    void connectNodeClient();
/**
 * @brief Refreshes state from storage.
 */
    void refreshStateFromStorage();
/**
 * @brief Sets workflow.
 */
    void setWorkflow(const QString &id, const QString &mode, const QString &state, const QString &slatepack, const QString &decoded);
/**
 * @brief Returns store owned output.
 */
    void storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit);
/**
 * @brief Returns store owned output.
 */
    void storeOwnedOutput(const WalletOutput &output);
    bool buildOwnedOutput(const QString &source,
                          const QString &amount,
                          WalletOutput *outputOut,
                          SlateV4::Commit *commitOut,
                          QString *errorOut = 0) const;
/**
 * @brief Persists workflow transaction.
 */
    void persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted);
/**
 * @brief Finalizes workflow outputs.
 */
    void finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted);
/**
 * @brief Returns store workflow context.
 */
    void storeWorkflowContext(const QString &workflowId, const QJsonObject &context);
/**
 * @brief Returns workflow context.
 */
    QJsonObject workflowContext(const QString &workflowId) const;
/**
 * @brief Requests wallet scan.
 */
    void requestWalletScan();
/**
 * @brief Starts seed scan.
 */
    void startSeedScan();
/**
 * @brief Finalizes the seed scan workflow and stores completion state.
 */
    void finishSeedScan(const QString &message);
/**
 * @brief Refreshes broadcast statuses.
 */
    void refreshBroadcastStatuses();
/**
 * @brief Starts next kernel status check.
 */
    void startNextKernelStatusCheck();
/**
 * @brief Returns kernel excess from entry.
 */
    QString kernelExcessFromEntry(const QJsonObject &entry) const;
/**
 * @brief Refreshes transaction confirmations.
 */
    void refreshTransactionConfirmations();
/**
 * @brief Processes touch wallet session.
 */
    void touchWalletSession();
/**
 * @brief Recovers pending broadcasts.
 */
    void recoverPendingBroadcasts();
/**
 * @brief Finalizes broadcasted workflow.
 */
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
/**
 * @brief Returns compact invoice slate for return.
 */
    void compactInvoiceSlateForReturn(const QString &workflowId, SlateV4 *slate);
/**
 * @brief Returns compact standard slate for return.
 */
    void compactStandardSlateForReturn(const QString &workflowId, SlateV4 *slate);
/**
 * @brief Returns current slatepack address.
 */
    QString currentSlatepackAddress() const;
/**
 * @brief Returns current payment proof address.
 */
    QString currentPaymentProofAddress() const;
/**
 * @brief Returns current slatepack secret.
 */
    QByteArray currentSlatepackSecret() const;
/**
 * @brief Returns align slate version with node.
 */
    void alignSlateVersionWithNode(SlateV4 *slate) const;
    void beginBroadcastWithInputPreflight(const QString &workflowId,
                                          const QJsonObject &txSkeleton);
/**
 * @brief Returns resolve workflow id by slate id.
 */
    QString resolveWorkflowIdBySlateId(const SlateV4 &slate) const;
/**
 * @brief Returns resolved network name.
 */
    QString resolvedNetworkName() const;
/**
 * @brief Marks transaction broadcast pending.
 */
    void markTransactionBroadcastPending(const QString &workflowId);
/**
 * @brief Marks transaction broadcast failed.
 */
    void markTransactionBroadcastFailed(const QString &workflowId, const QString &message);
/**
 * @brief Marks transaction kernel confirmed.
 */
    void markTransactionKernelConfirmed(const QString &workflowId, qulonglong confirmedHeight);
/**
 * @brief Marks transaction kernel broadcasted.
 */
    void markTransactionKernelBroadcasted(const QString &workflowId);
/**
 * @brief Marks transaction broadcast rejected.
 */
    void markTransactionBroadcastRejected(const QString &workflowId, const QString &message);
/**
 * @brief Marks transaction broadcast succeeded.
 */
    void markTransactionBroadcastSucceeded(const QString &workflowId);
/**
 * @brief Loads document for service.
 */
    QJsonObject loadDocumentForService() const;
/**
 * @brief Saves document for service.
 */
    bool saveDocumentForService(const QJsonObject &document) const;
    bool updateDocumentForService(const std::function<bool(QJsonObject *)> &updater) const;
/**
 * @brief Returns next child index from state for service.
 */
    quint32 nextChildIndexFromStateForService(const QJsonObject &walletState) const;
/**
 * @brief Resumes a persisted full rescan without resetting restore_leaf_index.
 * @return True when a pending full rescan was detected.
 */
    bool resumePendingFullRescan();
    QJsonObject filterWorkflowContextsForTransactionsForService(const QJsonObject &contexts,
                                                               const QJsonArray &transactions) const;
signals:
/**
 * @brief Processes wallet changed.
 */
    void walletChanged();
/**
 * @brief Processes node config changed.
 */
    void nodeConfigChanged();
/**
 * @brief Processes status changed.
 */
    void statusChanged();
/**
 * @brief Processes node status changed.
 */
    void nodeStatusChanged();
/**
 * @brief Processes wallet summary changed.
 */
    void walletSummaryChanged();
/**
 * @brief Processes storage state changed.
 */
    void storageStateChanged();
/**
 * @brief Processes settings changed.
 */
    void settingsChanged();
/**
 * @brief Processes last error changed.
 */
    void lastErrorChanged();
/**
 * @brief Processes last info changed.
 */
    void lastInfoChanged();
/**
 * @brief Processes workflow changed.
 */
    void workflowChanged();
/**
 * @brief Processes wallet outputs changed.
 */
    void walletOutputsChanged();
/**
 * @brief Processes transaction history changed.
 */
    void transactionHistoryChanged();

private:
/**
 * @brief Loads from storage.
 */
    void loadFromStorage();
/**
 * @brief Starts auto refresh.
 */
    void startAutoRefresh();
/**
 * @brief Finalizes transaction store update.
 */
    void finalizeTransactionStoreUpdate(const QJsonObject &document, bool changed);
   QVariantList buildWalletOutputs() const;
   QVariantList buildTransactionHistory() const;
   void refreshDerivedModels();
    void storeOutputsState(QJsonObject *document,
                           QJsonObject *walletState,
                           const QList<WalletOutput> &outputs,
                           quint32 nextChildIndex) const;
/**
 * @brief Updates transaction entry.
 */
    void updateTransactionEntry(const QString &workflowId, const std::function<void(QJsonObject &)> &updater);
/**
 * @brief Refreshes storage persistence state.
 */
    void refreshStoragePersistenceState();
    quint64 resolveWorkflowAmountNano(const QString &workflowId,
                                      const QJsonObject &localContext,
                                      const QString &amount) const;
/**
 * @brief Returns legacy invoice participant from context.
 */
    static QJsonObject legacyInvoiceParticipantFromContext(const QJsonObject &localContext);
/**
 * @brief Returns transaction entry less than.
 */
    static bool transactionEntryLessThan(const QJsonObject &left, const QJsonObject &right);
/**
 * @brief Returns wallet output less than.
 */
    static bool walletOutputLessThan(const WalletOutput &left, const WalletOutput &right);
/**
 * @brief Processes on session lock timeout.
 */
    void onSessionLockTimeout();
/**
 * @brief Processes on application state changed.
 */
    void onApplicationStateChanged(Qt::ApplicationState state);

    NodeForeignApi *m_nodeApi;
    GrinWalletNodeSyncService *m_nodeSyncService;
    GrinWalletWorkflowService *m_workflowService;
    GrinWalletShortcutBridge *m_shortcutBridge;
    QTimer *m_autoRefreshTimer;
    QTimer *m_sessionLockTimer;
   bool m_initialized;
    bool m_walletExists;
    bool m_walletUnlocked;
    QString m_walletName;
    QByteArray m_sessionMnemonicBytes;
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
    QString m_revertedBalance;
    qulonglong m_minimumConfirmations;
    qulonglong m_scanHeight;
    QString m_lastError;
    QString m_lastInfo;
    QString m_workflowId;
    QString m_workflowState;
    QString m_workflowMode;
    QString m_workflowSlatepack;
    QString m_workflowDecoded;
   QVariantList m_walletOutputsCache;
   QVariantList m_transactionHistoryCache;
    bool m_autoLockOnAppDeactivate;
    bool m_walletScanInFlight;
    bool m_seedScanActive;
   bool m_fullRescanInFlight;
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
