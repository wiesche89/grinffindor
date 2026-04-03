#include "grinwalletworkflowservice.h"

#include "grinwalletcontroller.h"
#include "grinwalletworkflowtxhelpers.h"
#include "walletcryptobackend.h"
#include "grinwalletworkflowhelpers.h"
#include "walletkeychain.h"
#include "walletscanner.h"
#include "wallettxbuilder.h"
#include "binaryslatev4writer.h"

#include <QDateTime>
#include <QJsonDocument>

/**
 * @brief GrinWalletWorkflowService::GrinWalletWorkflowService
 * @param controller
 */
GrinWalletWorkflowService::GrinWalletWorkflowService(GrinWalletController *controller)
    : QObject(controller)
    , m_controller(controller)
{
}

// -------------------------------------------------------------------------------------------------------
// Processing Workflow Slatepack State
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletWorkflowService::populatePaymentProofAddresses
 * @param slate
 * @param mode
 * @param localRoleTag
 * @param localPaymentProofAddress
 */
void GrinWalletWorkflowService::populatePaymentProofAddresses(
    SlateV4 *slate,
    const QString &mode,
    const QString &localRoleTag,
    const QString &localPaymentProofAddress) const
{
    if (!slate || localPaymentProofAddress.isEmpty() || !slate->hasPaymentProof) {
        return;
    }
    if (mode != QStringLiteral("send") && mode != QStringLiteral("invoice")) {
        return;
    }

    if (slate->paymentProof.senderAddress.trimmed().isEmpty()
        && localRoleTag == QStringLiteral("sender")) {
        slate->paymentProof.senderAddress = localPaymentProofAddress;
    }
    if (slate->paymentProof.receiverAddress.trimmed().isEmpty()
        && localRoleTag == QStringLiteral("receiver")) {
        slate->paymentProof.receiverAddress = localPaymentProofAddress;
    }
}


/**
 * @brief GrinWalletWorkflowService::continueProcessWorkflowSlatepack
 * @param slate
 * @param workflowId
 * @param mode
 * @param state
 * @param localRoleTag
 */
