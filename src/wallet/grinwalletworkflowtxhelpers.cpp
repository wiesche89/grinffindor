#include "grinwalletworkflowtxhelpers.h"

#include <QJsonArray>

#include "grinwalletworkflowhelpers.h"

QList<WalletOutput> GrinWalletWorkflowTxHelpers::collectSelectedInputs(
    const QJsonObject &localContext,
    const QList<WalletOutput> &trackedOutputs)
{
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    QList<WalletOutput> selectedInputs;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const QString selectedCommitment = selectedCommitments.at(i).toString();
        WalletOutput selectedInput =
            GrinWalletWorkflowHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitment);
        if (selectedInput.commitment.isEmpty() && !selectedCommitment.trimmed().isEmpty()) {
            selectedInput.commitment = selectedCommitment;
            selectedInput.coinbase = selectedInputCoinbase.value(selectedCommitment).toBool(false);
        }
        if (!selectedInput.commitment.isEmpty()) {
            selectedInputs.append(selectedInput);
        }
    }
    return selectedInputs;
}

WalletOutput GrinWalletWorkflowTxHelpers::resolveReceiverOutput(const SlateV4 &slate,
                                                               const QString &knownChangeCommit)
{
    WalletOutput receiverOutput;
    for (int ci = 0; ci < slate.commitments.size(); ++ci) {
        const SlateV4::Commit &commit = slate.commitments.at(ci);
        if (!commit.proof.trimmed().isEmpty() && commit.commitment != knownChangeCommit) {
            receiverOutput.commitment = commit.commitment;
            receiverOutput.proof = commit.proof;
            receiverOutput.amount = slate.amount;
            break;
        }
    }
    return receiverOutput;
}

WalletOutput GrinWalletWorkflowTxHelpers::resolveChangeOutput(const QJsonObject &localContext,
                                                             const QList<WalletOutput> &trackedOutputs)
{
    WalletOutput changeOutput;
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommit.isEmpty()) {
        changeOutput =
            GrinWalletWorkflowHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommit);
        if (changeOutput.commitment.isEmpty()) {
            changeOutput.commitment = changeCommit;
            changeOutput.proof = localContext.value(QStringLiteral("change_proof")).toString();
            changeOutput.amount = localContext.value(QStringLiteral("change_amount_display")).toString();
        }
    }
    return changeOutput;
}
