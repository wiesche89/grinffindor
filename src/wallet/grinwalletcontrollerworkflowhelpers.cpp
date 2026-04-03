#include "grinwalletcontroller.h"

#include "grinwalletcontrollerhelpers.h"
#include "grinwalletstorage.h"
#include "grinwalletworkflow.h"
#include "grinwalletworkflowhelpers.h"
#include "walletkeychain.h"
#include "walletscanner.h"
#include "walletselection.h"

/**
 * @brief GrinWalletController::currentSlatepackAddress
 * @return
 */
QString GrinWalletController::currentSlatepackAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::slatepackAddress(keychain, m_selectedNetwork) : QString();
}

/**
 * @brief GrinWalletController::currentPaymentProofAddress
 * @return
 */
QString GrinWalletController::currentPaymentProofAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::paymentProofAddress(keychain) : QString();
}

/**
 * @brief GrinWalletController::currentSlatepackSecret
 * @return
 */
QByteArray GrinWalletController::currentSlatepackSecret() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QByteArray();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? keychain.slatepackSecretKey() : QByteArray();
}

/**
 * @brief GrinWalletController::alignSlateVersionWithNode
 * @param slate
 */
void GrinWalletController::alignSlateVersionWithNode(SlateV4 *slate) const
{
    if (!slate) {
        return;
    }

    if (slate->ver.slateVersion <= 0) {
        slate->ver.slateVersion = 4;
    }

    const bool preserveIncomingInvoiceVersion =
        slate->metadata.value(QStringLiteral("external_binary")).toBool()

        && slate->modeCode() == QStringLiteral("invoice");
    if (preserveIncomingInvoiceVersion) {
        return;
    }

    if (m_nodeBlockHeaderVersion > 0 && slate->ver.blockHeaderVersion != m_nodeBlockHeaderVersion) {
        slate->ver.blockHeaderVersion = m_nodeBlockHeaderVersion;
    }
}

/**
 * @brief GrinWalletController::resolveWorkflowIdBySlateId
 * @param slate
 * @return
 */
QString GrinWalletController::resolveWorkflowIdBySlateId(const SlateV4 &slate) const
{
    if (slate.id.trimmed().isEmpty()) {
        return QString();
    }

    const QJsonArray transactions = GrinWalletStorage::loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))

                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("slate_id")).toString() == slate.id) {
            const QString resolvedWorkflowId =
                tx.value(QStringLiteral("workflow_id")).toString().trimmed();
            if (!resolvedWorkflowId.isEmpty()) {
                return resolvedWorkflowId;
            }
        }
    }

    return QString();
}

/**
 * @brief GrinWalletController::resolveWorkflowAmountNano
 * @param workflowId
 * @param localContext
 * @param amount
 * @return
 */
quint64 GrinWalletController::resolveWorkflowAmountNano(const QString &workflowId,
                                                        const QJsonObject &localContext,
                                                        const QString &amount) const
{

    quint64 resolvedAmount = GrinWalletWorkflowHelpers::amountToNanogrin(amount);
    if (resolvedAmount == 0) {
        resolvedAmount = localContext.value(QStringLiteral("amount_nano")).toVariant().toULongLong();
    }
    if (resolvedAmount != 0) {
        return resolvedAmount;
    }

    const QJsonArray transactions = GrinWalletStorage::loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))

                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() == workflowId) {
            return GrinWalletWorkflowHelpers::amountToNanogrin(tx.value(QStringLiteral("amount")).toString());
        }
    }

    return 0;
}

/**
 * @brief GrinWalletController::legacyInvoiceParticipantFromContext
 * @param localContext
 * @return
 */
QJsonObject GrinWalletController::legacyInvoiceParticipantFromContext(const QJsonObject &localContext)
{
    QJsonObject json;
    const QString secKey = localContext.value(QStringLiteral("invoice_sender_blind_secret")).toString();
    const QString secNonce = localContext.value(QStringLiteral("invoice_sender_nonce_secret")).toString();
    const QString pubKey = localContext.value(QStringLiteral("invoice_sender_blind_public")).toString();

    const QString pubNonce = localContext.value(QStringLiteral("invoice_sender_nonce_public")).toString();
    if (secKey.isEmpty() || secNonce.isEmpty() || pubKey.isEmpty() || pubNonce.isEmpty()) {
        return QJsonObject();
    }

    json.insert(QStringLiteral("sec_key"), secKey);
    json.insert(QStringLiteral("sec_nonce"), secNonce);
    json.insert(QStringLiteral("pub_key"), pubKey);
    json.insert(QStringLiteral("pub_nonce"), pubNonce);
    return json;
}

