#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "binaryslatev4writer.h"
#define private public
#include "grinwalletcontroller.h"
#include "grinwalletnodesyncservice.h"
#undef private
#include "grinwalletnodesync.h"
#include "grinwalletseedcrypto.h"
#include "grinwalletstorage.h"
#include "grinwallettransactionstore.h"
#include "grinwalletworkflow.h"
#include "grinwalletworkflowhelpers.h"
#include "slatev4.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"
#include "walletoutput.h"
#include "walletscanner.h"

namespace
{

/**
 * @brief testMnemonic
 * @return
 */
QString testMnemonic()
{
    return QStringLiteral(
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art");
}

/**
 * @brief repeatedHex
 * @param nibble
 * @param bytes
 * @return
 */
QString repeatedHex(const char nibble, int bytes)
{
    return QString(bytes * 2, QLatin1Char(nibble));
}

/**
 * @brief testKeychain
 * @return
 */
WalletKeychain testKeychain()
{
    return WalletKeychain(testMnemonic());
}

/**
 * @brief secondaryAuditKeychain
 * @return
 */
WalletKeychain secondaryAuditKeychain()
{
    static const QString mnemonic = QStringLiteral(
        "legal winner thank year wave sausage worth useful legal winner thank "
        "year wave sausage worth useful legal winner thank year wave sausage "
        "worth title");
    return WalletKeychain(mnemonic);
}

/**
 * @brief buildBinarySlate
 * @return
 */
SlateV4 buildBinarySlate()
{
    const WalletCryptoBackend::ParticipantContext sender =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
    const WalletCryptoBackend::ParticipantContext receiver =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("receiver"));

    SlateV4 slate;
    slate.state = SlateV4::Standard1;
    slate.amount = QStringLiteral("1.000000000");
    slate.fee = QStringLiteral("0.008000000");
    slate.offset = repeatedHex('6', 32);
    slate.numParticipants = 2;
    slate.metadata.insert(QStringLiteral("workflow_id"), QStringLiteral("integration-workflow"));
    slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    slate.signatures.append(WalletCryptoBackend::createParticipantData(sender));
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiver));
    return slate;
}

/**
 * @brief buildOutput
 * @param commitment
 * @param amount
 * @param onChain
 * @param spent
 * @param locked
 * @param pending
 * @param workflowId
 * @return
 */
WalletOutput buildOutput(const QString &commitment,
                         const QString &amount,
                         bool onChain,
                         bool spent,
                         bool locked,
                         bool pending,
                         const QString &workflowId = QString())
{
    WalletOutput output;
    output.commitment = commitment;
    output.amount = amount;
    output.onChain = onChain;
    output.spent = spent;
    output.locked = locked;
    output.pending = pending;
    output.workflowId = workflowId;
    return output;
}

/**
 * @brief walletStateDocument
 * @param outputs
 * @param transactions
 * @return
 */
QJsonObject walletStateDocument(const QList<WalletOutput> &outputs, const QJsonArray &transactions = QJsonArray())
{
    QJsonObject walletState;
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, 100));
    walletState.insert(QStringLiteral("transactions"), transactions);

    QJsonObject document;
    document.insert(QStringLiteral("wallet_state"), walletState);
    return document;
}

/**
 * @brief encryptedWalletObject
 * @param name
 * @param mnemonic
 * @param password
 * @return
 */
QJsonObject encryptedWalletObject(const QString &name, const QString &mnemonic, const QString &password)
{
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), name);
    wallet.insert(QStringLiteral("seed_fingerprint"), GrinWalletSeedCrypto::seedFingerprint(mnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"),
                  GrinWalletSeedCrypto::encryptMnemonic(mnemonic, password));
    wallet.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    return wallet;
}

/**
 * @brief transactionEntry
 * @param workflowId
 * @param status
 * @param broadcasted
 * @param attempts
 * @return
 */
QJsonObject transactionEntry(const QString &workflowId,
                            const QString &status,
                            bool broadcasted,
                            int attempts = 0)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("workflow_id"), workflowId);
    entry.insert(QStringLiteral("status"), status);
    entry.insert(QStringLiteral("broadcasted"), broadcasted);
    entry.insert(QStringLiteral("broadcast_attempts"), attempts);
    return entry;
}

/**
 * @brief storageFilePathForTests
 * @return
 */
QString storageFilePathForTests()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData + QStringLiteral("/grin-wallet/browser-wallet.json");
}

}

class WalletIntegrationTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void binarySlatepackRoundTripPreservesCoreFields();
    void binarySlatepackFallsBackToPlainSenderEnvelope();
    void workflowFinalizeOutputsTracksInputsAndChange();
    void transactionStoreTracksBroadcastLifecycle();
    void nodeSyncHelpersRecognizeRecoverableAndRefreshableStates();
    void storageRoundTripPreservesPerNetworkViews();
    void storageRefreshTransactionConfirmationsPromotesConfirmedEntries();
    void storageImportBackupNormalizesNetworkAndContexts();
    void controllerImportBackupLoadsWalletState();
    void controllerRejectsSlatepackForWrongNetwork();
    void controllerBroadcastTransactionRequiresSkeleton();
    void controllerCancelTransactionCleansOutputsAndContext();
    void controllerCleanupRemovesLocalOutputsAndCancelledTransactions();
    void controllerReloadPreservesStoredWorkflowState();
    void controllerReloadCanCancelInterruptedWorkflow();
    void nodeSyncRefreshBroadcastStatusesMarksMempoolTransactions();
    void nodeSyncKernelConfirmationFinalizesTrackedWorkflow();
    void nodeSyncPreflightRejectsMissingInputCommitment();
};

