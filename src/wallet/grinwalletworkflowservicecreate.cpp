#include "grinwalletworkflowservice.h"

#include <QDateTime>
#include <QJsonDocument>

#include "binaryslatev4writer.h"
#include "grinwalletcontroller.h"
#include "grinwalletworkflowhelpers.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"
#include "walletscanner.h"
#include "walletselection.h"

// -------------------------------------------------------------------------------------------------------
// Creating Send And Receive Workflows
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletWorkflowService::startSendWorkflow
 * @param amount
 * @param note
 */
void GrinWalletWorkflowService::startSendWorkflow(const QString &amount, const QString &note)
{
    m_controller->touchWalletSession();

    const quint64 requestedAmount = GrinWalletWorkflowHelpers::amountToNanogrin(amount);
    if (requestedAmount == 0) {
        m_controller->setLastError(QStringLiteral("Send amount must be greater than zero."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();

    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (!m_controller->sessionMnemonic().trimmed().isEmpty()) {
        const WalletKeychain keychain(m_controller->sessionMnemonic());
        if (keychain.isValid()) {
            for (int i = 0; i < outputs.size(); ++i) {
                outputs[i] = GrinWalletWorkflowHelpers::normalizedTrackedOutput(outputs.at(i), keychain);
            }
        }
    }

    const qulonglong effectiveHeight =
        m_controller->chainHeight() > 0 ? m_controller->chainHeight() : m_controller->scanHeight();
    const WalletSelection::Result selection =

        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, effectiveHeight);
    if (!selection.success) {
        m_controller->setLastError(selection.error);
        return;
    }

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                outputs[i].locked = true;
            }
        }
    }

    SlateV4 slate;
    m_controller->alignSlateVersionWithNode(&slate);
    const QString workflowId = slate.id;

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"),
                       WalletScanner::balancesFromOutputs(outputs, m_controller->chainHeight()));
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    const WalletCryptoBackend::ParticipantContext senderContext =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
    slate.state = SlateV4::Standard1;
    slate.amount = GrinWalletWorkflowHelpers::formatNanogrin(requestedAmount);
    slate.fee = QStringLiteral("%1.%2")
        .arg(QString::number(selection.fee / 1000000000ULL))
        .arg(QString::number(selection.fee % 1000000000ULL), 9, QLatin1Char('0'));
    slate.signatures.append(WalletCryptoBackend::createParticipantData(senderContext));

    const QString localSlatepackAddress = m_controller->currentSlatepackAddress();
    const QString localPaymentProofAddress = m_controller->currentPaymentProofAddress();

    slate.hasPaymentProof = !localPaymentProofAddress.isEmpty();
    if (slate.hasPaymentProof) {
        slate.paymentProof.senderAddress = localPaymentProofAddress;
    }
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("grin-browser-wallet"));
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    slate.metadata.insert(QStringLiteral("wallet"), m_controller->walletName());
    slate.metadata.insert(QStringLiteral("network"), m_controller->resolvedNetworkName());
    slate.metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonObject localContext;
    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    localContext.insert(QStringLiteral("amount_nano"), QString::number(requestedAmount));
    localContext.insert(QStringLiteral("amount_display"), slate.amount);
    localContext.insert(QStringLiteral("fee_nano"), QString::number(selection.fee));
    localContext.insert(QStringLiteral("fee_amount_display"), slate.fee);
    localContext.insert(GrinWalletWorkflowHelpers::standardContextKey(QStringLiteral("participant")),
                        GrinWalletWorkflowHelpers::participantContextToJson(senderContext));

    QJsonArray selectedCommitments;
    QJsonObject selectedInputCoinbase;
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        selectedCommitments.append(selection.selectedOutputs.at(i).commitment);
        selectedInputCoinbase.insert(selection.selectedOutputs.at(i).commitment,
                                     selection.selectedOutputs.at(i).coinbase);
    }
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);

    WalletOutput changeOutput;
    SlateV4::Commit changeCommit;
    if (selection.change > 0) {
        const QString changeAmount = QStringLiteral("%1.%2")
            .arg(QString::number(selection.change / 1000000000ULL))
            .arg(QString::number(selection.change % 1000000000ULL), 9, QLatin1Char('0'));
        QString outputError;
        if (m_controller->buildOwnedOutput(QStringLiteral("change"),
                                           changeAmount,
                                           &changeOutput,
                                           &changeCommit,
                                           &outputError)) {
            m_controller->storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (!outputError.isEmpty()) {
            m_controller->setLastInfo(QStringLiteral("Change output fallback used: %1").arg(outputError));
        }
    }

    QStringList s1PositiveBlinds;
    if (!changeOutput.blindingFactor.trimmed().isEmpty()) {
        s1PositiveBlinds.append(changeOutput.blindingFactor.trimmed());
    }
    QStringList s1NegativeBlinds;

    s1NegativeBlinds.append(senderContext.blindSecret.trimmed());
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        const QString inputBlind = selection.selectedOutputs.at(i).blindingFactor.trimmed();
        if (!inputBlind.isEmpty()) {
            s1NegativeBlinds.append(inputBlind);
        }
    }

    QString s1OffsetError;
    const QString s1Offset = WalletCryptoBackend::combineBlindingFactors(
        s1PositiveBlinds,
        s1NegativeBlinds,
        &s1OffsetError);
    if (s1Offset.isEmpty()) {
        m_controller->setLastError(s1OffsetError.isEmpty()
                                       ? QStringLiteral("Failed to compute standard sender offset.")
                                       : s1OffsetError);
        return;
    }
    slate.offset = s1Offset;
    localContext.insert(QStringLiteral("standard_sender_offset"), slate.offset);
    m_controller->storeWorkflowContext(workflowId, localContext);

    QList<SlateV4::Commit> s1Commits;
    const QJsonObject s1CoinbaseMap =

        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        const WalletOutput &inp = selection.selectedOutputs.at(i);
        SlateV4::Commit c;
        c.feature = s1CoinbaseMap.value(inp.commitment).toBool(inp.coinbase) ? 1 : 0;
        c.commitment = inp.commitment;
        s1Commits.append(c);
    }
    if (!changeCommit.commitment.isEmpty()) {
        s1Commits.append(changeCommit);
    }
    slate.commitments = GrinWalletWorkflowHelpers::sortedCompactCommitments(s1Commits);

    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());

    SlateV4 outboundSlate = slate;
    outboundSlate.metadata = QJsonObject();
    outboundSlate.metadata.insert(QStringLiteral("external_binary"), true);
    outboundSlate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));

    outboundSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    if (!localSlatepackAddress.trimmed().isEmpty()) {
        outboundSlate.metadata.insert(QStringLiteral("slatepack_sender"), localSlatepackAddress);
    }
    outboundSlate.commitments.clear();
    outboundSlate.hasPaymentProof = false;
    outboundSlate.paymentProof = SlateV4::PaymentProof();

    const QString decoded =
        QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(outboundSlate,
                                              &armoredSlatepack,
                                              &writerError,
                                              localSlatepackAddress,

                                              QStringList(),
                                              m_controller->currentSlatepackSecret())) {
        armoredSlatepack = GrinWalletWorkflowHelpers::encodeSlatepackArmor(
            QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented)),
            localSlatepackAddress);
    }
    m_controller->persistWorkflowTransaction(slate, false);
    m_controller->setWorkflow(slate.workflowId(),
                              slate.modeCode(),
                              slate.stateCode(),
                              armoredSlatepack,
                              decoded);
    m_controller->setLastInfo(
        QStringLiteral("SEND workflow started at S1. Share the generated Slatepack with the receiver."));
}