/**
 * @brief GrinWalletController::ensureWorkflowSelectionContext
 * @param workflowId
 * @param amount
 * @param feeOut
 * @param errorOut
 * @return
 */
bool GrinWalletController::ensureWorkflowSelectionContext(const QString &workflowId,
                                                          const QString &amount,
                                                          QString *feeOut,
                                                          QString *errorOut)
{
    if (workflowId.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow id is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    if (!localContext.value(QStringLiteral("selected_input_commits")).toArray().isEmpty()) {
        const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();
        const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
        bool hasTransactionEntry = false;
        for (int i = 0; i < transactions.size(); ++i) {
            if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == workflowId) {
                hasTransactionEntry = true;
                break;
            }
        }
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        bool hasMatchingSelection = false;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const WalletOutput output =
                GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, selectedCommitments.at(i).toString());
            if (!output.commitment.isEmpty()) {
                hasMatchingSelection = true;
                if (output.locked) {
                    break;
                }
            }
        }

        // Inputs selected for a SEND workflow intentionally keep their original
        // workflow id (the workflow that created the UTXO). Requiring
        // output.workflowId == workflowId here can incorrectly discard a valid
        // S1 selection before S3 finalization.
        if (!hasMatchingSelection) {
            localContext.remove(QStringLiteral("selected_inputs"));
            localContext.remove(QStringLiteral("selected_total"));
            localContext.remove(QStringLiteral("selected_input_commits"));
            localContext.remove(QStringLiteral("selected_input_coinbase"));
            localContext.remove(QStringLiteral("change_amount"));
            localContext.remove(QStringLiteral("amount_nano"));
            localContext.remove(QStringLiteral("amount_display"));
            localContext.remove(QStringLiteral("fee_nano"));
            localContext.remove(QStringLiteral("fee_amount_display"));
            localContext.remove(QStringLiteral("change_commit"));
            localContext.remove(QStringLiteral("change_proof"));
            localContext.remove(QStringLiteral("change_amount_display"));
            localContext.remove(QStringLiteral("change_child_index"));
            localContext.remove(QStringLiteral("change_key_path"));
            storeWorkflowContext(workflowId, localContext);
        } else {
            if (!hasTransactionEntry) {
                SlateV4 placeholder;
                placeholder.id = workflowId;
                placeholder.metadata.insert(QStringLiteral("workflow_id"), workflowId);
                placeholder.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
                placeholder.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
                placeholder.setStateFromCode(QStringLiteral("I1"));
                placeholder.amount = localContext.value(QStringLiteral("amount_display")).toString();
                placeholder.fee = localContext.value(QStringLiteral("fee_amount_display")).toString();
                placeholder.offset = localContext.value(QStringLiteral("offset")).toString();
                persistWorkflowTransaction(placeholder, false);
            }
            QJsonObject selectedInputCoinbase;
            quint64 selectedTotal = 0;
            for (int i = 0; i < selectedCommitments.size(); ++i) {
                const QString commitment = selectedCommitments.at(i).toString();
                const WalletOutput output = GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, commitment);
                if (!output.commitment.isEmpty()) {
                    selectedInputCoinbase.insert(output.commitment, output.coinbase);
                    selectedTotal += GrinWalletWorkflowHelpers::amountToNanogrin(output.amount);
                } else {
                    const QJsonValue persistedValue =
                        localContext.value(QStringLiteral("selected_input_coinbase")).toObject().value(commitment);
                    if (!persistedValue.isUndefined()) {
                        selectedInputCoinbase.insert(commitment, persistedValue.toBool());
                    }
                }
            }
            if (!selectedInputCoinbase.isEmpty()
                && selectedInputCoinbase != localContext.value(QStringLiteral("selected_input_coinbase")).toObject()) {
                localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
                storeWorkflowContext(workflowId, localContext);
            }
            if (!localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).isObject()) {
                const QJsonObject legacyParticipant = legacyInvoiceParticipantFromContext(localContext);
                if (!legacyParticipant.isEmpty()) {
                    localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")), legacyParticipant);
                    localContext.remove(QStringLiteral("invoice_sender_blind_secret"));
                    localContext.remove(QStringLiteral("invoice_sender_nonce_secret"));
                    localContext.remove(QStringLiteral("invoice_sender_blind_public"));
                    localContext.remove(QStringLiteral("invoice_sender_nonce_public"));
                    storeWorkflowContext(workflowId, localContext);
                }
            }
            quint64 amountNano = localContext.value(QStringLiteral("amount_nano")).toVariant().toULongLong();
            if (amountNano == 0) {
                amountNano = resolveWorkflowAmountNano(workflowId, localContext, amount);
                if (amountNano > 0) {
                    localContext.insert(QStringLiteral("amount_nano"), QString::number(amountNano));
                    if (localContext.value(QStringLiteral("amount_display")).toString().trimmed().isEmpty()) {
                        localContext.insert(QStringLiteral("amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(amountNano));
                    }
                    storeWorkflowContext(workflowId, localContext);
                }
            }
            if (amountNano == 0) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Workflow amount context is missing. Restart the send workflow from S1.");
                }
                return false;
            }
            const quint64 feeWithoutChange =
                WalletSelection::estimateFee(selectedCommitments.size(), 1, 1);
            const quint64 feeWithChange =
                WalletSelection::estimateFee(selectedCommitments.size(), 2, 1);
            const bool exactNoChange = (selectedTotal == amountNano + feeWithoutChange);
            const quint64 recalculatedFee = exactNoChange ? feeWithoutChange : feeWithChange;
            const quint64 recalculatedChange =
                (selectedTotal >= amountNano + recalculatedFee)
                    ? (selectedTotal - amountNano - recalculatedFee)
                    : 0;
            const quint64 persistedFee = localContext.value(QStringLiteral("fee_nano")).toVariant().toULongLong();
            const QString existingChangeAmountDisplay = localContext.value(QStringLiteral("change_amount_display")).toString();
            const QString recalculatedChangeDisplay =
                recalculatedChange > 0 ? GrinWalletWorkflowHelpers::formatNanogrin(recalculatedChange) : QString();
            if (persistedFee != recalculatedFee
                || localContext.value(QStringLiteral("selected_total")).toString() != QString::number(selectedTotal)
                || localContext.value(QStringLiteral("change_amount")).toString() != QString::number(recalculatedChange)
                || existingChangeAmountDisplay != recalculatedChangeDisplay) {
                localContext.insert(QStringLiteral("selected_total"), QString::number(selectedTotal));
                localContext.insert(QStringLiteral("fee_nano"), QString::number(recalculatedFee));
                localContext.insert(QStringLiteral("fee_amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(recalculatedFee));
                localContext.insert(QStringLiteral("change_amount"), QString::number(recalculatedChange));
                if (recalculatedChange > 0) {
                    WalletOutput changeOutput;
                    SlateV4::Commit changeCommit;
                    QString outputError;
                    if (buildOwnedOutput(QStringLiteral("change"),
                                         recalculatedChangeDisplay,
                                         &changeOutput,
                                         &changeCommit,
                                         &outputError)) {
                        changeOutput.workflowId = workflowId;
                        storeOwnedOutput(changeOutput);
                        localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
                        localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
                        localContext.insert(QStringLiteral("change_amount_display"), recalculatedChangeDisplay);
                        localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
                        localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
                    } else if (errorOut) {
                        *errorOut = outputError.isEmpty()
                            ? QStringLiteral("Failed to rebuild change output.")
                            : outputError;
                        return false;
                    }
                } else {
                    localContext.remove(QStringLiteral("change_commit"));
                    localContext.remove(QStringLiteral("change_proof"));
                    localContext.remove(QStringLiteral("change_amount_display"));
                    localContext.remove(QStringLiteral("change_child_index"));
                    localContext.remove(QStringLiteral("change_key_path"));
                }
                storeWorkflowContext(workflowId, localContext);
            }
            if (feeOut) {
                *feeOut = recalculatedFee > 0
                    ? GrinWalletWorkflowHelpers::formatNanogrin(recalculatedFee)
                    : localContext.value(QStringLiteral("fee_amount_display")).toString();
            }
            return true;
        }
    }

    quint64 requestedAmount = resolveWorkflowAmountNano(workflowId, localContext, amount);
    if (requestedAmount == 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow amount must be greater than zero.");
        }
        return false;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const WalletSelection::Result selection =

        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, m_chainHeight);
    if (!selection.success) {
        if (errorOut) {
            *errorOut = selection.error;
        }
        return false;
    }

    QJsonArray selectedCommitments;
    QJsonObject selectedInputCoinbase;
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs.at(i).commitment != selection.selectedOutputs.at(j).commitment) {
                continue;
            }

            outputs[i].locked = true;
            outputs[i].pending = false;
            // Preserve the UTXO's original workflowId (e.g. from the receive/invoice
            // that created it). The send workflow tracks inputs via selected_input_commits.
            selectedCommitments.append(outputs.at(i).commitment);
            selectedInputCoinbase.insert(outputs.at(i).commitment, outputs.at(i).coinbase);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));

    document.insert(QStringLiteral("wallet_state"), walletState);
    if (!GrinWalletStorage::saveDocument(document)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to persist sender selection context.");
        }
        return false;
    }

    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    localContext.insert(QStringLiteral("amount_nano"), QString::number(requestedAmount));
    localContext.insert(QStringLiteral("amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("fee_nano"), QString::number(selection.fee));

    localContext.insert(QStringLiteral("fee_amount_display"), GrinWalletWorkflowHelpers::formatNanogrin(selection.fee));
    if (!localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).isObject()) {
        localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(
                                WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"))));
    }

    if (selection.change > 0 && localContext.value(QStringLiteral("change_commit")).toString().isEmpty()) {
        const QString changeAmount = GrinWalletWorkflowHelpers::formatNanogrin(selection.change);
        WalletOutput changeOutput;
        SlateV4::Commit changeCommit;
        QString outputError;
        if (buildOwnedOutput(QStringLiteral("change"), changeAmount, &changeOutput, &changeCommit, &outputError)) {
            changeOutput.workflowId = workflowId;
            storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (errorOut) {
            *errorOut = outputError.isEmpty()
                ? QStringLiteral("Failed to derive change output.")
                : outputError;
            return false;
        }
    }

    storeWorkflowContext(workflowId, localContext);

    refreshStateFromStorage();
    if (feeOut) {
        *feeOut = GrinWalletWorkflowHelpers::formatNanogrin(selection.fee);
    }
    return true;
}

/**
 * @brief GrinWalletController::prepareInvoiceSenderContext
 * @param workflowId
 * @param slate
 * @param signatureOverrideOut
 * @param errorOut
 * @return
 */
bool GrinWalletController::prepareInvoiceSenderContext(
    const QString &workflowId,
    SlateV4 *slate,
    WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
    QString *errorOut)
{
    if (!slate || !signatureOverrideOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invoice sender context target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    WalletCryptoBackend::ParticipantContext senderAggsig =
        GrinWalletWorkflowHelpers::participantContextFromJson(
            localContext.value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).toObject(),
            QStringLiteral("sender"));
    if (senderAggsig.blindSecret.isEmpty()

        || senderAggsig.nonceSecret.isEmpty()
        || senderAggsig.blindPublic.isEmpty()
        || senderAggsig.noncePublic.isEmpty()) {
        senderAggsig = WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
        localContext.insert(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(senderAggsig));
        storeWorkflowContext(workflowId, localContext);
    }

    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();

    QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
    if (!m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < trackedOutputs.size(); ++i) {
                trackedOutputs[i] = GrinWalletControllerHelpers::normalizedTrackedOutput(trackedOutputs.at(i), keychain);
            }
        }
    }

    QStringList positiveBlinds;

    const QString priorOffset = slate->offset.trimmed();
    if (!priorOffset.isEmpty() && priorOffset != QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000")) {
        positiveBlinds.append(priorOffset);
    }

    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommit.isEmpty()) {
        const WalletOutput changeOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommit);
        if (!changeOutput.blindingFactor.isEmpty()) {
            positiveBlinds.append(changeOutput.blindingFactor);
        }
    }

    QStringList negativeBlinds;

    negativeBlinds.append(senderAggsig.blindSecret);
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (!input.blindingFactor.isEmpty()) {
            negativeBlinds.append(input.blindingFactor);
        }
    }

    QString cryptoError;
    const QString adjustedOffset = WalletCryptoBackend::combineBlindingFactors(
        positiveBlinds,
        negativeBlinds,
        &cryptoError);
    if (adjustedOffset.isEmpty()) {
        if (errorOut) {
            *errorOut = cryptoError.isEmpty()
                ? QStringLiteral("Failed to derive invoice sender offset.")
                : cryptoError;
        }
        return false;
    }

    slate->offset = adjustedOffset;
    *signatureOverrideOut = senderAggsig;

    QList<SlateV4::Commit> rebuiltCommitments = slate->commitments;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (input.commitment.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (int j = 0; j < rebuiltCommitments.size(); ++j) {
            if (rebuiltCommitments.at(j).commitment == input.commitment) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        SlateV4::Commit commit;
        const bool inputCoinbase =
            selectedInputCoinbase.value(input.commitment).toBool(input.coinbase);
        commit.feature = inputCoinbase ? 1 : 0;
        commit.commitment = input.commitment;
        rebuiltCommitments.append(commit);
    }

    const QString changeCommitment = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommitment.isEmpty()) {
        const WalletOutput changeOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommitment);
        if (!changeOutput.commitment.isEmpty()) {
            bool exists = false;
            for (int i = 0; i < rebuiltCommitments.size(); ++i) {
                if (rebuiltCommitments.at(i).commitment == changeOutput.commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                SlateV4::Commit commit;
                commit.feature = 0;
                commit.commitment = changeOutput.commitment;
                commit.proof = changeOutput.proof;
                rebuiltCommitments.append(commit);
            }
        }
    }

    slate->commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(rebuiltCommitments);
    return true;
}