/**
 * @brief WalletIntegrationTests::initTestCase
 */
void WalletIntegrationTests::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    Q_INIT_RESOURCE(wallettestresources);
    QVERIFY(QFile::exists(QStringLiteral(":/qml/src/wallet/resources/bip39_english.txt")));
    QFile::remove(storageFilePathForTests());
}

/**
 * @brief WalletIntegrationTests::binarySlatepackRoundTripPreservesCoreFields
 */
void WalletIntegrationTests::binarySlatepackRoundTripPreservesCoreFields()
{
    const WalletKeychain senderKeychain = testKeychain();
    QVERIFY(senderKeychain.isValid());

    SlateV4 slate = buildBinarySlate();
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = WalletCryptoBackend::paymentProofAddress(senderKeychain);
    slate.paymentProof.receiverAddress = WalletCryptoBackend::paymentProofAddress(senderKeychain);

    QString armored;
    QString error;
    QVERIFY2(BinarySlateV4Writer::encodeSlatepack(
                 slate,
                 &armored,
                 &error,
                 WalletCryptoBackend::slatepackAddress(senderKeychain, QStringLiteral("mainnet"))),
             qPrintable(error));
    QVERIFY(armored.contains(QStringLiteral("BEGINSLATEPACK")));

    const QString decoded = GrinWalletWorkflowHelpers::decodeIncomingSlatepack(armored, QByteArray());
    QVERIFY(!decoded.isEmpty());

    const SlateV4 parsed = SlateV4::fromJsonString(decoded);
    QCOMPARE(parsed.id, slate.id);
    QCOMPARE(parsed.state, slate.state);
    QCOMPARE(parsed.amount, slate.amount);
    QCOMPARE(parsed.fee, slate.fee);
    QCOMPARE(parsed.numParticipants, slate.numParticipants);
    QCOMPARE(parsed.signatures.size(), slate.signatures.size());
    QVERIFY(parsed.hasPaymentProof);
    QCOMPARE(parsed.paymentProof.senderAddress, slate.paymentProof.senderAddress);
    QCOMPARE(parsed.paymentProof.receiverAddress, slate.paymentProof.receiverAddress);
    QVERIFY(parsed.metadata.value(QStringLiteral("external_binary")).toBool());
    QCOMPARE(parsed.metadata.value(QStringLiteral("workflow_id")).toString(), slate.id);
}

/**
 * @brief WalletIntegrationTests::binarySlatepackFallsBackToPlainSenderEnvelope
 */
void WalletIntegrationTests::binarySlatepackFallsBackToPlainSenderEnvelope()
{
    const WalletKeychain senderKeychain = testKeychain();
    QVERIFY(senderKeychain.isValid());

    const QString senderAddress =
        WalletCryptoBackend::slatepackAddress(senderKeychain, QStringLiteral("mainnet"));
    const QString receiverAddress =
        WalletCryptoBackend::slatepackAddress(secondaryAuditKeychain(), QStringLiteral("mainnet"));

    SlateV4 slate = buildBinarySlate();
    slate.state = SlateV4::Invoice1;

    QString armored;
    QString error;
    QVERIFY2(BinarySlateV4Writer::encodeSlatepack(slate,
                                                  &armored,
                                                  &error,
                                                  senderAddress,
                                                  QStringList() << receiverAddress,
                                                  senderKeychain.slatepackSecretKey()),
             qPrintable(error));

    const QString decoded = GrinWalletWorkflowHelpers::decodeIncomingSlatepack(armored, QByteArray());
    QVERIFY(!decoded.isEmpty());

    const QJsonDocument decodedDocument = QJsonDocument::fromJson(decoded.toUtf8());
    QVERIFY(decodedDocument.isObject());
    const QJsonObject decodedObject = decodedDocument.object();
    const QJsonArray recipients = decodedObject.value(QStringLiteral("slatepack_recipients")).toArray();
    QCOMPARE(decodedObject.value(QStringLiteral("slatepack_sender")).toString(), senderAddress);
    QVERIFY(recipients.isEmpty());

    const SlateV4 parsed = SlateV4::fromJson(decodedObject);
    QCOMPARE(parsed.id, slate.id);
    QCOMPARE(parsed.state, slate.state);
    QCOMPARE(parsed.amount, slate.amount);
}

/**
 * @brief WalletIntegrationTests::workflowFinalizeOutputsTracksInputsAndChange
 */
