#ifndef GRINWALLETCONTROLLER_H
#define GRINWALLETCONTROLLER_H

#include <QObject>
#include <QJsonObject>
#include <QUrl>
#include <QString>
#include <QVariantList>
#include <functional>

#include "wallet/slatev4.h"
#include "wallet/walletoutput.h"

class NodeForeignApi;
class QTimer;

class GrinWalletController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool walletExists READ walletExists NOTIFY walletChanged)
    Q_PROPERTY(bool walletUnlocked READ walletUnlocked NOTIFY walletChanged)
    Q_PROPERTY(QString walletName READ walletName NOTIFY walletChanged)
    Q_PROPERTY(QString mnemonicPreview READ mnemonicPreview NOTIFY walletChanged)
    Q_PROPERTY(QString seedFingerprint READ seedFingerprint NOTIFY walletChanged)
    Q_PROPERTY(QString nodeUrl READ nodeUrl NOTIFY nodeConfigChanged)
    Q_PROPERTY(qulonglong chainHeight READ chainHeight NOTIFY statusChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY statusChanged)
    Q_PROPERTY(QString totalBalance READ totalBalance NOTIFY statusChanged)
    Q_PROPERTY(QString spendableBalance READ spendableBalance NOTIFY statusChanged)
    Q_PROPERTY(QString lockedBalance READ lockedBalance NOTIFY statusChanged)
    Q_PROPERTY(QString immatureBalance READ immatureBalance NOTIFY statusChanged)
    Q_PROPERTY(qulonglong scanHeight READ scanHeight NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastInfo READ lastInfo NOTIFY lastInfoChanged)
    Q_PROPERTY(QString workflowId READ workflowId NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowState READ workflowState NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowMode READ workflowMode NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowSlatepack READ workflowSlatepack NOTIFY workflowChanged)
    Q_PROPERTY(QString workflowDecoded READ workflowDecoded NOTIFY workflowChanged)
    Q_PROPERTY(QVariantList transactionHistory READ transactionHistory NOTIFY statusChanged)

public:
    explicit GrinWalletController(QObject *parent = nullptr);

    bool walletExists() const;
    bool walletUnlocked() const;
    QString walletName() const;
    QString mnemonicPreview() const;
    QString seedFingerprint() const;
    QString nodeUrl() const;
    qulonglong chainHeight() const;
    QString syncStatus() const;
    QString totalBalance() const;
    QString spendableBalance() const;
    QString lockedBalance() const;
    QString immatureBalance() const;
    qulonglong scanHeight() const;
    QString lastError() const;
    QString lastInfo() const;
    QString workflowId() const;
    QString workflowState() const;
    QString workflowMode() const;
    QString workflowSlatepack() const;
    QString workflowDecoded() const;
    QVariantList transactionHistory() const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE QString generateMnemonic() const;
    Q_INVOKABLE bool validateMnemonic(const QString &mnemonic) const;
    Q_INVOKABLE void createWallet(const QString &walletName, const QString &password);
    Q_INVOKABLE void importWallet(const QString &walletName, const QString &mnemonic, const QString &password);
    Q_INVOKABLE void restoreWallet(const QString &walletName, const QString &mnemonic, const QString &password);
    Q_INVOKABLE void unlockWallet(const QString &password);
    Q_INVOKABLE void lockWallet();
    Q_INVOKABLE void dismissMnemonicPreview();
    Q_INVOKABLE void deleteWallet();
    Q_INVOKABLE bool setNodeUrl(const QString &nodeUrl);
    Q_INVOKABLE void resetNodeUrl();
    Q_INVOKABLE void refreshNodeStatus();
    Q_INVOKABLE void syncWallet();
    Q_INVOKABLE void rescanWallet();
    Q_INVOKABLE QString requestPasteText() const;
    Q_INVOKABLE bool isValidNodeUrl(const QString &nodeUrl) const;
    Q_INVOKABLE QString createSlatepackTemplate(const QString &sender = QString()) const;
    Q_INVOKABLE void startSendWorkflow(const QString &amount, const QString &note = QString());
    Q_INVOKABLE void startReceiveWorkflow(const QString &amount, const QString &note = QString());
    Q_INVOKABLE void processWorkflowSlatepack(const QString &slatepack);
    Q_INVOKABLE void broadcastCurrentWorkflowTransaction();
    Q_INVOKABLE void broadcastTransaction(const QString &workflowId);
    Q_INVOKABLE void cancelTransaction(const QString &workflowId);
    Q_INVOKABLE void clearWorkflow();
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
    QString kernelExcessFromEntry(const QJsonObject &entry) const;
    void refreshTransactionConfirmations();
    QJsonArray rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs) const;

    NodeForeignApi *m_nodeApi;
    QTimer *m_autoRefreshTimer;
    bool m_walletExists;
    bool m_walletUnlocked;
    QString m_walletName;
    QString m_sessionMnemonic;
    QString m_mnemonicPreview;
    QString m_seedFingerprint;
    QString m_nodeUrl;
    qulonglong m_chainHeight;
    QString m_syncStatus;
    QString m_totalBalance;
    QString m_spendableBalance;
    QString m_lockedBalance;
    QString m_immatureBalance;
    qulonglong m_scanHeight;
    QString m_lastError;
    QString m_lastInfo;
    QString m_workflowId;
    QString m_workflowState;
    QString m_workflowMode;
    QString m_workflowSlatepack;
    QString m_workflowDecoded;
    bool m_walletScanInFlight;
    bool m_seedScanActive;
    qulonglong m_seedScanNextIndex;
    QList<WalletOutput> m_seedScanDiscovered;
    QString m_pendingBroadcastWorkflowId;
    bool m_broadcastStatusRefreshInFlight;
    bool m_kernelStatusCheckInFlight;
    QList<QPair<QString, QString> > m_kernelStatusQueue;
    QString m_currentKernelWorkflowId;
    QString m_currentKernelExcess;
};

#endif // GRINWALLETCONTROLLER_H