/**
 * @brief GrinWalletController::prepareStandardSenderContext
 * @param workflowId
 * @param slate
 * @param signatureOverrideOut
 * @param errorOut
 * @return
 */
bool GrinWalletController::prepareStandardSenderContext(
    const QString &workflowId,
    SlateV4 *slate,
    WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
    QString *errorOut)
{
    if (!slate || !signatureOverrideOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Standard sender context target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    WalletCryptoBackend::ParticipantContext senderAggsig =
        GrinWalletWorkflowHelpers::participantContextFromJson(
            localContext.value(GrinWalletWorkflowHelpers::standardContextKey(QStringLiteral("participant"))).toObject(),
            QStringLiteral("sender"));
    if (senderAggsig.blindSecret.isEmpty()

        || senderAggsig.nonceSecret.isEmpty()
        || senderAggsig.blindPublic.isEmpty()
        || senderAggsig.noncePublic.isEmpty()) {
        senderAggsig = WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
        localContext.insert(GrinWalletWorkflowHelpers::standardContextKey(QStringLiteral("participant")),
                            GrinWalletWorkflowHelpers::participantContextToJson(senderAggsig));
        storeWorkflowContext(workflowId, localContext);
    }

    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    const QJsonObject walletState = GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject();

    QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
    if (!m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < trackedOutputs.size(); ++i) {
                trackedOutputs[i] = GrinWalletControllerHelpers::normalizedTrackedOutput(trackedOutputs.at(i), keychain);
            }
        }
    }

    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    const WalletOutput changeOutput = changeCommit.isEmpty()
        ? WalletOutput()
        : GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, changeCommit);

    QString canonicalOffset = localContext.value(QStringLiteral("standard_sender_offset")).toString().trimmed();
    if (canonicalOffset.isEmpty()) {
        canonicalOffset = slate->offset.trimmed();
        if (canonicalOffset.isEmpty()) {
            canonicalOffset = localContext.value(QStringLiteral("incoming_s2_offset")).toString().trimmed();
        }
        if (canonicalOffset.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Failed to recover canonical standard sender offset.");
            }
            return false;
        }

        localContext.insert(QStringLiteral("standard_sender_offset"), canonicalOffset);
        storeWorkflowContext(workflowId, localContext);
    }

    const QString incomingOffset = slate->offset.trimmed();
    localContext.insert(QStringLiteral("incoming_s2_offset"), incomingOffset);

    QString effectiveOffset = canonicalOffset;
    if (!incomingOffset.isEmpty() && incomingOffset != canonicalOffset) {
        // grin-wallet updates the shared transaction offset during S2.
        // Compact external S2 slates do not reliably preserve our local metadata,
        // so the sender must treat the imported S2 offset as canonical here.
        effectiveOffset = incomingOffset;
    }

    slate->offset = effectiveOffset;
    localContext.insert(QStringLiteral("standard_sender_offset"), canonicalOffset);
    storeWorkflowContext(workflowId, localContext);

    *signatureOverrideOut = senderAggsig;

    QList<SlateV4::Commit> rebuiltCommitments = slate->commitments;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = GrinWalletControllerHelpers::findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (input.commitment.isEmpty()) {
            continue;
        }
        bool exists = false;
        for (int j = 0; j < rebuiltCommitments.size(); ++j) {
            if (rebuiltCommitments.at(j).commitment == input.commitment) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            SlateV4::Commit commit;
            commit.feature = selectedInputCoinbase.value(input.commitment).toBool(input.coinbase) ? 1 : 0;
            commit.commitment = input.commitment;
            rebuiltCommitments.append(commit);
        }
    }

    if (!changeCommit.isEmpty() && !changeOutput.commitment.isEmpty()) {
        bool exists = false;
        for (int i = 0; i < rebuiltCommitments.size(); ++i) {
            if (rebuiltCommitments.at(i).commitment == changeOutput.commitment) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            SlateV4::Commit commit;
            commit.feature = 0;
            commit.commitment = changeOutput.commitment;
            commit.proof = changeOutput.proof;
            rebuiltCommitments.append(commit);
        }
    }

    slate->commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(rebuiltCommitments);
    return true;
}