void WalletIntegrationTests::workflowFinalizeOutputsTracksInputsAndChange()
{
    const QString workflowId = QStringLiteral("workflow-test-1");
    const QString selectedCommit = repeatedHex('7', 33);
    const QString changeCommit = repeatedHex('8', 33);
    const QString receiverCommit = repeatedHex('9', 33);

    QList<WalletOutput> outputs;
    outputs.append(buildOutput(selectedCommit, QStringLiteral("2.000000000"), true, false, false, false));
    outputs.append(buildOutput(changeCommit, QStringLiteral("0.900000000"), false, false, false, false));
    outputs.append(buildOutput(receiverCommit, QStringLiteral("1.000000000"), false, false, false, false));

    QJsonObject document = walletStateDocument(outputs);

    QJsonObject localContext;
    localContext.insert(QStringLiteral("selected_input_commits"), QJsonArray() << selectedCommit);
    localContext.insert(QStringLiteral("change_commit"), changeCommit);

    SlateV4 slate;
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    SlateV4::Commit commit;
    commit.commitment = receiverCommit;
    commit.proof = repeatedHex('a', 8);
    slate.commitments.append(commit);

    QVERIFY(GrinWalletWorkflow::finalizeOutputs(&document, slate, false, 100, localContext));

    const QList<WalletOutput> finalized =
        WalletScanner::outputsFromState(document.value(QStringLiteral("wallet_state")).toObject());
    QCOMPARE(finalized.size(), 3);
    QCOMPARE(finalized.at(0).workflowId, workflowId);
    QVERIFY(finalized.at(0).pending);
    QVERIFY(finalized.at(0).locked);
    QVERIFY(!finalized.at(0).spent);
    QCOMPARE(finalized.at(1).workflowId, workflowId);
    QVERIFY(finalized.at(1).pending);
    QVERIFY(finalized.at(1).locked);
    QCOMPARE(finalized.at(2).workflowId, workflowId);
    QVERIFY(finalized.at(2).pending);
    QVERIFY(finalized.at(2).locked);

    QVERIFY(GrinWalletWorkflow::finalizeBroadcastedWorkflow(&document, workflowId, 100, localContext));
    const QList<WalletOutput> broadcasted =
        WalletScanner::outputsFromState(document.value(QStringLiteral("wallet_state")).toObject());
    QVERIFY(!broadcasted.at(0).pending);
    QVERIFY(!broadcasted.at(0).locked);
    QVERIFY(broadcasted.at(0).spent);
    QVERIFY(broadcasted.at(1).pending);
    QVERIFY(!broadcasted.at(1).locked);
    QVERIFY(broadcasted.at(2).pending);
    QVERIFY(!broadcasted.at(2).locked);
}

/**
 * @brief WalletIntegrationTests::transactionStoreTracksBroadcastLifecycle
 */
void WalletIntegrationTests::transactionStoreTracksBroadcastLifecycle()
{
    const QString workflowId = QStringLiteral("workflow-test-2");
    QJsonObject pendingEntry = transactionEntry(workflowId, QStringLiteral("ready"), false, 1);
    pendingEntry.insert(QStringLiteral("broadcast_error"), QStringLiteral("old error"));

    QJsonObject mempoolEntry = transactionEntry(QStringLiteral("workflow-test-3"),
                                                QStringLiteral("in_mempool"),
                                                true,
                                                2);

    QJsonArray transactions;
    transactions.append(pendingEntry);
    transactions.append(mempoolEntry);
    QJsonObject document = walletStateDocument(QList<WalletOutput>(), transactions);

    QVERIFY(GrinWalletTransactionStore::markBroadcastPending(&document, workflowId));
    QJsonArray updated = document.value(QStringLiteral("wallet_state")).toObject()
                             .value(QStringLiteral("transactions")).toArray();
    QJsonObject first = updated.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("status")).toString(), QStringLiteral("broadcast_pending"));
    QCOMPARE(first.value(QStringLiteral("broadcast_attempts")).toInt(), 2);
    QVERIFY(!first.contains(QStringLiteral("broadcast_error")));
    QVERIFY(!first.value(QStringLiteral("last_broadcast_attempt")).toString().isEmpty());

    QVERIFY(GrinWalletTransactionStore::markBroadcastFailed(&document,
                                                            workflowId,
                                                            QStringLiteral("node rejected")));
    updated = document.value(QStringLiteral("wallet_state")).toObject()
                  .value(QStringLiteral("transactions")).toArray();
    first = updated.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("status")).toString(), QStringLiteral("broadcast_failed"));
    QCOMPARE(first.value(QStringLiteral("broadcast_error")).toString(), QStringLiteral("node rejected"));

    QVERIFY(GrinWalletTransactionStore::markBroadcastSucceeded(&document, workflowId));
    updated = document.value(QStringLiteral("wallet_state")).toObject()
                  .value(QStringLiteral("transactions")).toArray();
    first = updated.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("status")).toString(), QStringLiteral("broadcasted"));
    QVERIFY(first.value(QStringLiteral("broadcasted")).toBool());
    QVERIFY(!first.value(QStringLiteral("broadcast_at")).toString().isEmpty());
    QVERIFY(!first.contains(QStringLiteral("broadcast_error")));

    QVERIFY(GrinWalletTransactionStore::markKernelBroadcasted(&document,
                                                              QStringLiteral("workflow-test-3")));
    updated = document.value(QStringLiteral("wallet_state")).toObject()
                  .value(QStringLiteral("transactions")).toArray();
    QJsonObject second = updated.at(1).toObject();
    QCOMPARE(second.value(QStringLiteral("status")).toString(), QStringLiteral("in_mempool"));
    QCOMPARE(second.value(QStringLiteral("confirmations")).toInt(), 0);

    QVERIFY(GrinWalletTransactionStore::markKernelConfirmed(&document, workflowId, 20, 17));
    updated = document.value(QStringLiteral("wallet_state")).toObject()
                  .value(QStringLiteral("transactions")).toArray();
    first = updated.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("status")).toString(), QStringLiteral("confirmed"));
    QCOMPARE(first.value(QStringLiteral("confirmed_height")).toVariant().toULongLong(), 17ULL);
    QCOMPARE(first.value(QStringLiteral("confirmations")).toVariant().toULongLong(), 4ULL);
}

