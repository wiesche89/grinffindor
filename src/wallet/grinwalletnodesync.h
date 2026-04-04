#ifndef GRINWALLETNODESYNC_H
#define GRINWALLETNODESYNC_H

#include <QJsonObject>
#include <QString>

class GrinWalletNodeSync
{
public:
    struct SeedScanState
    {
        qulonglong nextIndex{1};
        QString syncStatus;
    };

/**
 * @brief Returns whether recoverable broadcasts.
 */
    static bool hasRecoverableBroadcasts(const QJsonObject &document);
/**
 * @brief Returns whether refresh broadcast statuses.
 */
    static bool shouldRefreshBroadcastStatuses(const QJsonObject &document);
/**
 * @brief Builds seed scan state.
 */
    static SeedScanState buildSeedScanState(const QJsonObject &walletState);
};

#endif