/**
 * @brief GrinWalletController::compactInvoiceSlateForReturn
 * @param workflowId
 * @param slate
 */
void GrinWalletController::compactInvoiceSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    WalletCryptoBackend::ParticipantContext senderContext = GrinWalletWorkflowHelpers::participantContextFromJson(
        workflowContext(workflowId).value(GrinWalletWorkflowHelpers::invoiceContextKey(QStringLiteral("participant"))).toObject(),

        QStringLiteral("sender"));
    if (senderContext.blindPublic.isEmpty() || senderContext.noncePublic.isEmpty()) {
        senderContext = WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == senderContext.blindPublic && sig.nonce == senderContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }
    slate->numParticipants = 2;
    slate->amount.clear();
    slate->metadata.remove(QStringLiteral("message_hash"));
    slate->metadata.remove(QStringLiteral("pubkey_total"));
    slate->metadata.remove(QStringLiteral("pubnonce_total"));
    slate->metadata.remove(QStringLiteral("signature_status"));
    slate->metadata.remove(QStringLiteral("processed_by"));
    slate->metadata.remove(QStringLiteral("processed_at"));
    slate->metadata.remove(QStringLiteral("tx_ready"));
    slate->metadata.remove(QStringLiteral("network"));
}

/**
 * @brief GrinWalletController::compactStandardSlateForReturn
 * @param workflowId
 * @param slate
 */
