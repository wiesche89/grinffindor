#ifndef GRINWALLETWORKFLOWSERVICE_H
#define GRINWALLETWORKFLOWSERVICE_H

#include <QObject>

class QByteArray;
class QJsonDocument;
class QString;

class SlateV4;
class GrinWalletController;

class GrinWalletWorkflowService : public QObject
{
    Q_OBJECT

public:
/**
 * @brief Coordinates send, receive, and slatepack workflow execution.
 */
    explicit GrinWalletWorkflowService(GrinWalletController *controller);

/**
 * @brief Starts send workflow.
 */
    void startSendWorkflow(const QString &amount, const QString &note);
/**
 * @brief Starts receive workflow.
 */
    void startReceiveWorkflow(const QString &amount, const QString &note);
/**
 * @brief Returns process workflow slatepack.
 */
    void processWorkflowSlatepack(const QString &slatepack);
/**
 * @brief Clears workflow.
 */
    void clearWorkflow();
/**
 * @brief Processes cleanup local and cancelled items.
 */
    void cleanupLocalAndCancelledItems();
/**
 * @brief Processes broadcast current workflow transaction.
 */
    void broadcastCurrentWorkflowTransaction();
/**
 * @brief Returns broadcast transaction.
 */
    void broadcastTransaction(const QString &workflowId);
/**
 * @brief Returns whether cel transaction.
 */
    void cancelTransaction(const QString &workflowId);

private:
/**
 * @brief Returns current slatepack decryption key.
 */
    QByteArray currentSlatepackDecryptionKey() const;
    bool decodeWorkflowSlatepack(const QString &slatepack,
                                 QString *decodedOut,
                                 QJsonDocument *documentOut) const;
    bool initializeWorkflowSlate(const QString &slatepack,
                                 SlateV4 *slate,
                                 SlateV4 *incomingSlateOut,
                                 QString *workflowIdOut,
                                 QString *modeOut,
                                 QString *stateOut,
                                 QString *localRoleTagOut) const;
    void continueProcessWorkflowSlatepack(SlateV4 *slate,
                                          const QString &workflowId,
                                          const QString &mode,
                                          const QString &state,
                                          const QString &localRoleTag);
    void populatePaymentProofAddresses(SlateV4 *slate,
                                       const QString &mode,
                                       const QString &localRoleTag,
                                       const QString &localPaymentProofAddress) const;

    GrinWalletController *m_controller;
};

#endif
