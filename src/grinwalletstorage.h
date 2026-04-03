#ifndef GRINWALLETSTORAGE_H
#define GRINWALLETSTORAGE_H

#include <QJsonObject>
#include <QString>

class GrinWalletStorage
{
public:
    struct LoadedState
    {
        bool walletExists{false};
        QString walletName;
        QString seedFingerprint;
        QString selectedNetwork;
        QString nodeUrl;
        bool autoLockOnDeactivate{false};
    };

    struct RefreshedState
    {
        QJsonObject document;
        qulonglong scanHeight{0};
        QString totalBalance;
        QString spendableBalance;
        QString lockedBalance;
        QString immatureBalance;
        QString awaitingConfirmationBalance;
        QString awaitingFinalizationBalance;
        bool balancesChanged{false};
    };

    static QJsonObject defaultWalletState();
    static QJsonObject defaultWalletMetadata();
    static QJsonObject defaultDocument();
    static QJsonObject walletForNetwork(const QJsonObject &document, const QString &networkName);
    static QJsonObject walletStateForNetwork(const QJsonObject &document, const QString &networkName);
    static QJsonObject workflowContextsForNetwork(const QJsonObject &document, const QString &networkName);
    static void setWalletForNetwork(QJsonObject *document, const QString &networkName, const QJsonObject &wallet);
    static void setWalletStateForNetwork(QJsonObject *document,
                                         const QString &networkName,
                                         const QJsonObject &walletState);
    static void setWorkflowContextsForNetwork(QJsonObject *document,
                                              const QString &networkName,
                                              const QJsonObject &contexts);
    static void syncActiveNetworkView(QJsonObject *document, const QString &networkName);
    static void persistActiveNetworkView(QJsonObject *document, const QString &networkName);
    static QJsonObject ensureDocumentSchema(const QJsonObject &rawDocument);
    static QJsonObject normalizeDocumentSchema(const QJsonObject &rawDocument);
    static QJsonObject extractImportedBackupDocument(const QByteArray &json, QString *errorOut);
    static QJsonObject loadDocument();
    static bool saveDocument(const QJsonObject &document);

    static LoadedState loadState(const QJsonObject &document);
    static RefreshedState refreshState(QJsonObject document, qulonglong chainHeight);
    static bool refreshTransactionConfirmations(QJsonObject *document, qulonglong chainHeight);
    static bool storeWorkflowContext(QJsonObject *document,
                                     const QString &workflowId,
                                     const QJsonObject &context);
    static QJsonObject workflowContext(const QJsonObject &document, const QString &workflowId);
};

#endif