/**
 * @brief WalletIntegrationTests::nodeSyncHelpersRecognizeRecoverableAndRefreshableStates
 */
void WalletIntegrationTests::nodeSyncHelpersRecognizeRecoverableAndRefreshableStates()
{
    QJsonArray transactions;
    transactions.append(transactionEntry(QStringLiteral("wf-confirmed"), QStringLiteral("confirmed"), true));
    transactions.append(transactionEntry(QStringLiteral("wf-pending"), QStringLiteral("broadcast_pending"), false));
    transactions.append(transactionEntry(QStringLiteral("wf-mempool"), QStringLiteral("in_mempool"), true));

    QJsonObject walletState;
    walletState.insert(QStringLiteral("transactions"), transactions);
    walletState.insert(QStringLiteral("restore_leaf_index"), QStringLiteral("41"));

    QJsonObject document;
    document.insert(QStringLiteral("wallet_state"), walletState);

    QVERIFY(GrinWalletNodeSync::hasRecoverableBroadcasts(document));
    QVERIFY(GrinWalletNodeSync::shouldRefreshBroadcastStatuses(document));

    const GrinWalletNodeSync::SeedScanState state =
        GrinWalletNodeSync::buildSeedScanState(walletState);
    QCOMPARE(state.nextIndex, 42ULL);
    QVERIFY(state.syncStatus.contains(QStringLiteral("42")));
}

/**
 * @brief WalletIntegrationTests::storageRoundTripPreservesPerNetworkViews
 */
