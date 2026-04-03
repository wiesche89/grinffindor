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
    static QByteArray serializeTransactionForPoolV1(const Transaction &tx, QString *errorOut);
    static QList<QUrl> poolPushCandidateUrlsForApiUrl(const QString &apiUrl, bool fluff);
    static QJsonObject serializeTransactionForNode(const Transaction &tx);
    static QJsonObject serializeTransactionForNodeLegacyKernel(const Transaction &tx);
};

#endif
