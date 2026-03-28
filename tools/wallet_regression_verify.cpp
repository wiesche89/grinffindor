#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <iostream>

extern "C" {
#include "../3rdparty/monocypher/monocypher.h"
}

#define private public
#include "../src/grinwalletcontroller.h"
#undef private

#include "../src/wallet/walletoutput.h"
#include "../src/wallet/walletselection.h"

namespace {

const int kLegacyIterations = 240000;

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        std::cerr << message.toStdString() << std::endl;
        return false;
    }
    return true;
}

QString storageRootPath()
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty() ? QStringLiteral(".wallet-data") : appData;
}

QString storageFilePath()
{
    return storageRootPath() + QStringLiteral("/grin-wallet/browser-wallet.json");
}

void ensureStorageReady()
{
    const QFileInfo info(storageFilePath());
    QDir().mkpath(info.absolutePath());
}

QJsonObject defaultDocument()
{
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));

    QJsonObject walletState;
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 1);
    walletState.insert(QStringLiteral("next_child_index"), 0);
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());

    QJsonObject root;
    root.insert(QStringLiteral("wallet"), QJsonObject());
    QJsonObject nodeConfig;
    nodeConfig.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
    nodeConfig.insert(QStringLiteral("url"), QStringLiteral("https://mainnet.grinffindor.org/v2/foreign"));
    root.insert(QStringLiteral("node"), nodeConfig);
    root.insert(QStringLiteral("wallet_state"), walletState);
    root.insert(QStringLiteral("workflow_contexts"), QJsonObject());
    return root;
}

