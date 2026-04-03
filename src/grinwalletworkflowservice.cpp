#include "grinwalletworkflowservice.h"

#include "grinwalletcontroller.h"
#include "grinwalletworkflowhelpers.h"
#include "wallet/walletscanner.h"
#include "wallet/walletselection.h"
#include "wallet/wallettxbuilder.h"
#include "wallet/binaryslatev4writer.h"

#include <QJsonDocument>

GrinWalletWorkflowService::GrinWalletWorkflowService(GrinWalletController *controller)
    : QObject(controller)
    , m_controller(controller)
{
}

QByteArray GrinWalletWorkflowService::currentSlatepackDecryptionKey() const
{
    if (!m_controller->m_walletUnlocked || m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        return QByteArray();
    }

    const WalletKeychain keychain(m_controller->m_sessionMnemonic);
    return keychain.isValid() ? keychain.slatepackSecretKey() : QByteArray();
}

bool GrinWalletWorkflowService::decodeWorkflowSlatepack(const QString &slatepack,
                                                        QString *decodedOut,
                                                        QJsonDocument *documentOut) const
{
    const QString decoded =
        GrinWalletWorkflowHelpers::decodeIncomingSlatepack(slatepack, currentSlatepackDecryptionKey());
    if (decoded.isEmpty()) {
        m_controller->setLastError(QStringLiteral("Incoming Slatepack could not be decoded."));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(decoded.toUtf8());
    if (!document.isObject()) {
        m_controller->setLastError(QStringLiteral("Decoded Slatepack is not valid JSON."));
        return false;
    }
    if (document.object().value(QStringLiteral("encrypted_slatepack")).toBool()) {
        const QString info = document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Encrypted Slatepack could not be decrypted."));
        m_controller->setLastError(info);
        m_controller->setWorkflow(QString(),
                                  QString(),
                                  QString(),
                                  slatepack,
                                  QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return false;
    }
    if (document.object().value(QStringLiteral("external_slatepack")).toBool()) {
        m_controller->setLastError(document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Incoming Slatepack armor was recognized, but payload parsing failed.")));
        m_controller->setWorkflow(QString(),
                                  QString(),
                                  QString(),
                                  slatepack,
                                  QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return false;
    }

    if (decodedOut) {
        *decodedOut = decoded;
    }
    if (documentOut) {
        *documentOut = document;
    }
    return true;
}

bool GrinWalletWorkflowService::initializeWorkflowSlate(const QString &slatepack,
                                                        SlateV4 *slate,
                                                        SlateV4 *incomingSlateOut,
                                                        QString *workflowIdOut,
                                                        QString *modeOut,
                                                        QString *stateOut,
                                                        QString *localRoleTagOut) const
{
    QString decoded;
    QJsonDocument document;
    if (!decodeWorkflowSlatepack(slatepack, &decoded, &document)) {
        return false;
    }

    SlateV4 parsedSlate = SlateV4::fromJson(document.object());
    m_controller->alignSlateVersionWithNode(&parsedSlate);
    const SlateV4 incomingSlate = parsedSlate;
    if (parsedSlate.workflowId().isEmpty() && !parsedSlate.id.isEmpty() && parsedSlate.state != SlateV4::Unknown) {
        parsedSlate.metadata.insert(QStringLiteral("workflow_id"), parsedSlate.id);
        parsedSlate.metadata.insert(QStringLiteral("workflow"),
                                    parsedSlate.metadata.value(QStringLiteral("external_binary")).toBool()
                                        ? QStringLiteral("external-grin-slatepack")
                                        : QStringLiteral("imported-slatepack"));
    }
    if (parsedSlate.network().trimmed().isEmpty()) {
        parsedSlate.metadata.insert(QStringLiteral("network"), m_controller->resolvedNetworkName());
    }
    if (parsedSlate.network().trimmed().toLower() != m_controller->resolvedNetworkName()) {
        m_controller->setLastError(
            QStringLiteral("Incoming Slatepack targets %1, but the wallet is currently set to %2.")
                .arg(parsedSlate.network().trimmed(), m_controller->resolvedNetworkName()));
        return false;
    }

    QString workflowId = parsedSlate.workflowId();
    const QString mode = parsedSlate.modeCode();
    const QString state = parsedSlate.stateCode();
    if (workflowId.isEmpty()) {
        const QString resolvedWorkflowId = m_controller->resolveWorkflowIdBySlateId(parsedSlate);
        if (!resolvedWorkflowId.isEmpty()) {
            workflowId = resolvedWorkflowId;
            parsedSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        }
    }
    if (workflowId.isEmpty() || mode == QStringLiteral("unknown") || state == QStringLiteral("NA")) {
        m_controller->setLastError(QStringLiteral("Incoming Slatepack is missing workflow metadata."));
        return false;
    }

    if (parsedSlate.isFinalState()) {
        m_controller->setWorkflow(workflowId,
                                  mode,
                                  state,
                                  slatepack,
                                  QString::fromUtf8(QJsonDocument(parsedSlate.toJson()).toJson(QJsonDocument::Indented)));
        m_controller->setLastInfo(QStringLiteral("Workflow %1 is already complete at %2.").arg(workflowId, state));
        return false;
    }

    const QString localRoleTag =
        (state == QStringLiteral("S1")) ? QStringLiteral("receiver")
      : (state == QStringLiteral("S2")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I1")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I2")) ? QStringLiteral("receiver")
      : QString();

    if (localRoleTag.isEmpty()) {
        m_controller->setLastError(QStringLiteral("Unsupported workflow transition."));
        return false;
    }

    if (localRoleTag == QStringLiteral("sender")
        && m_controller->workflowContext(workflowId).isEmpty()
        && !parsedSlate.id.trimmed().isEmpty()) {
        const QString resolvedWorkflowId = m_controller->resolveWorkflowIdBySlateId(parsedSlate);
        if (!resolvedWorkflowId.isEmpty() && resolvedWorkflowId != workflowId) {
            workflowId = resolvedWorkflowId;
            parsedSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        }
    }

    if (slate) {
        *slate = parsedSlate;
    }
    if (incomingSlateOut) {
        *incomingSlateOut = incomingSlate;
    }
    if (workflowIdOut) {
        *workflowIdOut = workflowId;
    }
    if (modeOut) {
        *modeOut = mode;
    }
    if (stateOut) {
        *stateOut = state;
    }
    if (localRoleTagOut) {
        *localRoleTagOut = localRoleTag;
    }
    return true;
}

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
    if (!m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_controller->m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < outputs.size(); ++i) {
                outputs[i] = GrinWalletWorkflowHelpers::normalizedTrackedOutput(outputs.at(i), keychain);
            }
        }
    }

    const qulonglong effectiveHeight =
        m_controller->m_chainHeight > 0 ? m_controller->m_chainHeight : m_controller->m_scanHeight;
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
                       WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
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
    slate.metadata.insert(QStringLiteral("wallet"), m_controller->m_walletName);
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
                                                              m_controller->m_seedFingerprint,
                                                              workflowId,
                                                              QStringLiteral("receiver"));
    if (receiverContext.blindSecret.isEmpty()
        || receiverContext.nonceSecret.isEmpty()
        || receiverContext.blindPublic.isEmpty()
        || receiverContext.noncePublic.isEmpty()) {
        receiverContext = WalletCryptoBackend::createParticipant(
            m_controller->m_seedFingerprint,
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

void GrinWalletWorkflowService::processWorkflowSlatepack(const QString &slatepack)
{
    m_controller->touchWalletSession();

    SlateV4 slate;
    SlateV4 incomingSlate;
    QString workflowId;
    QString mode;
    QString state;
    QString localRoleTag;
    if (!initializeWorkflowSlate(slatepack,
                                 &slate,
                                 &incomingSlate,
                                 &workflowId,
                                 &mode,
                                 &state,
                                 &localRoleTag)) {
        return;
    }

    continueProcessWorkflowSlatepack(&slate, incomingSlate, workflowId, mode, state, localRoleTag);
}

void GrinWalletWorkflowService::continueProcessWorkflowSlatepack(SlateV4 *slate,
                                                                 const SlateV4 &incomingSlate,
                                                                 const QString &workflowId,
                                                                 const QString &mode,
                                                                 const QString &state,
                                                                 const QString &localRoleTag)
{
    const QString localSlatepackAddress = m_controller->currentSlatepackAddress();
    const QString localPaymentProofAddress = m_controller->currentPaymentProofAddress();
    if (!localPaymentProofAddress.isEmpty() && slate->hasPaymentProof) {
        if (mode == QStringLiteral("send")) {
            if (slate->paymentProof.senderAddress.trimmed().isEmpty()) {
                slate->paymentProof.senderAddress =
                    (localRoleTag == QStringLiteral("sender")) ? localPaymentProofAddress : slate->paymentProof.senderAddress;
            }
            if (slate->paymentProof.receiverAddress.trimmed().isEmpty()) {
                slate->paymentProof.receiverAddress =
                    (localRoleTag == QStringLiteral("receiver")) ? localPaymentProofAddress : slate->paymentProof.receiverAddress;
            }
        } else if (mode == QStringLiteral("invoice")) {
            if (slate->paymentProof.senderAddress.trimmed().isEmpty()) {
                slate->paymentProof.senderAddress =
                    (localRoleTag == QStringLiteral("sender")) ? localPaymentProofAddress : slate->paymentProof.senderAddress;
            }
            if (slate->paymentProof.receiverAddress.trimmed().isEmpty()) {
                slate->paymentProof.receiverAddress =
                    (localRoleTag == QStringLiteral("receiver")) ? localPaymentProofAddress : slate->paymentProof.receiverAddress;
            }
        }
    }

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
                                                                      m_controller->m_seedFingerprint,
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
                                                                      m_controller->m_seedFingerprint,
                                                                      workflowId,
                                                                      QStringLiteral("receiver"));
            if (receiverContext.blindSecret.isEmpty()) {
                receiverContext = WalletCryptoBackend::createParticipant(
                    m_controller->m_seedFingerprint, workflowId, QStringLiteral("receiver"));
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
                                                   m_controller->m_seedFingerprint,
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
            const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
            if (!selectedCommitments.isEmpty()) {
                const QJsonObject walletState =
                    m_controller->loadDocumentForService().value(QStringLiteral("wallet_state")).toObject();
                const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
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

                WalletOutput receiverOutput;
                for (int ci = 0; ci < slate->commitments.size(); ++ci) {
                    const SlateV4::Commit &c = slate->commitments.at(ci);
                    if (!c.proof.trimmed().isEmpty()
                        && c.commitment != localContext.value(QStringLiteral("change_commit")).toString()) {
                        receiverOutput.commitment = c.commitment;
                        receiverOutput.proof = c.proof;
                        receiverOutput.amount = slate->amount;
                        break;
                    }
                }

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
        && m_controller->m_walletUnlocked
        && !m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_controller->m_sessionMnemonic);
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
        slate->metadata.insert(QStringLiteral("processed_by"), m_controller->m_walletName);
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

            WalletOutput receiverOutput;
            {
                const QString knownChangeCommit = localContext.value(QStringLiteral("change_commit")).toString();
                for (int ci = 0; ci < slate->commitments.size(); ++ci) {
                    const SlateV4::Commit &c = slate->commitments.at(ci);
                    if (!c.proof.trimmed().isEmpty() && c.commitment != knownChangeCommit) {
                        receiverOutput.commitment = c.commitment;
                        receiverOutput.proof = c.proof;
                        receiverOutput.amount = slate->amount;
                        break;
                    }
                }
            }

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
    if (incomingSlate.metadata.value(QStringLiteral("external_binary")).toBool()
        && incomingSlate.modeCode() == QStringLiteral("invoice")
        && incomingSlate.stateCode() == QStringLiteral("I1")
        && nextState == QStringLiteral("I2")) {
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
        if (!m_controller->m_pendingBroadcastWorkflowId.isEmpty()) {
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

void GrinWalletWorkflowService::clearWorkflow()
{
    m_controller->setWorkflow(QString(), QString(), QString(), QString(), QString());
    m_controller->setLastInfo(QStringLiteral("Workflow state cleared."));
}

void GrinWalletWorkflowService::cleanupLocalAndCancelledItems()
{
    m_controller->touchWalletSession();

    if (!m_controller->m_walletUnlocked || m_controller->m_sessionMnemonic.trimmed().isEmpty()) {
        m_controller->setLastError(QStringLiteral("Unlock the wallet before cleaning up."));
        return;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    int localCount = 0;
    for (int i = outputs.size() - 1; i >= 0; --i) {
        if (!outputs.at(i).onChain) {
            outputs.removeAt(i);
            ++localCount;
        }
    }

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    int cancelledCount = 0;
    for (int i = transactions.size() - 1; i >= 0; --i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("status")).toString() == QStringLiteral("cancelled")) {
            transactions.removeAt(i);
            ++cancelledCount;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"),
                       WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    m_controller->setLastInfo(
        QStringLiteral("Cleanup completed: %1 local output(s) and %2 cancelled transaction(s) removed.")
            .arg(QString::number(localCount))
            .arg(QString::number(cancelledCount)));
}

void GrinWalletWorkflowService::broadcastCurrentWorkflowTransaction()
{
    m_controller->touchWalletSession();
    const QJsonDocument workflowDoc = QJsonDocument::fromJson(m_controller->m_workflowDecoded.toUtf8());
    if (!workflowDoc.isObject()) {
        m_controller->setLastError(QStringLiteral("Current workflow does not contain decodable transaction data."));
        return;
    }

    const QJsonObject txSkeleton = workflowDoc.object().value(QStringLiteral("tx_skeleton")).toObject();
    if (txSkeleton.isEmpty()) {
        m_controller->setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
        return;
    }

    if (!m_controller->m_nodeApi) {
        m_controller->connectNodeClient();
    }
    if (!m_controller->m_nodeApi) {
        m_controller->setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    if (!m_controller->m_pendingBroadcastWorkflowId.isEmpty()) {
        m_controller->setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
        return;
    }

    const SlateV4 slate = SlateV4::fromJson(workflowDoc.object());
    m_controller->persistWorkflowTransaction(slate, false);
    m_controller->markTransactionBroadcastPending(slate.workflowId());
    m_controller->m_pendingBroadcastWorkflowId = slate.workflowId();
    m_controller->beginBroadcastWithInputPreflight(slate.workflowId(), txSkeleton);
}

void GrinWalletWorkflowService::broadcastTransaction(const QString &workflowId)
{
    m_controller->touchWalletSession();
    const QJsonArray transactions = m_controller->loadDocumentForService()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        const QJsonObject txSkeleton = tx.value(QStringLiteral("tx_skeleton")).toObject();
        if (txSkeleton.isEmpty()) {
            m_controller->setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
            return;
        }
        if (!m_controller->m_nodeApi) {
            m_controller->connectNodeClient();
        }
        if (!m_controller->m_nodeApi) {
            m_controller->setLastError(QStringLiteral("Node client is not configured."));
            return;
        }
        if (!m_controller->m_pendingBroadcastWorkflowId.isEmpty()) {
            m_controller->setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
            return;
        }

        SlateV4 slate;
        slate.id = tx.value(QStringLiteral("slate_id")).toString();
        slate.amount = tx.value(QStringLiteral("amount")).toString();
        slate.fee = tx.value(QStringLiteral("fee")).toString();
        slate.offset = tx.value(QStringLiteral("offset")).toString();
        slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        slate.setStateFromCode(tx.value(QStringLiteral("state")).toString());
        m_controller->persistWorkflowTransaction(slate, false);
        m_controller->markTransactionBroadcastPending(workflowId);
        m_controller->m_pendingBroadcastWorkflowId = workflowId;
        m_controller->beginBroadcastWithInputPreflight(workflowId, txSkeleton);
        return;
    }

    m_controller->setLastError(QStringLiteral("Transaction not found in wallet history."));
}

void GrinWalletWorkflowService::cancelTransaction(const QString &workflowId)
{
    m_controller->touchWalletSession();
    if (workflowId.trimmed().isEmpty()) {
        m_controller->setLastError(QStringLiteral("Workflow id is required for cancel."));
        return;
    }

    const QJsonArray existingTransactions = m_controller->loadDocumentForService()
                                                .value(QStringLiteral("wallet_state"))
                                                .toObject()
                                                .value(QStringLiteral("transactions"))
                                                .toArray();
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject tx = existingTransactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        if (tx.value(QStringLiteral("confirmations")).toInt() > 0
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("confirmed")) {
            m_controller->setLastError(QStringLiteral("Confirmed transactions can no longer be cancelled."));
            return;
        }
        if (tx.value(QStringLiteral("broadcasted")).toBool()
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("broadcasted")
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("in_mempool")) {
            m_controller->setLastError(QStringLiteral("Broadcasted transactions can no longer be cancelled locally."));
            return;
        }
        break;
    }

    QJsonObject document = m_controller->loadDocumentForService();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonObject localContext = m_controller->workflowContext(workflowId);
    const QJsonArray selectedInputCommits = localContext.value(QStringLiteral("selected_input_commits")).toArray();

    QStringList selectedInputCommitments;
    for (int j = 0; j < selectedInputCommits.size(); ++j) {
        selectedInputCommitments.append(selectedInputCommits.at(j).toString());
    }

    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (outputs[i].spent) {
            continue;
        }

        if (selectedInputCommitments.contains(outputs[i].commitment)) {
            outputs[i].locked = false;
            outputs[i].pending = false;
        } else {
            outputs.removeAt(i);
            --i;
        }
    }

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject tx = transactions.at(i).toObject();
        if (tx.value(QStringLiteral("workflow_id")).toString() == workflowId) {
            tx.insert(QStringLiteral("status"), QStringLiteral("cancelled"));
            tx.insert(QStringLiteral("cancelled_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            transactions.replace(i, tx);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"),
                       WalletScanner::balancesFromOutputs(outputs, m_controller->m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.remove(workflowId);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    m_controller->saveDocumentForService(document);
    m_controller->refreshStateFromStorage();

    if (m_controller->m_workflowId == workflowId) {
        clearWorkflow();
    }
    m_controller->setLastError(QString());
    m_controller->setLastInfo(QStringLiteral("Transaction %1 cancelled and UTXOs released.").arg(workflowId));
}
