#ifndef NODEFOOTERSTATUS_H
#define NODEFOOTERSTATUS_H

#include <QObject>
#include <QTimer>

#include "nodeforeignapi.h"

/**
 * @brief Exposes periodic node connectivity and status information to QML.
 */
class NodeFooterStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mainnetAvailable READ mainnetAvailable NOTIFY dataChanged)
    Q_PROPERTY(QString mainnetTip READ mainnetTip NOTIFY dataChanged)
    Q_PROPERTY(QString mainnetVersion READ mainnetVersion NOTIFY dataChanged)
    Q_PROPERTY(bool testnetAvailable READ testnetAvailable NOTIFY dataChanged)
    Q_PROPERTY(QString testnetTip READ testnetTip NOTIFY dataChanged)
    Q_PROPERTY(QString testnetVersion READ testnetVersion NOTIFY dataChanged)

public:
    /**
     * @brief Constructs a node footer status provider.
     * @param parent Optional parent object.
     */
    explicit NodeFooterStatus(QObject *parent = nullptr);

    /**
     * @brief Indicates whether the mainnet node endpoint is reachable.
     * @return True when the mainnet endpoint is currently available.
     */
    bool mainnetAvailable() const;

    /**
     * @brief Returns the latest known mainnet tip representation.
     * @return Mainnet tip display text.
     */
    QString mainnetTip() const;

    /**
     * @brief Returns the latest known mainnet version representation.
     * @return Mainnet version display text.
     */
    QString mainnetVersion() const;

    /**
     * @brief Indicates whether the testnet node endpoint is reachable.
     * @return True when the testnet endpoint is currently available.
     */
    bool testnetAvailable() const;

    /**
     * @brief Returns the latest known testnet tip representation.
     * @return Testnet tip display text.
     */
    QString testnetTip() const;

    /**
     * @brief Returns the latest known testnet version representation.
     * @return Testnet version display text.
     */
    QString testnetVersion() const;

    /**
     * @brief Requests fresh tip and version data from configured node endpoints.
     */
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    /**
     * @brief Connects API completion signals to internal handlers.
     * @param api API instance whose signals should be connected.
     */
    void connectApi(NodeForeignApi *api);

    /**
     * @brief Updates availability state for the API associated with a network.
     * @param api API instance that produced the state.
     * @param available Availability flag to apply.
     */
    void updateApiAvailability(NodeForeignApi *api, bool available);

    /**
     * @brief Updates formatted tip text for the API associated with a network.
     * @param api API instance that produced the value.
     * @param tipText Pre-formatted tip text.
     */
    void updateApiTip(NodeForeignApi *api, const QString &tipText);

    /**
     * @brief Updates formatted version text for the API associated with a network.
     * @param api API instance that produced the value.
     * @param versionText Pre-formatted version text.
     */
    void updateApiVersion(NodeForeignApi *api, const QString &versionText);

    /**
     * @brief Handles completion of asynchronous tip queries.
     * @param result Tip query result payload.
     */
    void handleTipFinished(const Result<Tip> &result);

    /**
     * @brief Handles completion of asynchronous version queries.
     * @param result Version query result payload.
     */
    void handleVersionFinished(const Result<NodeVersion> &result);

    /**
     * @brief Converts a tip model to displayable text.
     * @param tip Tip model received from a node.
     * @return Formatted tip text.
     */
    static QString formatTip(const Tip &tip);

    /**
     * @brief Converts a node version model to displayable text.
     * @param version Node version model received from a node.
     * @return Formatted version text.
     */
    static QString formatVersion(const NodeVersion &version);

    bool m_mainnetAvailable{false};
    QString m_mainnetTip{"..."};
    QString m_mainnetVersion{"..."};
    bool m_testnetAvailable{false};
    QString m_testnetTip{"..."};
    QString m_testnetVersion{"..."};

    NodeForeignApi m_mainnetApi;
    NodeForeignApi m_testnetApi;
    QTimer m_refreshTimer;
};

#endif
