#ifndef GRINWALLETWORKFLOW_H
#define GRINWALLETWORKFLOW_H

#include <QJsonObject>

#include "slatev4.h"

class GrinWalletWorkflow
{
public:
/**
 * @brief Persists transaction.
 */
    static bool persistTransaction(QJsonObject *document, const SlateV4 &slate, bool broadcasted);
    static bool finalizeOutputs(QJsonObject *document,
                                const SlateV4 &slate,
                                bool broadcasted,
                                qulonglong chainHeight,
                                const QJsonObject &localContext);
    static bool finalizeBroadcastedWorkflow(QJsonObject *document,
                                            const QString &workflowId,
                                            qulonglong chainHeight,
                                            const QJsonObject &localContext);
};

#endif
