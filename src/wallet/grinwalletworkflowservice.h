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
    explicit GrinWalletWorkflowService(GrinWalletController *controller);

    void startSendWorkflow(const QString &amount, const QString &note);
    void startReceiveWorkflow(const QString &amount, const QString &note);
    void processWorkflowSlatepack(const QString &slatepack);
    void clearWorkflow();
    void cleanupLocalAndCancelledItems();
    void broadcastCurrentWorkflowTransaction();
    void broadcastTransaction(const QString &workflowId);
    void cancelTransaction(const QString &workflowId);

private:
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
