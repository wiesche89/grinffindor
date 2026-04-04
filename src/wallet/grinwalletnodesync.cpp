#include "grinwalletnodesync.h"

#include <QJsonArray>

/**
 * @brief GrinWalletNodeSync::hasRecoverableBroadcasts
 * @param document
 * @return
 */
bool GrinWalletNodeSync::hasRecoverableBroadcasts(const QJsonObject &document)
{
    const QJsonArray transactions = document
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))

                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QString status = transactions.at(i).toObject().value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("broadcast_pending")
            || status == QStringLiteral("broadcasted")
            || status == QStringLiteral("in_mempool")) {
            return true;
        }
    }

    return false;
}

/**
 * @brief GrinWalletNodeSync::shouldRefreshBroadcastStatuses
 * @param document
 * @return
 */
bool GrinWalletNodeSync::shouldRefreshBroadcastStatuses(const QJsonObject &document)
{
    const QJsonArray transactions = document
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))

                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        if (!entry.value(QStringLiteral("broadcasted")).toBool()
            && status != QStringLiteral("broadcast_pending")) {
            continue;
        }
        if (status != QStringLiteral("confirmed") && status != QStringLiteral("cancelled")) {
            return true;
        }
    }

    return false;
}

/**
 * @brief GrinWalletNodeSync::buildSeedScanState
 * @param walletState
 * @return
 */
GrinWalletNodeSync::SeedScanState GrinWalletNodeSync::buildSeedScanState(const QJsonObject &walletState)
{
    SeedScanState state;
    state.nextIndex = qMax<qulonglong>(
        1,
        walletState.value(QStringLiteral("restore_leaf_index")).toVariant().toULongLong() + 1);
    state.syncStatus = QStringLiteral("Seed scan 0% (starting at leaf %1)").arg(QString::number(state.nextIndex));
    return state;
}