void GrinWalletController::compactStandardSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    const QString receiverBlind = slate->metadata.value(QStringLiteral("receiver_blind")).toString().trimmed();
    WalletCryptoBackend::ParticipantContext receiverContext;
    if (!receiverBlind.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipantFromBlindSecret(
            receiverBlind,
            m_seedFingerprint,
            workflowId,
            QStringLiteral("receiver"));
    }
    if (receiverContext.blindSecret.isEmpty()

        || receiverContext.nonceSecret.isEmpty()
        || receiverContext.blindPublic.isEmpty()
        || receiverContext.noncePublic.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    }

    if (!receiverBlind.isEmpty() && !receiverContext.blindSecret.isEmpty()) {
        QStringList positiveBlinds;
        positiveBlinds.append(slate->offset);
        positiveBlinds.append(receiverBlind);

        QStringList negativeBlinds;
        negativeBlinds.append(receiverContext.blindSecret);

        QString offsetError;
        const QString adjustedOffset =
            WalletCryptoBackend::combineBlindingFactors(positiveBlinds, negativeBlinds, &offsetError);
        if (!adjustedOffset.isEmpty()) {
            slate->offset = adjustedOffset;
        }
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == receiverContext.blindPublic && sig.nonce == receiverContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }

    slate->amount.clear();
    slate->fee.clear();
}

