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
        qulonglong minimumConfirmations{10};
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
        QString revertedBalance;
        bool documentChanged{false};
        bool balancesChanged{false};
    };

/**
 * @brief Processes default wallet state.
 */
    static QJsonObject defaultWalletState();
/**
 * @brief Processes default wallet metadata.
 */
    static QJsonObject defaultWalletMetadata();
/**
 * @brief Processes default document.
 */
    static QJsonObject defaultDocument();
/**
 * @brief Returns wallet for network.
 */
    static QJsonObject walletForNetwork(const QJsonObject &document, const QString &networkName);
/**
 * @brief Returns wallet state for network.
 */
    static QJsonObject walletStateForNetwork(const QJsonObject &document, const QString &networkName);
/**
 * @brief Returns workflow contexts for network.
 */
    static QJsonObject workflowContextsForNetwork(const QJsonObject &document, const QString &networkName);
/**
 * @brief Sets wallet for network.
 */
    static void setWalletForNetwork(QJsonObject *document, const QString &networkName, const QJsonObject &wallet);
    static void setWalletStateForNetwork(QJsonObject *document,
                                         const QString &networkName,
                                         const QJsonObject &walletState);
    static void setWorkflowContextsForNetwork(QJsonObject *document,
                                              const QString &networkName,
                                              const QJsonObject &contexts);
/**
 * @brief Returns sync active network view.
 */
    static void syncActiveNetworkView(QJsonObject *document, const QString &networkName);
/**
 * @brief Persists active network view.
 */
    static void persistActiveNetworkView(QJsonObject *document, const QString &networkName);
/**
 * @brief Returns ensure document schema.
 */
    static QJsonObject ensureDocumentSchema(const QJsonObject &rawDocument);
/**
 * @brief Returns normalize document schema.
 */
    static QJsonObject normalizeDocumentSchema(const QJsonObject &rawDocument);
/**
 * @brief Loads document.
 */
    static QJsonObject loadDocument();
/**
 * @brief Saves document.
 */
    static bool saveDocument(const QJsonObject &document);

/**
 * @brief Loads state.
 */
    static LoadedState loadState(const QJsonObject &document);
/**
 * @brief Refreshes state.
 */
    static RefreshedState refreshState(QJsonObject document, qulonglong chainHeight);
/**
 * @brief Refreshes transaction confirmations.
 */
    static bool refreshTransactionConfirmations(QJsonObject *document, qulonglong chainHeight);
    static bool storeWorkflowContext(QJsonObject *document,
                                     const QString &workflowId,
                                     const QJsonObject &context);
/**
 * @brief Returns workflow context.
 */
    static QJsonObject workflowContext(const QJsonObject &document, const QString &workflowId);
};

#endif
