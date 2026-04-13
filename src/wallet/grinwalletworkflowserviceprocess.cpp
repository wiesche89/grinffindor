#include "grinwalletworkflowservice.h"

#include <QJsonDocument>

#include "grinwalletcontroller.h"
#include "grinwalletworkflowhelpers.h"
#include "walletkeychain.h"

// -------------------------------------------------------------------------------------------------------
// Decoding And Validating Incoming Slatepacks
// -------------------------------------------------------------------------------------------------------

/**
 * @brief GrinWalletWorkflowService::currentSlatepackDecryptionKey
 * @return
 */
QByteArray GrinWalletWorkflowService::currentSlatepackDecryptionKey() const
{
    if (!m_controller->hasUnlockedSession()) {
        return QByteArray();
    }

    const WalletKeychain keychain = m_controller->sessionKeychain();
    return keychain.isValid() ? keychain.slatepackSecretKey() : QByteArray();
}

/**
 * @brief GrinWalletWorkflowService::decodeWorkflowSlatepack
 * @param slatepack
 * @param decodedOut
 * @param documentOut
 * @return
 */
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

/**
 * @brief GrinWalletWorkflowService::initializeWorkflowSlate
 * @param slatepack
 * @param slate
 * @param incomingSlateOut
 * @param workflowIdOut
 * @param modeOut
 * @param stateOut
 * @param localRoleTagOut
 * @return
 */
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

/**
 * @brief GrinWalletWorkflowService::processWorkflowSlatepack
 * @param slatepack
 */
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

    continueProcessWorkflowSlatepack(&slate, workflowId, mode, state, localRoleTag);
}
