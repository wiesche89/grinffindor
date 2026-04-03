#include <QCoreApplication>
#include <QObject>
#include <cstdio>
#include <QtTest>

QObject *createWalletCoreTests();
QObject *createWalletIntegrationTests();

/**
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int status = 0;

    QObject *coreTests = createWalletCoreTests();
    std::fprintf(stderr, "running WalletCoreTests\n");
    char coreArg0[] = "wallettests";
    char coreArg1[] = "-txt";
    char *coreArgv[] = { coreArg0, coreArg1, nullptr };
    status |= QTest::qExec(coreTests, 2, coreArgv);
    std::fprintf(stderr, "WalletCoreTests status=%d\n", status);
    delete coreTests;

    const char *integrationFunctions[] = {
        "binarySlatepackRoundTripPreservesCoreFields",
        "binarySlatepackFallsBackToPlainSenderEnvelope",
        "workflowFinalizeOutputsTracksInputsAndChange",
        "transactionStoreTracksBroadcastLifecycle",
        "nodeSyncHelpersRecognizeRecoverableAndRefreshableStates",
        "storageRoundTripPreservesPerNetworkViews",
        "storageRefreshTransactionConfirmationsPromotesConfirmedEntries",
        "storageImportBackupNormalizesNetworkAndContexts",
        "controllerImportBackupLoadsWalletState",
        "controllerRejectsSlatepackForWrongNetwork",
        "controllerBroadcastTransactionRequiresSkeleton",
        "controllerCancelTransactionCleansOutputsAndContext",
        "controllerCleanupRemovesLocalOutputsAndCancelledTransactions",
        "controllerReloadPreservesStoredWorkflowState",
        "controllerReloadCanCancelInterruptedWorkflow",
        "nodeSyncRefreshBroadcastStatusesMarksMempoolTransactions",
        "nodeSyncKernelConfirmationFinalizesTrackedWorkflow",
        "nodeSyncPreflightRejectsMissingInputCommitment"
    };
    for (const char *functionName : integrationFunctions) {
        QObject *integrationTests = createWalletIntegrationTests();
        std::fprintf(stderr, "running WalletIntegrationTests::%s\n", functionName);
        char intArg0[] = "wallettests";
        char intArg1[] = "-txt";
        char intArg2[128];
        std::snprintf(intArg2, sizeof(intArg2), "%s", functionName);
        char *intArgv[] = { intArg0, intArg1, intArg2, nullptr };
        const int functionStatus = QTest::qExec(integrationTests, 3, intArgv);
        std::fprintf(stderr, "WalletIntegrationTests::%s status=%d\n", functionName, functionStatus);
        status |= functionStatus;
        delete integrationTests;
    }

    return status;
}