/**
 * @brief GrinWalletController::ensureReceiverOutputContext
 * @param workflowId
 * @param amount
 * @param source
 * @param outputOut
 * @param commitOut
 * @param errorOut
 * @return
 */
bool GrinWalletController::ensureReceiverOutputContext(const QString &workflowId,
                                                       const QString &amount,
                                                       const QString &source,
                                                       WalletOutput *outputOut,
                                                       SlateV4::Commit *commitOut,
                                                       QString *errorOut)
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Receiver output target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);

    const QString existingCommitment = localContext.value(QStringLiteral("receiver_commit")).toString();
    if (!existingCommitment.isEmpty()) {
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(
            GrinWalletStorage::loadDocument().value(QStringLiteral("wallet_state")).toObject());
        WalletOutput existingOutput = GrinWalletControllerHelpers::findTrackedOutputByCommitment(outputs, existingCommitment);
        if (existingOutput.commitment.isEmpty()) {
            existingOutput.commitment = existingCommitment;
            existingOutput.proof = localContext.value(QStringLiteral("receiver_proof")).toString();
            existingOutput.amount = localContext.value(QStringLiteral("receiver_amount_display")).toString(amount.trimmed());
            existingOutput.source = source;
            existingOutput.keyPath = localContext.value(QStringLiteral("receiver_key_path")).toString();
            existingOutput.blindingFactor = localContext.value(QStringLiteral("receiver_blind")).toString();
            existingOutput.childIndex = static_cast<quint32>(
                localContext.value(QStringLiteral("receiver_child_index")).toInt());
            existingOutput.workflowId = workflowId;
            existingOutput.pending = true;
            existingOutput.locked = false;
            existingOutput.onChain = false;
            existingOutput.spent = false;
            storeOwnedOutput(existingOutput);
        }

        *outputOut = existingOutput;
        commitOut->commitment = existingOutput.commitment;
        commitOut->proof = existingOutput.proof;
        return !existingOutput.commitment.isEmpty();
    }

    WalletOutput output;
    SlateV4::Commit commit;
    if (!buildOwnedOutput(source, amount, &output, &commit, errorOut)) {
        return false;
    }

    output.workflowId = workflowId;
    output.pending = true;
    output.locked = false;
    output.onChain = false;
    output.spent = false;
    storeOwnedOutput(output);

    localContext.insert(QStringLiteral("receiver_commit"), commit.commitment);
    localContext.insert(QStringLiteral("receiver_proof"), commit.proof);
    localContext.insert(QStringLiteral("receiver_amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("receiver_child_index"), static_cast<int>(output.childIndex));
    localContext.insert(QStringLiteral("receiver_key_path"), output.keyPath);
    localContext.insert(QStringLiteral("receiver_blind"), output.blindingFactor);
    storeWorkflowContext(workflowId, localContext);

    *outputOut = output;
    *commitOut = commit;
    return true;
}

/**
 * @brief GrinWalletController::persistWorkflowTransaction
 * @param slate
 * @param broadcasted
 */
void GrinWalletController::persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletWorkflow::persistTransaction(&document, slate, broadcasted)) {
        GrinWalletStorage::saveDocument(document);
    }
}