void WalletIntegrationTests::storageRoundTripPreservesPerNetworkViews()
{
    QFile::remove(storageFilePathForTests());

    QJsonObject document = GrinWalletStorage::defaultDocument();

    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("network"), QStringLiteral("testnet"));
    node.insert(QStringLiteral("url"), QStringLiteral("https://testnet.grinffindor.org/v2/foreign"));
    document.insert(QStringLiteral("node"), node);

    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), QStringLiteral("Testnet Wallet"));
    wallet.insert(QStringLiteral("seed_fingerprint"), QStringLiteral("seed-testnet"));
    document.insert(QStringLiteral("wallet"), wallet);

    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions;
    transactions.append(transactionEntry(QStringLiteral("wf-testnet"), QStringLiteral("ready"), false));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);

    QJsonObject workflowContexts;

    workflowContexts.insert(QStringLiteral("wf-testnet"),
                            QJsonObject{{QStringLiteral("context_kind"), QStringLiteral("send")}});
    document.insert(QStringLiteral("workflow_contexts"), workflowContexts);

    QVERIFY(GrinWalletStorage::saveDocument(document));

    const QJsonObject loaded = GrinWalletStorage::loadDocument();
    QCOMPARE(loaded.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
             QStringLiteral("testnet"));
    QCOMPARE(loaded.value(QStringLiteral("wallet")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Testnet Wallet"));
    QCOMPARE(loaded.value(QStringLiteral("workflow_contexts")).toObject().contains(QStringLiteral("wf-testnet")),
             true);
    QCOMPARE(loaded.value(QStringLiteral("wallet_state")).toObject()
                 .value(QStringLiteral("transactions")).toArray().size(),
             1);

    const QJsonObject mainnetState =
        GrinWalletStorage::walletStateForNetwork(loaded, QStringLiteral("mainnet"));
    QCOMPARE(mainnetState.value(QStringLiteral("transactions")).toArray().size(), 0);
    const QJsonObject testnetState =
        GrinWalletStorage::walletStateForNetwork(loaded, QStringLiteral("testnet"));
    QCOMPARE(testnetState.value(QStringLiteral("transactions")).toArray().size(), 1);
}

/**
 * @brief WalletIntegrationTests::storageRefreshTransactionConfirmationsPromotesConfirmedEntries
 */
void WalletIntegrationTests::storageRefreshTransactionConfirmationsPromotesConfirmedEntries()
{
    QJsonArray transactions;
    QJsonObject entry = transactionEntry(QStringLiteral("wf-confirm"), QStringLiteral("broadcasted"), true);
    entry.insert(QStringLiteral("confirmed_height"), 17);
    entry.insert(QStringLiteral("confirmations"), 0);
    transactions.append(entry);

    QJsonObject document = walletStateDocument(QList<WalletOutput>(), transactions);
    QVERIFY(GrinWalletStorage::refreshTransactionConfirmations(&document, 20));

    const QJsonObject updated = document.value(QStringLiteral("wallet_state")).toObject()
                                    .value(QStringLiteral("transactions")).toArray().first().toObject();
    QCOMPARE(updated.value(QStringLiteral("status")).toString(), QStringLiteral("confirmed"));
    QCOMPARE(updated.value(QStringLiteral("confirmations")).toInt(), 4);
}

/**
 * @brief WalletIntegrationTests::storageImportBackupNormalizesNetworkAndContexts
 */
void WalletIntegrationTests::storageImportBackupNormalizesNetworkAndContexts()
{
    const QByteArray backupJson = R"JSON(
{
  "wallet": {
    "name": "Imported Wallet",
    "seed_fingerprint": "seed-imported",
    "encrypted_seed": {
      "version": 1,
      "salt": "salt",
      "nonce": "nonce",
      "cipher": "cipher",
      "mac": "mac"
    }
  },
  "node": {
    "network": "testnet",
    "url": "https://testnet.grinffindor.org/v2/foreign"
  },
  "wallet_state": {
    "scan_height": 12,
    "restore_leaf_index": 34,
    "next_child_index": 9,
    "transactions": [
      {
        "workflow_id": "wf-imported",
        "status": "ready",
        "broadcasted": false
      }
    ]
  },
  "workflow_contexts": {
    "wf-imported": {
      "selected_input_commits": ["abc"]
    }
  }
}
)JSON";

    QString error;
    const QJsonObject imported =
        GrinWalletStorage::extractImportedBackupDocument(backupJson, &error);
    QVERIFY2(!imported.isEmpty(), qPrintable(error));

    QCOMPARE(imported.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
             QStringLiteral("testnet"));
    QCOMPARE(imported.value(QStringLiteral("wallet")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("Imported Wallet"));
    QCOMPARE(imported.value(QStringLiteral("workflow_contexts")).toObject().contains(QStringLiteral("wf-imported")),
             true);
    QCOMPARE(GrinWalletStorage::walletStateForNetwork(imported, QStringLiteral("testnet"))
                 .value(QStringLiteral("transactions")).toArray().size(),
             1);
    QCOMPARE(GrinWalletStorage::walletStateForNetwork(imported, QStringLiteral("mainnet"))
                 .value(QStringLiteral("transactions")).toArray().size(),
             0);
}

/**
 * @brief WalletIntegrationTests::controllerImportBackupLoadsWalletState
 */
void WalletIntegrationTests::controllerImportBackupLoadsWalletState()
{
    QFile::remove(storageFilePathForTests());

    const QString mnemonic = testKeychain().isValid() ? testMnemonic() : QString();
    const QJsonObject encryptedSeed =
        GrinWalletSeedCrypto::encryptMnemonic(mnemonic, QStringLiteral("secret"));
    QVERIFY(!encryptedSeed.isEmpty());

    QJsonObject document;
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), QStringLiteral("Imported Controller Wallet"));
    wallet.insert(QStringLiteral("seed_fingerprint"), GrinWalletSeedCrypto::seedFingerprint(mnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptedSeed);
    document.insert(QStringLiteral("wallet"), wallet);

    QJsonObject node;
    node.insert(QStringLiteral("network"), QStringLiteral("testnet"));
    node.insert(QStringLiteral("url"), QStringLiteral("https://testnet.grinffindor.org/v2/foreign"));
    document.insert(QStringLiteral("node"), node);

    document.insert(QStringLiteral("wallet_state"), GrinWalletStorage::defaultWalletState());
    document.insert(QStringLiteral("workflow_contexts"), QJsonObject());

    QJsonObject backup;
    backup.insert(QStringLiteral("backup_kind"), QStringLiteral("grinffindor.encrypted_wallet_backup"));
    backup.insert(QStringLiteral("document"), document);

    GrinWalletController controller;
    QVERIFY(controller.importEncryptedWalletBackup(
        QString::fromUtf8(QJsonDocument(backup).toJson(QJsonDocument::Indented))));
    QVERIFY(controller.walletExists());
    QVERIFY(!controller.walletUnlocked());
    QCOMPARE(controller.walletName(), QStringLiteral("Imported Controller Wallet"));
    QCOMPARE(controller.selectedNetwork(), QStringLiteral("testnet"));
    QCOMPARE(controller.seedFingerprint(), GrinWalletSeedCrypto::seedFingerprint(mnemonic));
}

/**
 * @brief WalletIntegrationTests::controllerRejectsSlatepackForWrongNetwork
 */
void WalletIntegrationTests::controllerRejectsSlatepackForWrongNetwork()
{
    QFile::remove(storageFilePathForTests());

    GrinWalletController controller;

    SlateV4 slate = buildBinarySlate();
    slate.metadata.insert(QStringLiteral("network"), QStringLiteral("testnet"));
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("imported-slatepack"));

    const QString armored = GrinWalletWorkflowHelpers::encodeSlatepackArmor(
        QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Compact)),
        QString());

    controller.processWorkflowSlatepack(armored);
    QVERIFY(controller.lastError().contains(QStringLiteral("targets testnet"), Qt::CaseInsensitive));
    QVERIFY(controller.lastError().contains(QStringLiteral("mainnet"), Qt::CaseInsensitive));
}

/**
 * @brief WalletIntegrationTests::controllerBroadcastTransactionRequiresSkeleton
 */
