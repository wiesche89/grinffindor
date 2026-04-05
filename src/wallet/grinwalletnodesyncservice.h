#ifndef GRINWALLETNODESYNCSERVICE_H
#define GRINWALLETNODESYNCSERVICE_H

#include <QObject>
#include <memory>
#include <QJsonArray>
#include <QHash>
#include <QJsonObject>
#include <QString>

#include "locatedtxkernel.h"
#include "nodeversion.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "poolentry.h"
#include "result.h"
#include "tip.h"
#include "walletkeychain.h"
#include "walletoutput.h"

class GrinWalletController;
class NodeForeignApi;

class GrinWalletNodeSyncService : public QObject
{
    Q_OBJECT

public:
/**
 * @brief Manages node synchronization, scan orchestration, and broadcast status checks.
 */
    explicit GrinWalletNodeSyncService(GrinWalletController *controller);

/**
 * @brief Connects node client.
 */
    void connectNodeClient();
/**
 * @brief Requests wallet scan.
 */
    void requestWalletScan();
/**
 * @brief Starts seed scan.
 */
    void startSeedScan();
/**
 * @brief Finalizes the seed scan and persists resulting state.
 */
    void finishSeedScan(const QString &message);
/**
 * @brief Recovers pending broadcasts.
 */
    void recoverPendingBroadcasts();
/**
 * @brief Refreshes broadcast statuses.
 */
    void refreshBroadcastStatuses();
/**
 * @brief Starts next kernel status check.
 */
    void startNextKernelStatusCheck();
    void beginBroadcastWithInputPreflight(const QString &workflowId,
                                          const QJsonObject &txSkeleton);

private slots:
/**
 * @brief Handles completion of the node tip request.
 */
    void onNodeTipFinished(const Result<Tip> &result);
/**
 * @brief Handles completion of the node version request.
 */
    void onNodeVersionFinished(const Result<NodeVersion> &result);
/**
 * @brief Handles completion of the node outputs request.
 */
    void onNodeOutputsFinished(const Result<QList<OutputPrintable> > &result);
/**
 * @brief Handles completion of the output-commitment lookup request.
 */
    void onNodeOutputCommitmentsFinished(const Result<QList<OutputPrintable> > &result);
/**
 * @brief Handles completion of the unspent outputs request.
 */
    void onNodeUnspentOutputsFinished(const Result<OutputListing> &result);
/**
 * @brief Handles completion of the unconfirmed transaction request.
 */
    void onNodeUnconfirmedTransactionsFinished(const Result<QList<PoolEntry> > &result);
/**
 * @brief Handles completion of the kernel lookup request.
 */
    void onNodeKernelFinished(const Result<LocatedTxKernel> &result);
/**
 * @brief Handles completion of the node transaction-push request.
 */
    void onNodePushTransactionFinished(const Result<bool> &result);

private:
    struct FullRescanSessionState {
        QJsonObject document;
        QJsonObject walletState;
        QList<WalletOutput> tracked;
        QHash<QString, int> trackedIndexByCommitment;
        WalletKeychain keychain;
        quint32 nextChildIndex{0};
        QJsonArray transactionRescanBackup;
        bool rebuildTransactions{false};
        bool fullRescanMode{false};
        int batchesSinceCheckpoint{0};
    };

    static constexpr int kSeedScanBatchSize = 5000;
    static constexpr int kFullRescanBatchSize = 2000;
    static constexpr bool kFullRescanIncludeProof = true;
    static constexpr int kFullRescanCheckpointInterval = 5;
/**
 * @brief Clears pending broadcast state.
 */
    void clearPendingBroadcastState();
/**
 * @brief Marks a pending broadcast as failed and updates controller status.
 */
    void failPendingBroadcast(const QString &workflowId, const QString &message);
    bool ensureFullRescanSession(QString *errorMessage);
    void clearFullRescanSession();
    bool persistFullRescanSession(quint64 restoreLeafIndex, bool completed, QString *errorMessage);
    void abortFullRescan(const QString &message);
    void requestNextFullRescanBatch();

    GrinWalletController *m_controller;
    std::shared_ptr<FullRescanSessionState> m_fullRescanSession;
};

#endif
