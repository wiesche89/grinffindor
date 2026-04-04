#ifndef GRINWALLETNODEPUSHHELPERS_H
#define GRINWALLETNODEPUSHHELPERS_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QUrl>

#include "transaction.h"

class GrinWalletNodePushHelpers
{
public:
/**
 * @brief Returns serialize transaction for pool v1.
 */
    static QByteArray serializeTransactionForPoolV1(const Transaction &tx, QString *errorOut);
/**
 * @brief Returns pool push candidate urls for api url.
 */
    static QList<QUrl> poolPushCandidateUrlsForApiUrl(const QString &apiUrl, bool fluff);
/**
 * @brief Returns serialize transaction for node.
 */
    static QJsonObject serializeTransactionForNode(const Transaction &tx);
/**
 * @brief Returns serialize transaction for node legacy kernel.
 */
    static QJsonObject serializeTransactionForNodeLegacyKernel(const Transaction &tx);
};

#endif