void WalletIntegrationTests::controllerBroadcastTransactionRequiresSkeleton()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-broadcast-no-skeleton");
    QJsonArray transactions;
    transactions.append(transactionEntry(workflowId, QStringLiteral("ready"), false));

    QJsonObject document = walletStateDocument(QList<WalletOutput>(), transactions);
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.broadcastTransaction(workflowId);

    QCOMPARE(controller.lastError(), QStringLiteral("No transaction skeleton is available for broadcast."));

    const QJsonArray updatedTransactions = GrinWalletStorage::loadDocument()
                                               .value(QStringLiteral("wallet_state"))
                                               .toObject()
                                               .value(QStringLiteral("transactions"))
                                               .toArray();
    QCOMPARE(updatedTransactions.size(), 1);
    QCOMPARE(updatedTransactions.first().toObject().value(QStringLiteral("status")).toString(),
             QStringLiteral("ready"));
}

/**
 * @brief WalletIntegrationTests::controllerCancelTransactionCleansOutputsAndContext
 */
void WalletIntegrationTests::controllerCancelTransactionCleansOutputsAndContext()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-cancel");
    const QString selectedCommit = repeatedHex('b', 33);
    const QString localCommit = repeatedHex('c', 33);

    QList<WalletOutput> outputs;
    outputs.append(buildOutput(selectedCommit, QStringLiteral("2.000000000"), true, false, true, true, workflowId));
    outputs.append(buildOutput(localCommit, QStringLiteral("0.500000000"), false, false, false, true, workflowId));

    QJsonArray transactions;
    transactions.append(transactionEntry(workflowId, QStringLiteral("ready"), false));

    QJsonObject document = walletStateDocument(outputs, transactions);

    document.insert(QStringLiteral("workflow_contexts"),
                    QJsonObject{
                        {workflowId,
                         QJsonObject{
                             {QStringLiteral("selected_input_commits"), QJsonArray() << selectedCommit}
                         }}
                    });
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.cancelTransaction(workflowId);

    const QJsonObject updated = GrinWalletStorage::loadDocument();
    const QJsonObject updatedWalletState = updated.value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> updatedOutputs = WalletScanner::outputsFromState(updatedWalletState);
    QCOMPARE(updatedOutputs.size(), 1);
    QCOMPARE(updatedOutputs.first().commitment, selectedCommit);
    QVERIFY(!updatedOutputs.first().locked);
    QVERIFY(!updatedOutputs.first().pending);

    const QJsonObject updatedTx =
        updatedWalletState.value(QStringLiteral("transactions")).toArray().first().toObject();
    QCOMPARE(updatedTx.value(QStringLiteral("status")).toString(), QStringLiteral("cancelled"));
    QVERIFY(!updated.value(QStringLiteral("workflow_contexts")).toObject().contains(workflowId));
}

/**
 * @brief WalletIntegrationTests::controllerCleanupRemovesLocalOutputsAndCancelledTransactions
 */
void WalletIntegrationTests::controllerCleanupRemovesLocalOutputsAndCancelledTransactions()
{
    QFile::remove(storageFilePathForTests());

    GrinWalletController controller;
    controller.importWallet(QStringLiteral("Cleanup Wallet"), testMnemonic(), QStringLiteral("secret"));
    QVERIFY(controller.walletUnlocked());
    controller.clearLastError();

    const QString localCommit = repeatedHex('d', 33);
    const QString onChainCommit = repeatedHex('e', 33);

    QList<WalletOutput> outputs;
    outputs.append(buildOutput(localCommit, QStringLiteral("0.500000000"), false, false, false, false));
    outputs.append(buildOutput(onChainCommit, QStringLiteral("2.000000000"), true, false, false, false));

    QJsonArray transactions;
    transactions.append(transactionEntry(QStringLiteral("wf-cancelled"), QStringLiteral("cancelled"), false));
    transactions.append(transactionEntry(QStringLiteral("wf-active"), QStringLiteral("ready"), false));

    QJsonObject document = GrinWalletStorage::loadDocument();
    document.insert(QStringLiteral("wallet_state"), walletStateDocument(outputs, transactions).value(QStringLiteral("wallet_state")).toObject());
    QVERIFY(GrinWalletStorage::saveDocument(document));

    controller.cleanupLocalAndCancelledItems();
    QVERIFY(controller.lastInfo().contains(QStringLiteral("Cleanup completed"), Qt::CaseInsensitive));

    const QJsonObject updated = GrinWalletStorage::loadDocument();
    const QJsonObject updatedWalletState = updated.value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> updatedOutputs = WalletScanner::outputsFromState(updatedWalletState);
    QCOMPARE(updatedOutputs.size(), 1);
    QCOMPARE(updatedOutputs.first().commitment, onChainCommit);

    const QJsonArray updatedTransactions = updatedWalletState.value(QStringLiteral("transactions")).toArray();
    QCOMPARE(updatedTransactions.size(), 1);
    QCOMPARE(updatedTransactions.first().toObject().value(QStringLiteral("workflow_id")).toString(),
             QStringLiteral("wf-active"));
}

/**
 * @brief WalletIntegrationTests::controllerReloadPreservesStoredWorkflowState
 */
