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

    static bool hasRecoverableBroadcasts(const QJsonObject &document);
    static bool shouldRefreshBroadcastStatuses(const QJsonObject &document);
    static SeedScanState buildSeedScanState(const QJsonObject &walletState);
};

#endif