void GrinWalletWorkflowService::continueProcessWorkflowSlatepack(SlateV4 *slate,
                                                                 const QString &workflowId,
                                                                 const QString &mode,
                                                                 const QString &state,
                                                                 const QString &localRoleTag)
{
    const QString localSlatepackAddress = m_controller->currentSlatepackAddress();
    const QString localPaymentProofAddress = m_controller->currentPaymentProofAddress();
    populatePaymentProofAddresses(slate, mode, localRoleTag, localPaymentProofAddress);

    QString cryptoError;
    if (localRoleTag == QStringLiteral("sender")) {
        QString selectedFee;
        if (!m_controller->ensureWorkflowSelectionContext(workflowId, slate->amount, &selectedFee, &cryptoError)) {
            m_controller->setLastError(cryptoError.isEmpty()
                                           ? QStringLiteral("Failed to select sender outputs for workflow funding.")
                                           : cryptoError);
            return;
        }
        if (!selectedFee.isEmpty()) {
            slate->fee = selectedFee;
        }
        if (slate->metadata.value(QStringLiteral("external_binary")).toBool()
            && mode == QStringLiteral("invoice")
            && state == QStringLiteral("I1")) {
            m_controller->persistWorkflowTransaction(*slate, false);
        }
    }

    WalletCryptoBackend::ParticipantContext signatureOverrideContext;
    WalletCryptoBackend::ParticipantContext *signatureOverride = 0;

    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I1")
        && localRoleTag == QStringLiteral("sender")) {
        if (!m_controller->prepareInvoiceSenderContext(workflowId, slate, &signatureOverrideContext, &cryptoError)) {
            m_controller->setLastError(cryptoError.isEmpty()
                                           ? QStringLiteral("Failed to prepare invoice sender context.")
                                           : cryptoError);
            return;
        }
        signatureOverride = &signatureOverrideContext;
    }

    if (mode == QStringLiteral("send")
        && state == QStringLiteral("S2")
        && localRoleTag == QStringLiteral("sender")) {
        if (!m_controller->prepareStandardSenderContext(workflowId, slate, &signatureOverrideContext, &cryptoError)) {
            m_controller->setLastError(cryptoError.isEmpty()
                                           ? QStringLiteral("Failed to prepare standard sender context.")
                                           : cryptoError);
            return;
        }
        signatureOverride = &signatureOverrideContext;
    }

    if (localRoleTag == QStringLiteral("receiver")
        && (slate->commitments.isEmpty()
            || state == QStringLiteral("S1")
            || (mode == QStringLiteral("invoice") && state == QStringLiteral("I2")))) {
        WalletOutput receiveOutput;
        SlateV4::Commit receiveCommit;
        const QString receiverSource =
            (mode == QStringLiteral("invoice")) ? QStringLiteral("invoice") : QStringLiteral("receive");
        if (!m_controller->ensureReceiverOutputContext(workflowId,
                                                       slate->amount,
                                                       receiverSource,
                                                       &receiveOutput,
                                                       &receiveCommit,
                                                       &cryptoError)) {
            m_controller->setLastError(cryptoError.isEmpty()
                                           ? QStringLiteral("Failed to derive receiver output.")
                                           : cryptoError);
            return;
        }

        bool hasReceiverCommit = false;
        for (int i = 0; i < slate->commitments.size(); ++i) {
            if (slate->commitments.at(i).commitment == receiveCommit.commitment) {
                hasReceiverCommit = true;
                break;
            }
        }
        if (!hasReceiverCommit) {
            slate->commitments.append(receiveCommit);
            slate->commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(slate->commitments);
        }
        slate->metadata.insert(QStringLiteral("receiver_blind"), receiveOutput.blindingFactor);
        slate->metadata.insert(QStringLiteral("receiver_child_index"), static_cast<int>(receiveOutput.childIndex));
        slate->metadata.insert(QStringLiteral("receiver_key_path"), receiveOutput.keyPath);

        if (localRoleTag == QStringLiteral("receiver")
            && !receiveOutput.blindingFactor.trimmed().isEmpty()) {
            WalletCryptoBackend::ParticipantContext receiverContextFromBlind =
                WalletCryptoBackend::createParticipantFromBlindSecret(receiveOutput.blindingFactor,
                                                                      m_controller->seedFingerprint(),
                                                                      workflowId,
                                                                      QStringLiteral("receiver"));
            if (!receiverContextFromBlind.blindSecret.isEmpty()
                && !receiverContextFromBlind.nonceSecret.isEmpty()
                && !receiverContextFromBlind.blindPublic.isEmpty()
                && !receiverContextFromBlind.noncePublic.isEmpty()) {
                signatureOverrideContext = receiverContextFromBlind;
                signatureOverride = &signatureOverrideContext;
            }
        }

        if (mode == QStringLiteral("invoice") && state == QStringLiteral("I2")) {
            WalletCryptoBackend::ParticipantContext receiverContext =
                WalletCryptoBackend::createParticipantFromBlindSecret(receiveOutput.blindingFactor,
                                                                      m_controller->seedFingerprint(),
                                                                      workflowId,
                                                                      QStringLiteral("receiver"));
            if (receiverContext.blindSecret.isEmpty()) {
                receiverContext = WalletCryptoBackend::createParticipant(
                    m_controller->seedFingerprint(), workflowId, QStringLiteral("receiver"));
            }
            if (!receiveOutput.blindingFactor.isEmpty() && !receiverContext.blindSecret.isEmpty()) {
                QString offsetError;
                const QString adjustedOffset = WalletCryptoBackend::combineBlindingFactors(
                    QStringList() << slate->offset << receiveOutput.blindingFactor,
                    QStringList() << receiverContext.blindSecret,
                    &offsetError);
                if (!adjustedOffset.isEmpty()) {
                    slate->offset = adjustedOffset;
                }
            }
        }
    }

    if (!WalletCryptoBackend::applyRound2Signature(slate,
                                                   m_controller->seedFingerprint(),
                                                   localRoleTag,
                                                   signatureOverride,
                                                   &cryptoError)) {
        m_controller->setLastError(cryptoError.isEmpty()
                                       ? QStringLiteral("Failed to apply round 2 signature.")
                                       : cryptoError);
        return;
    }
    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I2")
        && localRoleTag == QStringLiteral("receiver")
        && slate->numParticipants < slate->signatures.size()) {
        slate->numParticipants = slate->signatures.size();
    }

    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I1")
        && localRoleTag == QStringLiteral("sender")) {
        if (slate->metadata.value(QStringLiteral("external_binary")).toBool()) {
            slate->metadata.remove(QStringLiteral("tx_skeleton"));
            slate->metadata.remove(QStringLiteral("tx_build_error"));
            slate->metadata.insert(QStringLiteral("tx_ready"), false);
        } else {
            const QJsonObject localContext = m_controller->workflowContext(workflowId);
            const QJsonArray selectedCommitments =
                localContext.value(QStringLiteral("selected_input_commits")).toArray();
            if (!selectedCommitments.isEmpty()) {
                const QJsonObject walletState =
                    m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject();
                const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
                const QList<WalletOutput> selectedInputs =
                    GrinWalletWorkflowTxHelpers::collectSelectedInputs(localContext, trackedOutputs);
                const WalletOutput receiverOutput = GrinWalletWorkflowTxHelpers::resolveReceiverOutput(
                    *slate, localContext.value(QStringLiteral("change_commit")).toString());
                const WalletOutput changeOutput =
                    GrinWalletWorkflowTxHelpers::resolveChangeOutput(localContext, trackedOutputs);

                const WalletTxBuilder::BuildResult txBuild = WalletTxBuilder::buildTransactionSkeleton(
                    *slate,
                    selectedInputs,
                    receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput,
                    changeOutput.commitment.isEmpty() ? 0 : &changeOutput);
                if (txBuild.success) {
                    slate->metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                    slate->metadata.insert(QStringLiteral("tx_ready"), false);
                    slate->metadata.remove(QStringLiteral("tx_build_error"));
                } else {
                    slate->metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
                }
            }
        }
        m_controller->compactInvoiceSlateForReturn(workflowId, slate);
    }

    if (mode == QStringLiteral("send")

        && state == QStringLiteral("S1")
        && localRoleTag == QStringLiteral("receiver")) {
        m_controller->compactStandardSlateForReturn(workflowId, slate);
    }

    if (localRoleTag == QStringLiteral("receiver")

        && m_controller->hasUnlockedSession()) {
        const WalletKeychain keychain(m_controller->sessionMnemonic());
        if (keychain.isValid()
            && !WalletCryptoBackend::signPaymentProof(slate, keychain, &cryptoError)
            && slate->hasPaymentProof) {
            m_controller->setLastInfo(cryptoError);
        }
    }

    slate->advanceState();
    const QString nextState = slate->stateCode();
    const bool externalBinary = slate->metadata.value(QStringLiteral("external_binary")).toBool();
    const bool compactExternalInvoiceI2 =

        externalBinary && nextState == QStringLiteral("I2") && mode == QStringLiteral("invoice");
    if (!compactExternalInvoiceI2) {
        slate->metadata.insert(QStringLiteral("processed_by"), m_controller->walletName());
        slate->metadata.insert(QStringLiteral("processed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    }

    if ((nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3"))

        && !WalletCryptoBackend::finalizeSlate(slate, &cryptoError)) {
        m_controller->setLastError(cryptoError.isEmpty()
                                       ? QStringLiteral("Failed to finalize slate signature.")
                                       : cryptoError);
        return;
    }

    if (slate->hasPaymentProof && !slate->paymentProof.receiverSignature.isEmpty()) {
        if (WalletCryptoBackend::verifyPaymentProof(*slate, &cryptoError)) {
            slate->metadata.insert(QStringLiteral("payment_proof_valid"), true);
            slate->metadata.insert(QStringLiteral("payment_proof_status"), QStringLiteral("verified"));
        } else {
            slate->metadata.insert(QStringLiteral("payment_proof_valid"), false);
            slate->metadata.insert(QStringLiteral("payment_proof_status"), QStringLiteral("invalid"));
            slate->metadata.insert(QStringLiteral("payment_proof_error"), cryptoError);
        }
    }

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        const QJsonObject localContext = m_controller->workflowContext(workflowId);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        if (!selectedCommitments.isEmpty()) {
            const QJsonObject walletState =
                m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject();
            const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
            const QList<WalletOutput> selectedInputs =
                GrinWalletWorkflowTxHelpers::collectSelectedInputs(localContext, trackedOutputs);
            const WalletOutput receiverOutput = GrinWalletWorkflowTxHelpers::resolveReceiverOutput(
                *slate, localContext.value(QStringLiteral("change_commit")).toString());
            const WalletOutput changeOutput =
                GrinWalletWorkflowTxHelpers::resolveChangeOutput(localContext, trackedOutputs);

            WalletTxBuilder::BuildResult txBuild = WalletTxBuilder::buildTransactionSkeleton(
                *slate,
                selectedInputs,
                receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput,
                changeOutput.commitment.isEmpty() ? 0 : &changeOutput);

            if (txBuild.success) {
                slate->metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                slate->metadata.insert(QStringLiteral("tx_ready"), true);
                slate->metadata.remove(QStringLiteral("tx_build_error"));
                m_controller->finalizeWorkflowOutputs(*slate, false);
            } else {
                slate->metadata.insert(QStringLiteral("tx_ready"), false);
                slate->metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
            }
        } else if (externalBinary && nextState == QStringLiteral("I3")) {
            WalletOutput receiverOutput;
            SlateV4::Commit receiverCommit;
            QString receiverError;
            const QJsonObject localContext = m_controller->workflowContext(workflowId);
            const QString receiverAmount =
                localContext.value(QStringLiteral("receiver_amount_display")).toString();
            if (!receiverAmount.trimmed().isEmpty()) {
                if (!m_controller->ensureReceiverOutputContext(workflowId,
                                                               receiverAmount,
                                                               QStringLiteral("invoice"),
                                                               &receiverOutput,
                                                               &receiverCommit,
                                                               &receiverError)) {
                    slate->metadata.insert(QStringLiteral("tx_ready"), false);
                    slate->metadata.insert(QStringLiteral("tx_build_error"),
                                           receiverError.isEmpty()
                                               ? QStringLiteral("Failed to derive receiver output for final invoice transaction.")
                                               : receiverError);
                    receiverOutput = WalletOutput();
                }
            }
            const WalletTxBuilder::BuildResult txBuild =
                WalletTxBuilder::buildTransactionSkeletonFromCommitments(
                    *slate,
                    receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput);
            if (txBuild.success) {
                slate->metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                slate->metadata.insert(QStringLiteral("tx_ready"), true);
                slate->metadata.remove(QStringLiteral("tx_build_error"));
            } else {
                slate->metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
            }
        }
    }

    QString updatedSlatepack;
    const QStringList outgoingRecipients;
    const QString outgoingSender = localSlatepackAddress;
    if (!outgoingSender.trimmed().isEmpty()) {
        slate->metadata.insert(QStringLiteral("slatepack_sender"), outgoingSender);
    }
    const QString updatedDecoded =
        QString::fromUtf8(QJsonDocument(slate->toJson()).toJson(QJsonDocument::Indented));
    if (!BinarySlateV4Writer::encodeSlatepack(*slate,
                                              &updatedSlatepack,
                                              &cryptoError,
                                              outgoingSender,
                                              outgoingRecipients,

                                              m_controller->currentSlatepackSecret())) {
        if (externalBinary) {
            m_controller->setLastError(cryptoError.isEmpty()
                                           ? QStringLiteral("Failed to encode binary Slatepack.")
                                           : cryptoError);
            return;
        }
        updatedSlatepack =
            GrinWalletWorkflowHelpers::encodeSlatepackArmor(updatedDecoded, localSlatepackAddress);
    }
    m_controller->persistWorkflowTransaction(*slate, false);
    m_controller->setWorkflow(workflowId, mode, nextState, updatedSlatepack, updatedDecoded);

    const bool autoBroadcastExternalFinal =
        externalBinary
        && (nextState == QStringLiteral("I3") || nextState == QStringLiteral("S3"))
        && slate->metadata.value(QStringLiteral("tx_ready")).toBool()

        && slate->metadata.value(QStringLiteral("tx_skeleton")).isObject();
    if (autoBroadcastExternalFinal) {
        m_controller->broadcastCurrentWorkflowTransaction();
        if (m_controller->hasPendingBroadcastWorkflow()) {
            m_controller->setLastInfo(
                QStringLiteral("Workflow %1 advanced to %2 and is being broadcast to the node.")
                    .arg(workflowId, nextState));
            return;
        }
    }

    if (externalBinary

        && (nextState == QStringLiteral("I3") || nextState == QStringLiteral("S3"))
        && !slate->metadata.value(QStringLiteral("tx_ready")).toBool()) {
        const QString txBuildError = slate->metadata.value(QStringLiteral("tx_build_error")).toString();
        if (!txBuildError.trimmed().isEmpty()) {
            m_controller->setLastError(QStringLiteral("Final transaction build failed: %1").arg(txBuildError));
            return;
        }
    }

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        m_controller->setLastInfo(
            QStringLiteral("Workflow %1 advanced to %2 and reached the final exchange step.")
                .arg(workflowId, nextState));
    } else {
        m_controller->setLastInfo(QStringLiteral("Workflow %1 advanced from %2 to %3.")
                                      .arg(workflowId, state, nextState));
    }
}