/**
 * @brief GrinWalletWorkflowService::startReceiveWorkflow
 * @param amount
 * @param note
 */
void GrinWalletWorkflowService::startReceiveWorkflow(const QString &amount, const QString &note)
{
    m_controller->touchWalletSession();
    SlateV4 slate;
    const QString workflowId = slate.id;
    slate.ver.slateVersion = 4;
    slate.ver.blockHeaderVersion = 3;

    const quint64 requestedAmount = GrinWalletWorkflowHelpers::amountToNanogrin(amount);
    if (requestedAmount == 0) {
        m_controller->setLastError(QStringLiteral("Enter a valid amount in GRIN, e.g. 1.000000000."));
        return;
    }

    slate.state = SlateV4::Invoice1;
    slate.amount = GrinWalletWorkflowHelpers::formatNanogrin(requestedAmount);
    slate.offset = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000");

    WalletOutput invoiceOutput;
    SlateV4::Commit invoiceCommit;
    QString outputError;
    if (!m_controller->ensureReceiverOutputContext(workflowId,
                                                   slate.amount,

                                                   QStringLiteral("invoice"),
                                                   &invoiceOutput,
                                                   &invoiceCommit,
                                                   &outputError)) {
        m_controller->setLastError(outputError.isEmpty()
                                       ? QStringLiteral("Failed to derive invoice output.")
                                       : outputError);
        return;
    }

    WalletCryptoBackend::ParticipantContext receiverContext =
        WalletCryptoBackend::createParticipantFromBlindSecret(invoiceOutput.blindingFactor,
                                                              m_controller->seedFingerprint(),
                                                              workflowId,
                                                              QStringLiteral("receiver"));
    if (receiverContext.blindSecret.isEmpty()

        || receiverContext.nonceSecret.isEmpty()
        || receiverContext.blindPublic.isEmpty()
        || receiverContext.noncePublic.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipant(
            m_controller->seedFingerprint(),
            workflowId,
            QStringLiteral("receiver"));
    }
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiverContext));
    slate.commitments.append(invoiceCommit);

    const QString localSlatepackAddress = m_controller->currentSlatepackAddress();
    slate.hasPaymentProof = false;
    slate.paymentProof = SlateV4::PaymentProof();
    slate.metadata.insert(QStringLiteral("external_binary"), true);
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));

    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    if (!localSlatepackAddress.trimmed().isEmpty()) {
        slate.metadata.insert(QStringLiteral("slatepack_sender"), localSlatepackAddress);
    }
    if (!note.trimmed().isEmpty()) {
        slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    }

    SlateV4 outboundSlate = slate;
    outboundSlate.metadata = QJsonObject();
    outboundSlate.metadata.insert(QStringLiteral("external_binary"), true);
    outboundSlate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));

    outboundSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    if (!localSlatepackAddress.trimmed().isEmpty()) {
        outboundSlate.metadata.insert(QStringLiteral("slatepack_sender"), localSlatepackAddress);
    }
    outboundSlate.commitments.clear();
    outboundSlate.hasPaymentProof = false;
    outboundSlate.paymentProof = SlateV4::PaymentProof();

    const QString decoded =
        QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(outboundSlate,
                                              &armoredSlatepack,
                                              &writerError,
                                              localSlatepackAddress,

                                              QStringList(),
                                              m_controller->currentSlatepackSecret())) {
        armoredSlatepack = GrinWalletWorkflowHelpers::encodeSlatepackArmor(
            QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented)),
            localSlatepackAddress);
    }
    m_controller->persistWorkflowTransaction(slate, false);
    m_controller->setWorkflow(slate.workflowId(),
                              slate.modeCode(),
                              slate.stateCode(),
                              armoredSlatepack,
                              decoded);
    m_controller->setLastInfo(
        QStringLiteral("RECEIVE workflow started at I1. Share the invoice Slatepack with the sender."));
}
