#include "grinwalletcontrollerhelpers.h"

#include <QUrl>

#include "grinwalletworkflowhelpers.h"

namespace {

const char *kMainnetNodeUrl = "https://mainnet.grinffindor.org/v2/foreign";
const char *kTestnetNodeUrl = "https://testnet.grinffindor.org/v2/foreign";

}

/**
 * @brief GrinWalletControllerHelpers::defaultNetworkName
 * @return
 */
QString GrinWalletControllerHelpers::defaultNetworkName()
{
    return QStringLiteral("mainnet");
}

/**
 * @brief GrinWalletControllerHelpers::isAcceptedNetworkName
 * @param networkName
 * @return
 */
bool GrinWalletControllerHelpers::isAcceptedNetworkName(const QString &networkName)
{
    const QString normalized = networkName.trimmed().toLower();
    return normalized == QStringLiteral("mainnet") || normalized == QStringLiteral("testnet");
}

/**
 * @brief GrinWalletControllerHelpers::defaultNodeUrlForNetwork
 * @param networkName
 * @return
 */
QString GrinWalletControllerHelpers::defaultNodeUrlForNetwork(const QString &networkName)
{
    return networkName.trimmed().toLower() == QStringLiteral("testnet")
        ? QString::fromUtf8(kTestnetNodeUrl)
        : QString::fromUtf8(kMainnetNodeUrl);
}

/**
 * @brief GrinWalletControllerHelpers::inferNetworkName
 * @param networkName
 * @param nodeUrl
 * @return
 */
QString GrinWalletControllerHelpers::inferNetworkName(const QString &networkName, const QString &nodeUrl)
{

    const QString normalizedNetwork = networkName.trimmed().toLower();
    if (isAcceptedNetworkName(normalizedNetwork)) {
        return normalizedNetwork;
    }

    const QString normalizedUrl = nodeUrl.trimmed().toLower();
    if (normalizedUrl.contains(QStringLiteral("testnet."))) {
        return QStringLiteral("testnet");
    }

    return defaultNetworkName();
}

/**
 * @brief GrinWalletControllerHelpers::isNodeUrlAccepted
 * @param nodeUrl
 * @return
 */
bool GrinWalletControllerHelpers::isNodeUrlAccepted(const QString &nodeUrl)
{
    const QUrl parsed = QUrl::fromUserInput(nodeUrl.trimmed());
    return parsed.isValid()
        && !parsed.scheme().trimmed().isEmpty()
        && !parsed.host().trimmed().isEmpty()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"));
}

bool GrinWalletControllerHelpers::isBuiltInProjectNode(const QString &nodeUrl)
{
    const QString normalized = QUrl::fromUserInput(nodeUrl.trimmed()).toString(QUrl::RemoveFragment | QUrl::RemoveQuery);
    return normalized == defaultNodeUrlForNetwork(QStringLiteral("mainnet"))
        || normalized == defaultNodeUrlForNetwork(QStringLiteral("testnet"));
}

/**
 * @brief GrinWalletControllerHelpers::isFinalTransactionStatus
 * @param status
 * @return
 */
bool GrinWalletControllerHelpers::isFinalTransactionStatus(const QString &status)
{
    return status == QStringLiteral("confirmed")
        || status == QStringLiteral("cancelled")
        || status == QStringLiteral("spent");
}

/**
 * @brief GrinWalletControllerHelpers::findTrackedOutputByCommitment
 * @param outputs
 * @param commitment
 * @return
 */
WalletOutput GrinWalletControllerHelpers::findTrackedOutputByCommitment(const QList<WalletOutput> &outputs,
                                                                        const QString &commitment)
{
    return GrinWalletWorkflowHelpers::findTrackedOutputByCommitment(outputs, commitment);
}

/**
 * @brief GrinWalletControllerHelpers::normalizedTrackedOutput
 * @param output
 * @param keychain
 * @return
 */
WalletOutput GrinWalletControllerHelpers::normalizedTrackedOutput(const WalletOutput &output,
                                                                  const WalletKeychain &keychain)
{
    return GrinWalletWorkflowHelpers::normalizedTrackedOutput(output, keychain);
}

/**
 * @brief GrinWalletControllerHelpers::displayAmountForTransactionEntry
 * @param entry
 * @param outputs
 * @return
 */
QString GrinWalletControllerHelpers::displayAmountForTransactionEntry(const QJsonObject &entry,
                                                                      const QList<WalletOutput> &outputs)
{

    const QString storedAmount = entry.value(QStringLiteral("amount")).toString().trimmed();
    if (!storedAmount.isEmpty()) {
        return storedAmount;
    }

    const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
    quint64 receivedAmount = 0;
    quint64 changeAmount = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (output.workflowId != workflowId) {
            continue;
        }
        const quint64 amount = GrinWalletWorkflowHelpers::amountToNanogrin(output.amount);
        if (output.source == QStringLiteral("change")) {
            changeAmount += amount;
        } else if (output.source == QStringLiteral("receive") || output.source == QStringLiteral("invoice")) {
            receivedAmount += amount;
        }
    }

    if (receivedAmount > 0) {
        return GrinWalletWorkflowHelpers::formatNanogrin(receivedAmount);
    }

    quint64 inputAmount = 0;
    const QJsonArray inputs = entry.value(QStringLiteral("tx_skeleton"))
                                  .toObject()
                                  .value(QStringLiteral("body"))
                                  .toObject()
                                  .value(QStringLiteral("inputs"))

                                  .toArray();
    for (int i = 0; i < inputs.size(); ++i) {
        const QJsonObject input = inputs.at(i).toObject();
        QString commitment = input.value(QStringLiteral("commit")).toString();
        if (commitment.isEmpty()) {
            commitment = input.value(QStringLiteral("commit")).toObject().value(QStringLiteral("hex")).toString();
        }
        if (commitment.isEmpty()) {
            continue;
        }
        inputAmount += GrinWalletWorkflowHelpers::amountToNanogrin(
            findTrackedOutputByCommitment(outputs, commitment).amount);
    }

    const quint64 feeAmount = GrinWalletWorkflowHelpers::amountToNanogrin(entry.value(QStringLiteral("fee")).toString());
    if (inputAmount > 0 && inputAmount >= changeAmount + feeAmount) {
        return GrinWalletWorkflowHelpers::formatNanogrin(inputAmount - changeAmount - feeAmount);
    }

    return QString();
}

/**
 * @brief GrinWalletControllerHelpers::filterWorkflowContextsForTransactions
 * @param contexts
 * @param transactions
 * @return
 */
QJsonObject GrinWalletControllerHelpers::filterWorkflowContextsForTransactions(const QJsonObject &contexts,
                                                                               const QJsonArray &transactions)
{
    QJsonObject filtered;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        const QString status = entry.value(QStringLiteral("status")).toString();
        if (workflowId.isEmpty() || isFinalTransactionStatus(status)) {
            continue;
        }
        if (contexts.contains(workflowId)) {
            filtered.insert(workflowId, contexts.value(workflowId));
        }
    }
    return filtered;
}
