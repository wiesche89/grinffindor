#ifndef GRINWALLETNODESYNCSERVICE_H
#define GRINWALLETNODESYNCSERVICE_H

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "locatedtxkernel.h"
#include "nodeversion.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "poolentry.h"
#include "result.h"
#include "tip.h"

class GrinWalletController;
class NodeForeignApi;

class GrinWalletNodeSyncService : public QObject
{
    Q_OBJECT

public:
    explicit GrinWalletNodeSyncService(GrinWalletController *controller);

    void connectNodeClient();
    void requestWalletScan();
    void startSeedScan();
    void finishSeedScan(const QString &message);
    void recoverPendingBroadcasts();
    void refreshBroadcastStatuses();
    void startNextKernelStatusCheck();
    void beginBroadcastWithInputPreflight(const QString &workflowId,
                                          const QJsonObject &txSkeleton);

private slots:
    void onNodeTipFinished(const Result<Tip> &result);
    void onNodeVersionFinished(const Result<NodeVersion> &result);
    void onNodeOutputsFinished(const Result<QList<OutputPrintable> > &result);
    void onNodeOutputCommitmentsFinished(const Result<QList<OutputPrintable> > &result);
    void onNodeUnspentOutputsFinished(const Result<OutputListing> &result);
    void onNodeUnconfirmedTransactionsFinished(const Result<QList<PoolEntry> > &result);
    void onNodeKernelFinished(const Result<LocatedTxKernel> &result);
    void onNodePushTransactionFinished(const Result<bool> &result);

private:
    GrinWalletController *m_controller;
};

#endif