/**
 * @brief GrinWalletController::finalizeWorkflowOutputs
 * @param slate
 * @param broadcasted
 */
void GrinWalletController::finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();

    const QJsonObject localContext = workflowContext(slate.workflowId());
    if (GrinWalletWorkflow::finalizeOutputs(&document, slate, broadcasted, m_chainHeight, localContext)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

/**
 * @brief GrinWalletController::finalizeBroadcastedWorkflow
 * @param workflowId
 */
void GrinWalletController::finalizeBroadcastedWorkflow(const QString &workflowId)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();

    const QJsonObject localContext = workflowContext(workflowId);
    if (GrinWalletWorkflow::finalizeBroadcastedWorkflow(&document, workflowId, m_chainHeight, localContext)) {
        GrinWalletStorage::saveDocument(document);
        refreshStateFromStorage();
    }
}

/**
 * @brief GrinWalletController::storeWorkflowContext
 * @param workflowId
 * @param context
 */
void GrinWalletController::storeWorkflowContext(const QString &workflowId, const QJsonObject &context)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = GrinWalletStorage::loadDocument();
    if (GrinWalletStorage::storeWorkflowContext(&document, workflowId, context)) {
        GrinWalletStorage::saveDocument(document);
    }
}

/**
 * @brief GrinWalletController::workflowContext
 * @param workflowId
 * @return
 */
QJsonObject GrinWalletController::workflowContext(const QString &workflowId) const
{
    return GrinWalletStorage::workflowContext(GrinWalletStorage::loadDocument(), workflowId);
}