bool saveDocument(const QJsonObject &document)
{
    ensureStorageReady();
    QSaveFile file(storageFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(document).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject loadDocument()
{
    QFile file(storageFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return defaultDocument();
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : defaultDocument();
}

QString normalizeMnemonic(const QString &mnemonic)
{
    QString normalized = mnemonic.toLower().trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

QByteArray deriveKeyMaterialV2(const QString &password, const QByteArray &salt, int iterations, int outputLength)
{
    const QByteArray material = password.toUtf8() + salt;
    QByteArray state = QCryptographicHash::hash(material, QCryptographicHash::Sha512);
    for (int i = 0; i < iterations; ++i) {
        state = QCryptographicHash::hash(state + material + QByteArray::number(i), QCryptographicHash::Sha512);
    }

    QByteArray output;
    output.reserve(outputLength);
    QByteArray blockSeed = state;
    quint32 counter = 0;
    while (output.size() < outputLength) {
        blockSeed = QCryptographicHash::hash(
            blockSeed + material + QByteArray::number(counter++),
            QCryptographicHash::Sha512);
        output.append(blockSeed);
    }
    output.truncate(outputLength);
    return output;
}

QByteArray xorStream(const QByteArray &data, const QByteArray &key, const QByteArray &nonce)
{
    QByteArray output(data.size(), Qt::Uninitialized);
    int offset = 0;
    quint32 counter = 0;
    while (offset < data.size()) {
        const QByteArray block = QCryptographicHash::hash(
            key + nonce + QByteArray::number(counter++), QCryptographicHash::Sha256);
        for (int i = 0; i < block.size() && offset < data.size(); ++i, ++offset) {
            output[offset] = static_cast<char>(
                static_cast<unsigned char>(data.at(offset)) ^ static_cast<unsigned char>(block.at(i)));
        }
    }
    return output;
}

QJsonObject encryptMnemonicV2(const QString &mnemonic, const QString &password)
{
    const QByteArray salt("0123456789ABCDEF");
    const QByteArray nonce("ABCDEFGHIJKLMNOPQRSTUVWX");
    const QByteArray keyMaterial = deriveKeyMaterialV2(password, salt, kLegacyIterations, 64);
    const QByteArray encryptionKey = keyMaterial.left(32);
    const QByteArray macKey = keyMaterial.mid(32, 32);
    const QByteArray plain = normalizeMnemonic(mnemonic).toUtf8();
    const QByteArray cipher = xorStream(plain, encryptionKey, nonce);
    const QByteArray mac = QCryptographicHash::hash(macKey + nonce + cipher + macKey, QCryptographicHash::Sha256);

    QJsonObject encrypted;
    encrypted.insert(QStringLiteral("version"), 2);
    encrypted.insert(QStringLiteral("kdf_iterations"), kLegacyIterations);
    encrypted.insert(QStringLiteral("salt"), QString::fromUtf8(salt.toBase64()));
    encrypted.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
    encrypted.insert(QStringLiteral("cipher"), QString::fromUtf8(cipher.toBase64()));
    encrypted.insert(QStringLiteral("mac"), QString::fromUtf8(mac.toBase64()));
    return encrypted;
}

WalletOutput makeOutput(const QString &commitment,
                        const QString &amount,
                        const QString &source,
                        quint64 height,
                        bool onChain,
                        bool spent,
                        bool locked,
                        bool pending,
                        const QString &workflowId = QString())
{
    WalletOutput output;
    output.commitment = commitment;
    output.amount = amount;
    output.source = source;
    output.height = height;
    output.onChain = onChain;
    output.spent = spent;
    output.locked = locked;
    output.pending = pending;
    output.workflowId = workflowId;
    return output;
}

bool verifyLegacySeedUpgrade()
{
    const QString password = QStringLiteral("secret-pass");
    GrinWalletController seedController;
    const QString mnemonic = seedController.generateMnemonic();
    if (!expect(seedController.validateMnemonic(mnemonic),
                QStringLiteral("Regression test should generate a valid mnemonic."))) {
        return false;
    }

    QJsonObject document = defaultDocument();
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), QStringLiteral("legacy-test"));
    wallet.insert(QStringLiteral("seed_fingerprint"), QStringLiteral("legacyfingerprint"));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptMnemonicV2(mnemonic, password));
    document.insert(QStringLiteral("wallet"), wallet);
    if (!expect(saveDocument(document), QStringLiteral("Legacy wallet document should be writable."))) {
        return false;
    }

    GrinWalletController controller;
    controller.initialize();
    controller.unlockWallet(password);
    if (!expect(controller.walletUnlocked(),
                QStringLiteral("Controller should unlock a legacy v2 wallet."))) {
        return false;
    }

    const QJsonObject upgraded = loadDocument()
                                     .value(QStringLiteral("wallet"))
                                     .toObject()
                                     .value(QStringLiteral("encrypted_seed"))
                                     .toObject();
    return expect(upgraded.value(QStringLiteral("version")).toInt() == 3,
                  QStringLiteral("Legacy wallet unlock should migrate encrypted seed to version 3."));
}

bool verifyCoinSelectionRegression()
{
    QList<WalletOutput> outputs;
    outputs.append(makeOutput(QStringLiteral("c1"), QStringLiteral("4.000000000"), QStringLiteral("receive"), 100, true, false, false, false));
    outputs.append(makeOutput(QStringLiteral("c2"), QStringLiteral("3.000000000"), QStringLiteral("receive"), 100, true, false, false, false));
    outputs.append(makeOutput(QStringLiteral("c3"), QStringLiteral("1.000000000"), QStringLiteral("receive"), 100, true, false, false, false));

    const quint64 amount = 3900000000ULL;
    const WalletSelection::Result result = WalletSelection::selectSpendableOutputs(outputs, amount, 200);
    if (!expect(result.success, QStringLiteral("Coin selection should succeed for exact-ish single-input case."))) {
        return false;
    }

    return expect(result.selectedOutputs.size() == 1,
                  QStringLiteral("Coin selection should prefer a single exact-match input when possible."))
        && expect(result.selectedOutputs.first().commitment == QStringLiteral("c1"),
                  QStringLiteral("Coin selection should choose the 4 grin output for a 3.9 grin send."))
        && expect(result.change == 72000000ULL,
                  QStringLiteral("Coin selection should compute the no-change fee path correctly."));
}

bool verifyBroadcastCancelProtection()
{
    QJsonObject document = defaultDocument();
    QJsonObject wallet;
    wallet.insert(QStringLiteral("name"), QStringLiteral("cancel-test"));
    wallet.insert(QStringLiteral("seed_fingerprint"), QStringLiteral("fingerprint"));
    wallet.insert(QStringLiteral("encrypted_seed"), QJsonObject());
    document.insert(QStringLiteral("wallet"), wallet);

    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions;
    QJsonObject tx;
    tx.insert(QStringLiteral("workflow_id"), QStringLiteral("wf-broadcasted"));
    tx.insert(QStringLiteral("status"), QStringLiteral("broadcasted"));
    tx.insert(QStringLiteral("broadcasted"), true);
    tx.insert(QStringLiteral("confirmations"), 0);
    transactions.append(tx);
    walletState.insert(QStringLiteral("transactions"), transactions);

    QList<WalletOutput> outputs;
    outputs.append(makeOutput(QStringLiteral("commit-1"),
                              QStringLiteral("1.000000000"),
                              QStringLiteral("receive"),
                              100,
                              true,
                              false,
                              true,
                              false,
                              QStringLiteral("wf-broadcasted")));
    QJsonArray outputArray;
    for (int i = 0; i < outputs.size(); ++i) {
        outputArray.append(outputs.at(i).toJson());
    }
    walletState.insert(QStringLiteral("outputs"), outputArray);
    document.insert(QStringLiteral("wallet_state"), walletState);
    if (!expect(saveDocument(document), QStringLiteral("Cancel regression document should be writable."))) {
        return false;
    }

    GrinWalletController controller;
    controller.initialize();
    controller.cancelTransaction(QStringLiteral("wf-broadcasted"));

    const QJsonObject after = loadDocument();
    const QJsonArray afterTransactions = after.value(QStringLiteral("wallet_state")).toObject().value(QStringLiteral("transactions")).toArray();
    const QJsonObject afterTx = afterTransactions.first().toObject();
    const QJsonArray afterOutputs = after.value(QStringLiteral("wallet_state")).toObject().value(QStringLiteral("outputs")).toArray();
    const QJsonObject afterOutput = afterOutputs.first().toObject();

    return expect(controller.lastError().contains(QStringLiteral("Broadcasted transactions")),
                  QStringLiteral("Cancelling a broadcasted transaction should set a blocking error."))
        && expect(afterTx.value(QStringLiteral("status")).toString() == QStringLiteral("broadcasted"),
                  QStringLiteral("Broadcasted transaction status must remain unchanged after cancel attempt."))
        && expect(afterOutput.value(QStringLiteral("locked")).toBool(),
                  QStringLiteral("Broadcasted transaction inputs must remain locked after cancel attempt."));
}

bool verifyHistoryRebuildKeepsBroadcastedState()
{
    GrinWalletController controller;
    controller.m_chainHeight = 250;

    QList<WalletOutput> outputs;
    outputs.append(makeOutput(QStringLiteral("commit-send"),
                              QStringLiteral("2.000000000"),
                              QStringLiteral("change"),
                              200,
                              true,
                              false,
                              false,
                              false,
                              QStringLiteral("wf-send")));

    QJsonArray existingTransactions;
    QJsonObject tx;
    tx.insert(QStringLiteral("workflow_id"), QStringLiteral("wf-send"));
    tx.insert(QStringLiteral("mode"), QStringLiteral("send"));
    tx.insert(QStringLiteral("status"), QStringLiteral("broadcasted"));
    tx.insert(QStringLiteral("broadcasted"), true);
    tx.insert(QStringLiteral("amount"), QStringLiteral("2.000000000"));
    tx.insert(QStringLiteral("fee"), QStringLiteral("0.001000000"));
    tx.insert(QStringLiteral("timestamp"), QStringLiteral("2026-03-28T10:00:00Z"));
    existingTransactions.append(tx);

    const QJsonArray rebuilt = controller.rebuildTransactionHistoryFromOutputs(outputs, existingTransactions);
    if (!expect(rebuilt.size() == 1, QStringLiteral("History rebuild should keep exactly one transaction entry."))) {
        return false;
    }

    const QJsonObject rebuiltTx = rebuilt.first().toObject();
    return expect(rebuiltTx.value(QStringLiteral("broadcasted")).toBool(),
                  QStringLiteral("History rebuild should preserve broadcasted=true for known send transactions."))
        && expect(rebuiltTx.value(QStringLiteral("status")).toString() == QStringLiteral("broadcasted"),
                  QStringLiteral("History rebuild should preserve broadcasted status for known send transactions."))
        && expect(rebuiltTx.value(QStringLiteral("workflow_id")).toString() == QStringLiteral("wf-send"),
                  QStringLiteral("History rebuild should preserve workflow ids."));
}

bool verifyEncryptedBackupRoundTrip()
{
    QFile::remove(storageFilePath());

    GrinWalletController creator;
    creator.initialize();
    creator.restoreWallet(QStringLiteral("backup-wallet"),
                          QStringLiteral("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art"),
                          QStringLiteral("backup-secret"));
    if (!expect(creator.walletUnlocked(),
                QStringLiteral("Backup roundtrip setup wallet should unlock after restore."))) {
        return false;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    walletState.insert(QStringLiteral("scan_height"), 777);
    QJsonArray transactions;
    QJsonObject tx;
    tx.insert(QStringLiteral("workflow_id"), QStringLiteral("wf-backup"));
    tx.insert(QStringLiteral("status"), QStringLiteral("broadcasted"));
    tx.insert(QStringLiteral("broadcasted"), true);
    transactions.append(tx);
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    QJsonObject walletStates = document.value(QStringLiteral("wallet_states")).toObject();
    walletStates.insert(QStringLiteral("mainnet"), walletState);
    document.insert(QStringLiteral("wallet_states"), walletStates);
    saveDocument(document);

    const QString backupJson = creator.exportEncryptedWalletBackup();
    if (!expect(backupJson.contains(QStringLiteral("grinffindor.encrypted_wallet_backup")),
                QStringLiteral("Exported backup should include the backup envelope."))) {
        return false;
    }

    QFile::remove(storageFilePath());

    GrinWalletController importer;
    importer.initialize();
    if (!expect(importer.importEncryptedWalletBackup(backupJson),
                QStringLiteral("Controller should import a previously exported encrypted backup."))) {
        return false;
    }
    if (!expect(importer.walletExists(),
                QStringLiteral("Imported encrypted backup should recreate wallet presence."))) {
        return false;
    }
    if (!expect(!importer.walletUnlocked(),
                QStringLiteral("Imported encrypted backup should remain locked until password unlock."))) {
        return false;
    }

    const QJsonObject imported = loadDocument();
    const QJsonObject importedMainnetState =
        imported.value(QStringLiteral("wallet_states")).toObject().value(QStringLiteral("mainnet")).toObject();
    return expect(imported.value(QStringLiteral("wallet")).toObject().value(QStringLiteral("name")).toString()
                      == QStringLiteral("backup-wallet"),
                  QStringLiteral("Imported backup should restore wallet name."))
        && expect(importedMainnetState.value(QStringLiteral("scan_height")).toInt() == 777,
                  QStringLiteral("Imported backup should restore wallet scan height."))
        && expect(importedMainnetState.value(QStringLiteral("transactions")).toArray().size() == 1,
                  QStringLiteral("Imported backup should restore transaction history."));
}

bool verifyTestnetNetworkSwitch()
{
    QFile::remove(storageFilePath());

    GrinWalletController controller;
    controller.initialize();
    controller.restoreWallet(QStringLiteral("testnet-wallet"),
                             QStringLiteral("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art"),
                             QStringLiteral("network-secret"));
    if (!expect(controller.walletUnlocked(),
                QStringLiteral("Network switch test wallet should unlock after restore."))) {
        return false;
    }
    QJsonObject document = loadDocument();
    QJsonObject mainnetState = document.value(QStringLiteral("wallet_state")).toObject();
    mainnetState.insert(QStringLiteral("scan_height"), 321);
    QJsonArray mainnetTransactions;
    QJsonObject mainnetTx;
    mainnetTx.insert(QStringLiteral("workflow_id"), QStringLiteral("wf-mainnet"));
    mainnetTransactions.append(mainnetTx);
    mainnetState.insert(QStringLiteral("transactions"), mainnetTransactions);
    document.insert(QStringLiteral("wallet_state"), mainnetState);
    QJsonObject walletStates = document.value(QStringLiteral("wallet_states")).toObject();
    walletStates.insert(QStringLiteral("mainnet"), mainnetState);
    document.insert(QStringLiteral("wallet_states"), walletStates);
    saveDocument(document);

    if (!expect(controller.setSelectedNetwork(QStringLiteral("testnet")),
                QStringLiteral("Controller should switch wallet network to testnet."))) {
        return false;
    }

    document = loadDocument();
    const QJsonObject storedNode = document.value(QStringLiteral("node")).toObject();
    if (!expect(storedNode.value(QStringLiteral("network")).toString() == QStringLiteral("testnet"),
                QStringLiteral("Wallet document should persist the selected testnet network."))) {
        return false;
    }
    if (!expect(storedNode.value(QStringLiteral("url")).toString() == QStringLiteral("https://testnet.grinffindor.org/v2/foreign"),
                QStringLiteral("Wallet document should reset the node URL to the Grinffindor testnet endpoint."))) {
        return false;
    }

    const QString decodedTemplate = controller.decodeSlatepack(controller.createSlatepackTemplate());
    const QJsonDocument templateDoc = QJsonDocument::fromJson(decodedTemplate.toUtf8());
    if (!expect(templateDoc.isObject(),
                QStringLiteral("Testnet Slatepack template should decode to a JSON object."))) {
        return false;
    }

    if (!expect(templateDoc.object().value(QStringLiteral("network")).toString() == QStringLiteral("testnet"),
                QStringLiteral("Generated Slatepack template should carry the selected testnet network metadata."))) {
        return false;
    }

    const QJsonObject activeTestnetState = document.value(QStringLiteral("wallet_state")).toObject();
    if (!expect(activeTestnetState.value(QStringLiteral("scan_height")).toInt() == 0,
                QStringLiteral("Switching to testnet should activate an isolated wallet state."))) {
        return false;
    }

    if (!expect(controller.setSelectedNetwork(QStringLiteral("mainnet")),
                QStringLiteral("Controller should switch back to mainnet."))) {
        return false;
    }

    document = loadDocument();
    return expect(document.value(QStringLiteral("wallet_state")).toObject().value(QStringLiteral("scan_height")).toInt() == 321,
                  QStringLiteral("Switching back to mainnet should restore the previous mainnet wallet state."))
        && expect(document.value(QStringLiteral("wallet_state")).toObject().value(QStringLiteral("transactions")).toArray().size() == 1,
                  QStringLiteral("Switching back to mainnet should restore the previous mainnet transaction history."));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Grinffindor"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("grinffindor.org"));
    QCoreApplication::setApplicationName(QStringLiteral("wallet_regression_verify"));
    QStandardPaths::setTestModeEnabled(true);

    ensureStorageReady();

    const bool ok = verifyLegacySeedUpgrade()
        && verifyCoinSelectionRegression()
        && verifyBroadcastCancelProtection()
        && verifyHistoryRebuildKeepsBroadcastedState()
        && verifyEncryptedBackupRoundTrip()
        && verifyTestnetNetworkSwitch();

    QFile::remove(storageFilePath());

    if (!ok) {
        return 1;
    }

    std::cout << "Wallet regression verification passed." << std::endl;
    return 0;
}