void WalletIntegrationTests::controllerReloadPreservesStoredWorkflowState()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-reload");
    QList<WalletOutput> outputs;
    outputs.append(buildOutput(repeatedHex('f', 33),
                               QStringLiteral("1.250000000"),
                               false,
                               false,
                               false,
                               true,
                               workflowId));

    QJsonArray transactions;
    transactions.append(transactionEntry(workflowId, QStringLiteral("ready"), false));

    QJsonObject document = walletStateDocument(outputs, transactions);
    document.insert(QStringLiteral("wallet"),
                    encryptedWalletObject(QStringLiteral("Reload Wallet"),
                                          testMnemonic(),
                                          QStringLiteral("secret")));

    document.insert(QStringLiteral("workflow_contexts"),
                    QJsonObject{
                        {workflowId,
                         QJsonObject{
                             {QStringLiteral("selected_input_commits"), QJsonArray()},
                             {QStringLiteral("amount_display"), QStringLiteral("1.250000000")}
                         }}
                    });
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.loadFromStorage();

    QVERIFY(controller.walletExists());
    QVERIFY(!controller.walletUnlocked());
    QCOMPARE(controller.walletName(), QStringLiteral("Reload Wallet"));
    QCOMPARE(controller.seedFingerprint(), GrinWalletSeedCrypto::seedFingerprint(testMnemonic()));
    QCOMPARE(controller.transactionHistory().size(), 1);
    QCOMPARE(controller.walletOutputs().size(), 1);
    QCOMPARE(GrinWalletStorage::workflowContext(GrinWalletStorage::loadDocument(), workflowId)
                 .value(QStringLiteral("amount_display")).toString(),
             QStringLiteral("1.250000000"));
}

/**
 * @brief WalletIntegrationTests::controllerReloadCanCancelInterruptedWorkflow
 */
void WalletIntegrationTests::controllerReloadCanCancelInterruptedWorkflow()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-reload-cancel");
    const QString selectedCommit = repeatedHex('1', 33);
    const QString localCommit = repeatedHex('2', 33);

    QList<WalletOutput> outputs;
    outputs.append(buildOutput(selectedCommit, QStringLiteral("2.000000000"), true, false, true, true, workflowId));
    outputs.append(buildOutput(localCommit, QStringLiteral("0.400000000"), false, false, false, true, workflowId));

    QJsonArray transactions;
    transactions.append(transactionEntry(workflowId, QStringLiteral("ready"), false));

    QJsonObject document = walletStateDocument(outputs, transactions);
    document.insert(QStringLiteral("wallet"),
                    encryptedWalletObject(QStringLiteral("Reload Cancel Wallet"),
                                          testMnemonic(),
                                          QStringLiteral("secret")));

    document.insert(QStringLiteral("workflow_contexts"),
                    QJsonObject{
                        {workflowId,
                         QJsonObject{
                             {QStringLiteral("selected_input_commits"), QJsonArray() << selectedCommit}
                         }}
                    });
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.loadFromStorage();
    controller.cancelTransaction(workflowId);

    const QJsonObject updated = GrinWalletStorage::loadDocument();
    const QList<WalletOutput> updatedOutputs =
        WalletScanner::outputsFromState(updated.value(QStringLiteral("wallet_state")).toObject());
    QCOMPARE(updatedOutputs.size(), 1);
    QCOMPARE(updatedOutputs.first().commitment, selectedCommit);
    QVERIFY(!updatedOutputs.first().locked);
    QVERIFY(!updatedOutputs.first().pending);
    QVERIFY(!updated.value(QStringLiteral("workflow_contexts")).toObject().contains(workflowId));
}

/**
 * @brief WalletIntegrationTests::nodeSyncRefreshBroadcastStatusesMarksMempoolTransactions
 */
void WalletIntegrationTests::nodeSyncRefreshBroadcastStatusesMarksMempoolTransactions()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-mempool");
    QJsonObject tx = transactionEntry(workflowId, QStringLiteral("broadcasted"), true);
    tx.insert(QStringLiteral("kernel_excess"), repeatedHex('3', 33));

    QJsonArray transactions;
    transactions.append(tx);

    QJsonObject document = walletStateDocument(QList<WalletOutput>(), transactions);
    QVERIFY(GrinWalletStorage::saveDocument(document));

    TxKernel kernel;
    kernel.setExcess(repeatedHex('3', 33));
    TransactionBody body;
    body.setKernels(QVector<TxKernel>() << kernel);
    Transaction transaction;
    transaction.setBody(body);

    PoolEntry poolEntry;
    poolEntry.setTx(transaction);

    GrinWalletController controller;
    controller.loadFromStorage();
    controller.m_nodeSyncService->onNodeUnconfirmedTransactionsFinished(
        Result<QList<PoolEntry>>::ok(QList<PoolEntry>() << poolEntry));

    const QJsonObject updatedTx = GrinWalletStorage::loadDocument()
                                      .value(QStringLiteral("wallet_state"))
                                      .toObject()
                                      .value(QStringLiteral("transactions"))
                                      .toArray()
                                      .first()
                                      .toObject();
    QCOMPARE(updatedTx.value(QStringLiteral("status")).toString(), QStringLiteral("in_mempool"));
    QVERIFY(updatedTx.value(QStringLiteral("broadcasted")).toBool());
}

/**
 * @brief WalletIntegrationTests::nodeSyncKernelConfirmationFinalizesTrackedWorkflow
 */
