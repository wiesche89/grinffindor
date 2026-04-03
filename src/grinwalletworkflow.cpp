#include "grinwalletworkflow.h"

#include <QDateTime>
#include <QJsonArray>

#include "wallet/walletscanner.h"

bool GrinWalletWorkflow::persistTransaction(QJsonObject *document, const SlateV4 &slate, bool broadcasted)
{
    if (!document || slate.workflowId().isEmpty()) {
        return false;
    }

    QJsonObject walletState = document->value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    QJsonObject existingEntry;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject candidate = transactions.at(i).toObject();
        if (candidate.value(QStringLiteral("workflow_id")).toString() == slate.workflowId()) {
            existingEntry = candidate;
            break;
        }
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("workflow_id"), slate.workflowId());
    entry.insert(QStringLiteral("mode"), slate.modeCode());
    entry.insert(QStringLiteral("state"), slate.stateCode());
    entry.insert(QStringLiteral("amount"),
                 !slate.amount.trimmed().isEmpty()
                     ? slate.amount
                     : existingEntry.value(QStringLiteral("amount")).toString());
    entry.insert(QStringLiteral("fee"), slate.fee);
    entry.insert(QStringLiteral("slate_id"), slate.id);
    entry.insert(QStringLiteral("offset"), slate.offset);
    entry.insert(QStringLiteral("broadcasted"),
                 broadcasted || existingEntry.value(QStringLiteral("broadcasted")).toBool());
    entry.insert(QStringLiteral("timestamp"),
                 existingEntry.value(QStringLiteral("timestamp")).toString().isEmpty()
                    ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
                    : existingEntry.value(QStringLiteral("timestamp")).toString());
    entry.insert(QStringLiteral("tx_ready"), slate.metadata.value(QStringLiteral("tx_ready")).toBool());
    entry.insert(QStringLiteral("confirmations"), existingEntry.value(QStringLiteral("confirmations")).toInt());
    entry.insert(QStringLiteral("kernel_excess"),
                 !slate.metadata.value(QStringLiteral("pubkey_total")).toString().isEmpty()
                     ? slate.metadata.value(QStringLiteral("pubkey_total")).toString()
                     : existingEntry.value(QStringLiteral("kernel_excess")).toString());
    entry.insert(QStringLiteral("kernel_signature"),
                 !slate.metadata.value(QStringLiteral("final_sig")).toString().isEmpty()
                     ? slate.metadata.value(QStringLiteral("final_sig")).toString()
                     : existingEntry.value(QStringLiteral("kernel_signature")).toString());
    entry.insert(QStringLiteral("status"),
                 broadcasted ? QStringLiteral("broadcasted")
                             : (slate.metadata.value(QStringLiteral("tx_ready")).toBool()
                                    ? QStringLiteral("ready")
                                    : existingEntry.value(QStringLiteral("status")).toString().isEmpty()
                                        ? QStringLiteral("in_progress")
                                        : existingEntry.value(QStringLiteral("status")).toString()));
    if (existingEntry.contains(QStringLiteral("broadcast_at"))) {
        entry.insert(QStringLiteral("broadcast_at"), existingEntry.value(QStringLiteral("broadcast_at")).toString());
    }
    if (existingEntry.contains(QStringLiteral("broadcast_error"))) {
        entry.insert(QStringLiteral("broadcast_error"), existingEntry.value(QStringLiteral("broadcast_error")).toString());
    }
    if (existingEntry.contains(QStringLiteral("confirmed_height"))) {
        entry.insert(QStringLiteral("confirmed_height"), existingEntry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong());
    }
    if (existingEntry.contains(QStringLiteral("last_node_check"))) {
        entry.insert(QStringLiteral("last_node_check"), existingEntry.value(QStringLiteral("last_node_check")).toString());
    }
    if (existingEntry.contains(QStringLiteral("output_commitments"))) {
        entry.insert(QStringLiteral("output_commitments"), existingEntry.value(QStringLiteral("output_commitments")).toArray());
    }
    if (existingEntry.contains(QStringLiteral("rescan_rebuilt"))) {
        entry.insert(QStringLiteral("rescan_rebuilt"), existingEntry.value(QStringLiteral("rescan_rebuilt")).toBool());
    }
    if (slate.hasPaymentProof) {
        entry.insert(QStringLiteral("payment_proof"), slate.paymentProof.toJson());
        const QString metadataStatus = slate.metadata.value(QStringLiteral("payment_proof_status")).toString();
        entry.insert(QStringLiteral("payment_proof_status"),
                     !metadataStatus.isEmpty()
                         ? metadataStatus
                         : (slate.paymentProof.receiverSignature.isEmpty()
                                ? QStringLiteral("pending")
                                : QStringLiteral("receiver_signed")));
        if (slate.metadata.contains(QStringLiteral("payment_proof_valid"))) {
            entry.insert(QStringLiteral("payment_proof_valid"),
                         slate.metadata.value(QStringLiteral("payment_proof_valid")).toBool());
        }
        if (!slate.metadata.value(QStringLiteral("payment_proof_error")).toString().isEmpty()) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         slate.metadata.value(QStringLiteral("payment_proof_error")).toString());
        } else if (existingEntry.contains(QStringLiteral("payment_proof_error"))) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         existingEntry.value(QStringLiteral("payment_proof_error")).toString());
        }
    } else if (existingEntry.contains(QStringLiteral("payment_proof"))) {
        entry.insert(QStringLiteral("payment_proof"), existingEntry.value(QStringLiteral("payment_proof")).toObject());
        if (existingEntry.contains(QStringLiteral("payment_proof_status"))) {
            entry.insert(QStringLiteral("payment_proof_status"),
                         existingEntry.value(QStringLiteral("payment_proof_status")).toString());
        }
        if (existingEntry.contains(QStringLiteral("payment_proof_valid"))) {
            entry.insert(QStringLiteral("payment_proof_valid"),
                         existingEntry.value(QStringLiteral("payment_proof_valid")).toBool());
        }
        if (existingEntry.contains(QStringLiteral("payment_proof_error"))) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         existingEntry.value(QStringLiteral("payment_proof_error")).toString());
        }
    }
    if (slate.metadata.value(QStringLiteral("tx_skeleton")).isObject()) {
        const QJsonObject txSkeleton = slate.metadata.value(QStringLiteral("tx_skeleton")).toObject();
        entry.insert(QStringLiteral("tx_skeleton"), txSkeleton);
        const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
        if (!kernels.isEmpty()) {
            const QJsonObject kernel = kernels.first().toObject();
            if (!kernel.value(QStringLiteral("excess")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_excess"), kernel.value(QStringLiteral("excess")).toString());
            }
            if (!kernel.value(QStringLiteral("excess_sig")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_signature"), kernel.value(QStringLiteral("excess_sig")).toString());
            }
        }
    }

    bool replaced = false;
    for (int i = 0; i < transactions.size(); ++i) {
        if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == slate.workflowId()) {
            transactions.replace(i, entry);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        transactions.append(entry);
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document->insert(QStringLiteral("wallet_state"), walletState);
    return true;
}

bool GrinWalletWorkflow::finalizeOutputs(QJsonObject *document,
                                         const SlateV4 &slate,
                                         bool broadcasted,
                                         qulonglong chainHeight,
                                         const QJsonObject &localContext)
{
    if (!document || slate.workflowId().isEmpty()) {
        return false;
    }

    QJsonObject walletState = document->value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selectedCommitments.size(); ++j) {
            if (outputs[i].commitment == selectedCommitments.at(j).toString()) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = !broadcasted;
                outputs[i].locked = !broadcasted;
                outputs[i].spent = broadcasted;
                outputs[i].onChain = false;
            }
        }

        if (!changeCommit.isEmpty() && outputs[i].commitment == changeCommit) {
            outputs[i].workflowId = slate.workflowId();
            outputs[i].pending = true;
            outputs[i].locked = !broadcasted;
        }

        for (int j = 0; j < slate.commitments.size(); ++j) {
            if (outputs[i].commitment == slate.commitments.at(j).commitment) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = true;
                outputs[i].locked = !broadcasted;
            }
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, chainHeight));
    document->insert(QStringLiteral("wallet_state"), walletState);
    return true;
}

bool GrinWalletWorkflow::finalizeBroadcastedWorkflow(QJsonObject *document,
                                                     const QString &workflowId,
                                                     qulonglong chainHeight,
                                                     const QJsonObject &localContext)
{
    if (!document || workflowId.isEmpty()) {
        return false;
    }

    QJsonObject walletState = document->value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();

    for (int i = 0; i < outputs.size(); ++i) {
        bool matchesSelectedInput = false;
        for (int j = 0; j < selectedCommitments.size(); ++j) {
            if (outputs[i].commitment == selectedCommitments.at(j).toString()) {
                matchesSelectedInput = true;
                break;
            }
        }

        if (matchesSelectedInput) {
            outputs[i].pending = false;
            outputs[i].locked = false;
            outputs[i].spent = true;
            outputs[i].onChain = false;
            continue;
        }

        if (outputs[i].workflowId == workflowId) {
            outputs[i].pending = true;
            outputs[i].locked = false;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, chainHeight));
    document->insert(QStringLiteral("wallet_state"), walletState);
    return true;
}