void WalletIntegrationTests::nodeSyncKernelConfirmationFinalizesTrackedWorkflow()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-kernel-confirm");
    const QString selectedCommit = repeatedHex('4', 33);
    const QString changeCommit = repeatedHex('5', 33);

    QList<WalletOutput> outputs;
    outputs.append(buildOutput(selectedCommit, QStringLiteral("2.000000000"), true, false, true, true, workflowId));
    outputs.append(buildOutput(changeCommit, QStringLiteral("0.900000000"), false, false, true, true, workflowId));

    QJsonObject tx = transactionEntry(workflowId, QStringLiteral("in_mempool"), true);
    tx.insert(QStringLiteral("kernel_excess"), repeatedHex('6', 33));

    QJsonObject document = walletStateDocument(outputs, QJsonArray() << tx);

    document.insert(QStringLiteral("workflow_contexts"),
                    QJsonObject{
                        {workflowId,
                         QJsonObject{
                             {QStringLiteral("selected_input_commits"), QJsonArray() << selectedCommit}
                         }}
                    });
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.loadFromStorage();
    controller.m_chainHeight = 100;
    controller.m_currentKernelWorkflowId = workflowId;
    controller.m_currentKernelExcess = repeatedHex('6', 33);

    LocatedTxKernel locatedKernel;
    locatedKernel.setHeight(97);
    controller.m_nodeSyncService->onNodeKernelFinished(Result<LocatedTxKernel>::ok(locatedKernel));

    const QJsonObject updated = GrinWalletStorage::loadDocument();
    const QJsonObject updatedTx = updated.value(QStringLiteral("wallet_state"))
                                      .toObject()
                                      .value(QStringLiteral("transactions"))
                                      .toArray()
                                      .first()
                                      .toObject();
    QCOMPARE(updatedTx.value(QStringLiteral("status")).toString(), QStringLiteral("confirmed"));
    QCOMPARE(updatedTx.value(QStringLiteral("confirmed_height")).toInt(), 97);

    const QList<WalletOutput> updatedOutputs =
        WalletScanner::outputsFromState(updated.value(QStringLiteral("wallet_state")).toObject());
    QCOMPARE(updatedOutputs.size(), 2);
    QCOMPARE(updatedOutputs.at(0).commitment, selectedCommit);
    QVERIFY(updatedOutputs.at(0).spent);
    QVERIFY(!updatedOutputs.at(0).locked);
    QVERIFY(!updatedOutputs.at(0).pending);
    QCOMPARE(updatedOutputs.at(1).commitment, changeCommit);
    QVERIFY(updatedOutputs.at(1).pending);
    QVERIFY(!updatedOutputs.at(1).locked);
}

/**
 * @brief WalletIntegrationTests::nodeSyncPreflightRejectsMissingInputCommitment
 */
void WalletIntegrationTests::nodeSyncPreflightRejectsMissingInputCommitment()
{
    QFile::remove(storageFilePathForTests());

    const QString workflowId = QStringLiteral("wf-preflight-missing");
    const QString inputCommit = repeatedHex('7', 33);

    QJsonObject tx = transactionEntry(workflowId, QStringLiteral("broadcast_pending"), false, 1);
    QJsonArray transactions;
    transactions.append(tx);

    QJsonObject document = walletStateDocument(QList<WalletOutput>(), transactions);
    QVERIFY(GrinWalletStorage::saveDocument(document));

    GrinWalletController controller;
    controller.loadFromStorage();
    controller.m_pendingBroadcastInputLookup = true;
    controller.m_pendingBroadcastWorkflowId = workflowId;

    controller.m_pendingBroadcastInputCommits = QJsonArray() << inputCommit;
    controller.m_pendingBroadcastTxSkeleton = QJsonObject{
        {QStringLiteral("body"),
         QJsonObject{
             {QStringLiteral("inputs"),
              QJsonArray{
                  QJsonObject{
                      {QStringLiteral("commit"), inputCommit},
                      {QStringLiteral("features"), QStringLiteral("Plain")}
                  }}
             }
         }}
    };

    controller.m_nodeSyncService->onNodeOutputCommitmentsFinished(
        Result<QList<OutputPrintable>>::ok(QList<OutputPrintable>()));

    QVERIFY(!controller.m_pendingBroadcastInputLookup);
    QVERIFY(controller.m_pendingBroadcastWorkflowId.isEmpty());
    QVERIFY(controller.m_pendingBroadcastInputCommits.isEmpty());
    QVERIFY(controller.m_pendingBroadcastTxSkeleton.isEmpty());
    QVERIFY(controller.lastError().contains(QStringLiteral("preflight rejected input commit"),
                                            Qt::CaseInsensitive));

    const QJsonObject updatedTx = GrinWalletStorage::loadDocument()
                                      .value(QStringLiteral("wallet_state"))
                                      .toObject()
                                      .value(QStringLiteral("transactions"))
                                      .toArray()
                                      .first()
                                      .toObject();
    QCOMPARE(updatedTx.value(QStringLiteral("status")).toString(), QStringLiteral("broadcast_failed"));
}

/**
 * @brief createWalletIntegrationTests
 * @return
 */
QObject *createWalletIntegrationTests()
{
    return new WalletIntegrationTests;
}

#include "tst_walletintegration.moc"
