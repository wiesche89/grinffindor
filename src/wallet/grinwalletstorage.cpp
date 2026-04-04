#include "grinwalletstorage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

#include "walletscanner.h"

#ifdef Q_OS_WASM

/**
 * @brief Writes a JSON blob to browser localStorage under the provided key.
 * @param key Storage key.
 * @param value JSON payload string.
 */
EM_JS(void, browserLocalStorageSet, (const char *key, const char *value), {
    if (typeof localStorage === 'undefined' || !key || !value) {
        return;
    }
    localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
});

/**
 * @brief Reads a JSON blob from browser localStorage for the provided key.
 * @param key Storage key.
 * @return Newly allocated UTF-8 string or null.
 */
EM_JS(char *, browserLocalStorageGet, (const char *key), {
    if (typeof localStorage === 'undefined' || !key) {
        return 0;
    }

    const value = localStorage.getItem(UTF8ToString(key));
    if (value === null || value === undefined) {
        return 0;
    }
    return stringToNewUTF8(value);
});
#endif

namespace {

const char *kAppSettingsKey = "app_settings";
const char *kAutoLockOnDeactivateKey = "auto_lock_on_app_deactivate";
const char *kWalletStorePath = "/grin-wallet/browser-wallet.json";
#ifdef Q_OS_WASM
const char *kWalletLocalStorageKey = "grinffindor.browserWallet";
#endif
const char *kMainnetNodeUrl = "https://mainnet.grinffindor.org/v2/foreign";
const char *kTestnetNodeUrl = "https://testnet.grinffindor.org/v2/foreign";

/**
 * @brief Returns the default wallet network name.
 * @return Default network identifier.
 */
QString defaultNetworkName()
{
    return QStringLiteral("mainnet");
}

/**
 * @brief Checks whether a network name is supported.
 * @param networkName Candidate network name.
 * @return True when network is mainnet or testnet.
 */
bool isAcceptedNetworkName(const QString &networkName)
{
    const QString normalized = networkName.trimmed().toLower();
    return normalized == QStringLiteral("mainnet") || normalized == QStringLiteral("testnet");
}

/**
 * @brief Resolves default node URL for the given network.
 * @param networkName Network identifier.
 * @return Default node foreign API URL.
 */
QString defaultNodeUrlForNetwork(const QString &networkName)
{
    return networkName.trimmed().toLower() == QStringLiteral("testnet")
        ? QString::fromUtf8(kTestnetNodeUrl)
        : QString::fromUtf8(kMainnetNodeUrl);
}

/**
 * @brief Infers network name from explicit name or node URL.
 * @param networkName Optional explicit network value.
 * @param nodeUrl Optional node URL used as fallback hint.
 * @return Normalized network identifier.
 */
QString inferNetworkName(const QString &networkName, const QString &nodeUrl)
{
    const QString normalizedNetwork = networkName.trimmed().toLower();
    if (isAcceptedNetworkName(normalizedNetwork)) {
        return normalizedNetwork;
    }

    const QString normalizedUrl = nodeUrl.trimmed().toLower();
    if (normalizedUrl.contains(QStringLiteral("testnet."))) {
        return QStringLiteral("testnet");
    }

    return defaultNetworkName();
}

/**
 * @brief Reads an amount string from balances object with a default fallback.
 * @param balances Balance object.
 * @param key Balance field key.
 * @return Amount string.
 */
QString amountStringFromJson(const QJsonObject &balances, const QString &key)
{
    return balances.value(key).toString(QStringLiteral("0.000000000"));
}

/**
 * @brief Validates whether a node URL has acceptable scheme and host.
 * @param nodeUrl Candidate node URL.
 * @return True when URL is valid HTTP/HTTPS endpoint.
 */
bool isNodeUrlAccepted(const QString &nodeUrl)
{
    const QUrl parsed = QUrl::fromUserInput(nodeUrl.trimmed());
    return parsed.isValid()
        && !parsed.scheme().trimmed().isEmpty()
        && !parsed.host().trimmed().isEmpty()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"));
}

/**
 * @brief Resolves the root path for wallet storage.
 * @return Storage root directory path.
 */
QString storageRootPath()
{
#ifdef Q_OS_WASM
    return QStringLiteral("/persistent");
#else
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty() ? QStringLiteral(".wallet-data") : appData;
#endif
}

/**
 * @brief Resolves the full wallet storage file path.
 * @return Absolute storage file path.
 */
QString storageFilePath()
{
    return storageRootPath() + QString::fromUtf8(kWalletStorePath);
}

/**
 * @brief Creates storage directories and mounts persistent filesystem on WASM.
 */
void ensureStorageReady()
{
    const QFileInfo info(storageFilePath());
    QDir().mkpath(info.absolutePath());
#ifdef Q_OS_WASM
    EM_ASM({
        if (typeof FS !== "undefined" && typeof IDBFS !== "undefined") {
            try {
                FS.mkdir('/persistent');
            } catch (e) {}
            if (!Module.grinWalletIdbMounted) {
                FS.mount(IDBFS, {}, '/persistent');
                Module.grinWalletIdbMounted = true;
                FS.syncfs(true, function(err) {});
            }
        }
    });
#endif
}

/**
 * @brief Flushes pending WASM filesystem changes to persistent backing store.
 */
void flushStorage()
{
#ifdef Q_OS_WASM
    EM_ASM({
        if (typeof FS !== "undefined" && Module.grinWalletIdbMounted) {
            FS.syncfs(false, function(err) {});
        }
    });
#endif
}

/**
 * @brief Validates required fields of an encrypted seed backup object.
 * @param encryptedSeed Encrypted seed object.
 * @param errorOut Optional validation error output.
 * @return True when backup seed object is valid.
 */
bool validateEncryptedSeedObject(const QJsonObject &encryptedSeed, QString *errorOut)
{
    const int version = encryptedSeed.value(QStringLiteral("version")).toInt();
    const QStringList requiredFields = {
        QStringLiteral("salt"),
        QStringLiteral("nonce"),
        QStringLiteral("cipher"),
        QStringLiteral("mac")
    };

    if (version < 1) {
        if (errorOut) {
            *errorOut = QStringLiteral("Backup seed payload is missing a valid encryption version.");
        }
        return false;
    }

    for (int i = 0; i < requiredFields.size(); ++i) {
        if (encryptedSeed.value(requiredFields.at(i)).toString().trimmed().isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Backup seed payload is incomplete.");
            }
            return false;
        }
    }

    return true;
}

/**
 * @brief Normalizes an imported backup candidate into current storage schema.
 * @param candidate Raw imported backup object.
 * @param documentOut Output normalized document.
 * @param errorOut Optional normalization error output.
 * @return True when normalization succeeds.
 */
bool normalizeImportedDocument(const QJsonObject &candidate, QJsonObject *documentOut, QString *errorOut)
{
    if (!documentOut) {
        return false;
    }

    // -------------------------------------------------------------------------------------------------------
    // Validating Wallet Metadata And Seed Payload
    // -------------------------------------------------------------------------------------------------------
    QJsonObject document = GrinWalletStorage::defaultDocument();
    QJsonObject wallet = candidate.value(QStringLiteral("wallet")).toObject();
    if (wallet.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Backup does not contain a wallet section.");
        }
        return false;
    }

    const QJsonObject encryptedSeed = wallet.value(QStringLiteral("encrypted_seed")).toObject();
    if (!validateEncryptedSeedObject(encryptedSeed, errorOut)) {
        return false;
    }

    const QString walletName = wallet.value(QStringLiteral("name")).toString().trimmed();
    if (walletName.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Backup wallet name is missing.");
        }
        return false;
    }

    // -------------------------------------------------------------------------------------------------------
    // Normalizing Network And Node Configuration
    // -------------------------------------------------------------------------------------------------------
    QJsonObject node = candidate.value(QStringLiteral("node")).toObject();
    const QString importedNodeUrl = node.value(QStringLiteral("url")).toString().trimmed();
    QJsonObject normalizedNode = document.value(QStringLiteral("node")).toObject();
    const QString importedNetwork =
        inferNetworkName(node.value(QStringLiteral("network")).toString(), importedNodeUrl);
    normalizedNode.insert(QStringLiteral("network"), importedNetwork);
    normalizedNode.insert(QStringLiteral("url"),
                          isNodeUrlAccepted(importedNodeUrl)
                              ? importedNodeUrl
                              : defaultNodeUrlForNetwork(importedNetwork));
    document.insert(QStringLiteral("node"), normalizedNode);

    QJsonObject normalizedWallet = wallet;
    normalizedWallet.insert(QStringLiteral("name"), walletName);
    GrinWalletStorage::setWalletForNetwork(&document, importedNetwork, normalizedWallet);
    document.insert(QStringLiteral("wallet"), normalizedWallet);

    // -------------------------------------------------------------------------------------------------------
    // Normalizing Wallet State And Workflow Contexts
    // -------------------------------------------------------------------------------------------------------
    QJsonObject walletState = candidate.value(QStringLiteral("wallet_state")).toObject();
    QJsonObject normalizedState = document.value(QStringLiteral("wallet_state")).toObject();
    if (!walletState.isEmpty()) {
        normalizedState.insert(QStringLiteral("balances"),
                               walletState.value(QStringLiteral("balances")).isObject()
                                   ? walletState.value(QStringLiteral("balances")).toObject()
                                   : normalizedState.value(QStringLiteral("balances")).toObject());
        normalizedState.insert(QStringLiteral("scan_height"),
                               walletState.value(QStringLiteral("scan_height")).toVariant().toLongLong());
        normalizedState.insert(QStringLiteral("restore_leaf_index"),
                               qMax<qint64>(0, walletState.value(QStringLiteral("restore_leaf_index")).toVariant().toLongLong()));
        normalizedState.insert(QStringLiteral("next_child_index"),
                               qMax<qint64>(0, walletState.value(QStringLiteral("next_child_index")).toVariant().toLongLong()));
        normalizedState.insert(QStringLiteral("outputs"),
                               walletState.value(QStringLiteral("outputs")).isArray()
                                   ? walletState.value(QStringLiteral("outputs")).toArray()
                                   : QJsonArray());
        normalizedState.insert(QStringLiteral("transactions"),
                               walletState.value(QStringLiteral("transactions")).isArray()
                                   ? walletState.value(QStringLiteral("transactions")).toArray()
                                   : QJsonArray());
        if (walletState.value(QStringLiteral("transaction_rescan_backup")).isArray()) {
            normalizedState.insert(QStringLiteral("transaction_rescan_backup"),
                                   walletState.value(QStringLiteral("transaction_rescan_backup")).toArray());
        }
        if (walletState.contains(QStringLiteral("last_sync_mode"))) {
            normalizedState.insert(QStringLiteral("last_sync_mode"),
                                   walletState.value(QStringLiteral("last_sync_mode")).toString());
        }
        if (walletState.contains(QStringLiteral("last_synced_at"))) {
            normalizedState.insert(QStringLiteral("last_synced_at"),
                                   walletState.value(QStringLiteral("last_synced_at")).toString());
        }
    }
    document.insert(QStringLiteral("wallet_state"), normalizedState);
    GrinWalletStorage::setWalletStateForNetwork(&document, importedNetwork, normalizedState);

    const QJsonObject normalizedContexts =
        candidate.value(QStringLiteral("workflow_contexts")).isObject()
            ? candidate.value(QStringLiteral("workflow_contexts")).toObject()
            : QJsonObject();
    document.insert(QStringLiteral("workflow_contexts"), normalizedContexts);
    GrinWalletStorage::setWorkflowContextsForNetwork(&document, importedNetwork, normalizedContexts);

    *documentOut = document;
    return true;
}

} // namespace

/**
 * @brief Builds an empty default wallet_state object.
 * @return Default wallet_state JSON object.
 */
QJsonObject GrinWalletStorage::defaultWalletState()
{
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("awaiting_confirmation"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("awaiting_finalization"), QStringLiteral("0.000000000"));

    QJsonObject walletState;
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 0);
    walletState.insert(QStringLiteral("next_child_index"), 1);
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());
    return walletState;
}

/**
 * @brief Builds default wallet metadata for a network slot.
 * @return Default wallet metadata object.
 */
QJsonObject GrinWalletStorage::defaultWalletMetadata()
{
    return QJsonObject();
}

/**
 * @brief Builds a default storage document with per-network sections.
 * @return Default storage document object.
 */
QJsonObject GrinWalletStorage::defaultDocument()
{
    const QJsonObject walletState = defaultWalletState();
    QJsonObject root;
    root.insert(QStringLiteral("wallet"), defaultWalletMetadata());
    QJsonObject nodeConfig;
    nodeConfig.insert(QStringLiteral("network"), defaultNetworkName());
    nodeConfig.insert(QStringLiteral("url"), defaultNodeUrlForNetwork(defaultNetworkName()));
    root.insert(QStringLiteral("node"), nodeConfig);
    root.insert(QStringLiteral("wallet_state"), walletState);
    root.insert(QStringLiteral("workflow_contexts"), QJsonObject());

    QJsonObject walletStates;
    walletStates.insert(defaultNetworkName(), walletState);
    walletStates.insert(QStringLiteral("testnet"), defaultWalletState());
    root.insert(QStringLiteral("wallet_states"), walletStates);

    QJsonObject walletsByNetwork;
    walletsByNetwork.insert(defaultNetworkName(), defaultWalletMetadata());
    walletsByNetwork.insert(QStringLiteral("testnet"), defaultWalletMetadata());
    root.insert(QStringLiteral("wallets_by_network"), walletsByNetwork);

    QJsonObject workflowContextsByNetwork;
    workflowContextsByNetwork.insert(defaultNetworkName(), QJsonObject());
    workflowContextsByNetwork.insert(QStringLiteral("testnet"), QJsonObject());
    root.insert(QStringLiteral("workflow_contexts_by_network"), workflowContextsByNetwork);
    return root;
}

/**
 * @brief Retrieves wallet metadata for a specific network.
 * @param document Storage document.
 * @param networkName Network identifier.
 * @return Network-scoped wallet metadata.
 */
QJsonObject GrinWalletStorage::walletForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject walletsByNetwork = document.value(QStringLiteral("wallets_by_network")).toObject();
    return walletsByNetwork.value(networkName).toObject();
}

/**
 * @brief Retrieves wallet_state for a specific network.
 * @param document Storage document.
 * @param networkName Network identifier.
 * @return Network-scoped wallet_state object.
 */
QJsonObject GrinWalletStorage::walletStateForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject walletStates = document.value(QStringLiteral("wallet_states")).toObject();
    const QJsonObject state = walletStates.value(networkName).toObject();
    return state.isEmpty() ? defaultWalletState() : state;
}

/**
 * @brief Retrieves workflow contexts for a specific network.
 * @param document Storage document.
 * @param networkName Network identifier.
 * @return Network-scoped workflow contexts.
 */
QJsonObject GrinWalletStorage::workflowContextsForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject contextsByNetwork = document.value(QStringLiteral("workflow_contexts_by_network")).toObject();
    return contextsByNetwork.value(networkName).toObject();
}

/**
 * @brief Sets wallet metadata for a specific network.
 * @param document In/out storage document.
 * @param networkName Network identifier.
 * @param wallet Network-scoped wallet metadata.
 */
void GrinWalletStorage::setWalletForNetwork(QJsonObject *document,
                                            const QString &networkName,
                                            const QJsonObject &wallet)
{
    if (!document) {
        return;
    }

    QJsonObject walletsByNetwork = document->value(QStringLiteral("wallets_by_network")).toObject();
    walletsByNetwork.insert(networkName, wallet);
    document->insert(QStringLiteral("wallets_by_network"), walletsByNetwork);
}

/**
 * @brief Sets wallet_state for a specific network.
 * @param document In/out storage document.
 * @param networkName Network identifier.
 * @param walletState Network-scoped wallet_state.
 */
void GrinWalletStorage::setWalletStateForNetwork(QJsonObject *document,
                                                 const QString &networkName,
                                                 const QJsonObject &walletState)
{
    if (!document) {
        return;
    }

    QJsonObject walletStates = document->value(QStringLiteral("wallet_states")).toObject();
    walletStates.insert(networkName, walletState);
    document->insert(QStringLiteral("wallet_states"), walletStates);
}

/**
 * @brief Sets workflow contexts for a specific network.
 * @param document In/out storage document.
 * @param networkName Network identifier.
 * @param contexts Network-scoped workflow contexts.
 */
void GrinWalletStorage::setWorkflowContextsForNetwork(QJsonObject *document,
                                                      const QString &networkName,
                                                      const QJsonObject &contexts)
{
    if (!document) {
        return;
    }

    QJsonObject contextsByNetwork = document->value(QStringLiteral("workflow_contexts_by_network")).toObject();
    contextsByNetwork.insert(networkName, contexts);
    document->insert(QStringLiteral("workflow_contexts_by_network"), contextsByNetwork);
}

/**
 * @brief Syncs top-level active view fields from network-scoped sections.
 * @param document In/out storage document.
 * @param networkName Active network identifier.
 */
void GrinWalletStorage::syncActiveNetworkView(QJsonObject *document, const QString &networkName)
{
    if (!document) {
        return;
    }

    document->insert(QStringLiteral("wallet"), walletForNetwork(*document, networkName));
    document->insert(QStringLiteral("wallet_state"), walletStateForNetwork(*document, networkName));
    document->insert(QStringLiteral("workflow_contexts"), workflowContextsForNetwork(*document, networkName));
}

/**
 * @brief Persists top-level active view fields into network-scoped sections.
 * @param document In/out storage document.
 * @param networkName Active network identifier.
 */
void GrinWalletStorage::persistActiveNetworkView(QJsonObject *document, const QString &networkName)
{
    if (!document) {
        return;
    }

    setWalletForNetwork(document, networkName, document->value(QStringLiteral("wallet")).toObject());
    setWalletStateForNetwork(document, networkName, document->value(QStringLiteral("wallet_state")).toObject());
    setWorkflowContextsForNetwork(document,
                                  networkName,
                                  document->value(QStringLiteral("workflow_contexts")).toObject());
}

/**
 * @brief Ensures required schema sections exist and are normalized.
 * @param rawDocument Raw storage document.
 * @return Schema-complete document.
 */
QJsonObject GrinWalletStorage::ensureDocumentSchema(const QJsonObject &rawDocument)
{
    // -------------------------------------------------------------------------------------------------------
    // Ensuring Network-Scoped Document Sections
    // -------------------------------------------------------------------------------------------------------
    QJsonObject document = rawDocument.isEmpty() ? defaultDocument() : rawDocument;
    const QString networkName =
        inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());

    QJsonObject walletStates = document.value(QStringLiteral("wallet_states")).toObject();
    if (walletStates.isEmpty()) {
        walletStates.insert(networkName,
                            document.value(QStringLiteral("wallet_state")).toObject().isEmpty()
                                ? defaultWalletState()
                                : document.value(QStringLiteral("wallet_state")).toObject());
    }
    if (walletStates.value(QStringLiteral("mainnet")).toObject().isEmpty()) {
        walletStates.insert(QStringLiteral("mainnet"), defaultWalletState());
    }
    if (walletStates.value(QStringLiteral("testnet")).toObject().isEmpty()) {
        walletStates.insert(QStringLiteral("testnet"), defaultWalletState());
    }
    document.insert(QStringLiteral("wallet_states"), walletStates);

    QJsonObject walletsByNetwork = document.value(QStringLiteral("wallets_by_network")).toObject();
    if (walletsByNetwork.isEmpty()) {
        walletsByNetwork.insert(networkName, document.value(QStringLiteral("wallet")).toObject());
    }
    if (walletsByNetwork.value(QStringLiteral("mainnet")).toObject().isEmpty()

        && networkName != QStringLiteral("mainnet")) {
        walletsByNetwork.insert(QStringLiteral("mainnet"), defaultWalletMetadata());
    }
    if (walletsByNetwork.value(QStringLiteral("testnet")).toObject().isEmpty()

        && networkName != QStringLiteral("testnet")) {
        walletsByNetwork.insert(QStringLiteral("testnet"), defaultWalletMetadata());
    }
    if (!walletsByNetwork.contains(QStringLiteral("mainnet"))) {
        walletsByNetwork.insert(QStringLiteral("mainnet"), defaultWalletMetadata());
    }
    if (!walletsByNetwork.contains(QStringLiteral("testnet"))) {
        walletsByNetwork.insert(QStringLiteral("testnet"), defaultWalletMetadata());
    }
    document.insert(QStringLiteral("wallets_by_network"), walletsByNetwork);

    QJsonObject contextsByNetwork = document.value(QStringLiteral("workflow_contexts_by_network")).toObject();
    if (contextsByNetwork.isEmpty()) {
        contextsByNetwork.insert(networkName, document.value(QStringLiteral("workflow_contexts")).toObject());
    }
    if (contextsByNetwork.value(QStringLiteral("mainnet")).toObject().isEmpty()) {
        contextsByNetwork.insert(QStringLiteral("mainnet"), QJsonObject());
    }
    if (contextsByNetwork.value(QStringLiteral("testnet")).toObject().isEmpty()) {
        contextsByNetwork.insert(QStringLiteral("testnet"), QJsonObject());
    }
    document.insert(QStringLiteral("workflow_contexts_by_network"), contextsByNetwork);

    QJsonObject node = document.value(QStringLiteral("node")).toObject();

    node.insert(QStringLiteral("network"), networkName);
    if (!isNodeUrlAccepted(node.value(QStringLiteral("url")).toString())) {
        node.insert(QStringLiteral("url"), defaultNodeUrlForNetwork(networkName));
    }
    document.insert(QStringLiteral("node"), node);
    return document;
}

/**
 * @brief Normalizes schema and synchronizes active network view.
 * @param rawDocument Raw storage document.
 * @return Normalized document.
 */
QJsonObject GrinWalletStorage::normalizeDocumentSchema(const QJsonObject &rawDocument)
{
    QJsonObject document = ensureDocumentSchema(rawDocument);
    const QString networkName =
        inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    syncActiveNetworkView(&document, networkName);
    return document;
}

/**
 * @brief Extracts and normalizes backup JSON into current storage schema.
 * @param json Backup JSON bytes.
 * @param errorOut Optional parse/normalization error output.
 * @return Normalized backup document, or empty object on failure.
 */
QJsonObject GrinWalletStorage::extractImportedBackupDocument(const QByteArray &json, QString *errorOut)
{
    QJsonParseError parseError;

    const QJsonDocument parsed = QJsonDocument::fromJson(json, &parseError);
    if (!parsed.isObject()) {
        if (errorOut) {
            *errorOut = parseError.error == QJsonParseError::NoError
                ? QStringLiteral("Backup text is not a JSON object.")
                : QStringLiteral("Backup JSON could not be parsed: %1").arg(parseError.errorString());
        }
        return QJsonObject();
    }

    const QJsonObject root = parsed.object();
    QJsonObject candidate = root;
    if (root.value(QStringLiteral("backup_kind")).toString()

        == QStringLiteral("grinffindor.encrypted_wallet_backup")) {
        candidate = root.value(QStringLiteral("document")).toObject();
    }

    QJsonObject normalized;
    if (!normalizeImportedDocument(candidate, &normalized, errorOut)) {
        return QJsonObject();
    }
    return normalized;
}

/**
 * @brief Loads wallet document from persistent storage.
 * @return Loaded and normalized document.
 */
QJsonObject GrinWalletStorage::loadDocument()
{
    ensureStorageReady();
#ifdef Q_OS_WASM
    if (char *storedJson = browserLocalStorageGet(kWalletLocalStorageKey)) {
        const QByteArray localJson(storedJson);
        free(storedJson);
        const QJsonDocument localDoc = QJsonDocument::fromJson(localJson);
        if (localDoc.isObject()) {
            return normalizeDocumentSchema(localDoc.object());
        }
    }
#endif

    QFile file(storageFilePath());
    if (!file.exists()) {
        return defaultDocument();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return defaultDocument();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.isObject() ? normalizeDocumentSchema(doc.object()) : defaultDocument();
}

/**
 * @brief Saves wallet document to persistent storage.
 * @param document Storage document to persist.
 * @return True on successful write.
 */
bool GrinWalletStorage::saveDocument(const QJsonObject &document)
{
    ensureStorageReady();
    // -------------------------------------------------------------------------------------------------------
    // Normalizing And Persisting Active Network View
    // -------------------------------------------------------------------------------------------------------
    QJsonObject normalized = ensureDocumentSchema(document);
    const QString networkName =
        inferNetworkName(normalized.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         normalized.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    persistActiveNetworkView(&normalized, networkName);
    const QByteArray json = QJsonDocument(normalized).toJson(QJsonDocument::Indented);
#ifdef Q_OS_WASM
    browserLocalStorageSet(kWalletLocalStorageKey, json.constData());
#endif

    QSaveFile file(storageFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(json);
    if (!file.commit()) {
        return false;
    }
    flushStorage();
    return true;
}

/**
 * @brief Builds controller-facing state summary from storage document.
 * @param document Storage document.
 * @return Loaded controller state snapshot.
 */
GrinWalletStorage::LoadedState GrinWalletStorage::loadState(const QJsonObject &document)
{
    LoadedState state;
    const QString activeNetwork =
        inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    const QJsonObject wallet = walletForNetwork(document, activeNetwork);
    const QJsonObject node = document.value(QStringLiteral("node")).toObject();
    const QString storedNodeUrl = node.value(QStringLiteral("url")).toString();

    state.walletExists = !wallet.isEmpty();
    state.walletName = wallet.value(QStringLiteral("name")).toString();
    state.seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    state.selectedNetwork = activeNetwork;
    state.nodeUrl = isNodeUrlAccepted(storedNodeUrl)
        ? storedNodeUrl
        : defaultNodeUrlForNetwork(state.selectedNetwork);
    state.autoLockOnDeactivate = document
        .value(QLatin1String(kAppSettingsKey))
        .toObject()
        .value(QLatin1String(kAutoLockOnDeactivateKey))
        .toBool(false);

    return state;
}

/**
 * @brief Recomputes balances and summary fields from stored outputs.
 * @param document Storage document copy.
 * @param chainHeight Current chain height.
 * @return Refreshed state with optional updated document.
 */
GrinWalletStorage::RefreshedState GrinWalletStorage::refreshState(QJsonObject document, qulonglong chainHeight)
{
    RefreshedState state;
    state.document = document;

    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    const QJsonObject storedBalances = walletState.value(QStringLiteral("balances")).toObject();
    state.scanHeight = static_cast<qulonglong>(walletState.value(QStringLiteral("scan_height")).toInt());

    const qulonglong effectiveHeight = chainHeight > 0 ? chainHeight : state.scanHeight;
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    const QJsonObject recalculatedBalances = WalletScanner::balancesFromOutputs(outputs, effectiveHeight);

    const QJsonObject balances = recalculatedBalances.isEmpty() ? storedBalances : recalculatedBalances;

    if (balances != storedBalances) {
        walletState.insert(QStringLiteral("balances"), balances);
        state.document.insert(QStringLiteral("wallet_state"), walletState);
        state.balancesChanged = true;
    }

    state.totalBalance = amountStringFromJson(balances, QStringLiteral("total"));
    state.spendableBalance = amountStringFromJson(balances, QStringLiteral("spendable"));
    state.lockedBalance = amountStringFromJson(balances, QStringLiteral("locked"));
    state.immatureBalance = amountStringFromJson(balances, QStringLiteral("immature"));
    state.awaitingConfirmationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_confirmation"));
    state.awaitingFinalizationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_finalization"));

    return state;
}

/**
 * @brief Updates transaction confirmation counts and confirmation status.
 * @param document In/out storage document.
 * @param chainHeight Current chain height.
 * @return True when transaction entries were changed.
 */
bool GrinWalletStorage::refreshTransactionConfirmations(QJsonObject *document, qulonglong chainHeight)
{
    if (!document) {
        return false;
    }

    QJsonObject walletState = document->value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool changed = false;

    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
        const QString status = entry.value(QStringLiteral("status")).toString();

        int confirmations = 0;
        if (confirmedHeight > 0 && chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<int>(chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
        }

        if (entry.value(QStringLiteral("confirmations")).toInt() != confirmations) {
            entry.insert(QStringLiteral("confirmations"), confirmations);
            changed = true;
        }

        if (confirmedHeight > 0 && confirmations > 0 && status != QStringLiteral("confirmed")) {
            entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
            changed = true;
        }

        transactions.replace(i, entry);
    }

    if (changed) {
        walletState.insert(QStringLiteral("transactions"), transactions);
        document->insert(QStringLiteral("wallet_state"), walletState);
    }

    return changed;
}

/**
 * @brief Stores workflow context object by workflow id.
 * @param document In/out storage document.
 * @param workflowId Workflow identifier.
 * @param context Workflow context object.
 * @return True when context was stored.
 */
bool GrinWalletStorage::storeWorkflowContext(QJsonObject *document,
                                             const QString &workflowId,
                                             const QJsonObject &context)
{
    if (!document || workflowId.isEmpty()) {
        return false;
    }

    QJsonObject contexts = document->value(QStringLiteral("workflow_contexts")).toObject();
    contexts.insert(workflowId, context);
    document->insert(QStringLiteral("workflow_contexts"), contexts);
    return true;
}

/**
 * @brief Retrieves workflow context for a workflow id.
 * @param document Storage document.
 * @param workflowId Workflow identifier.
 * @return Stored workflow context object.
 */
QJsonObject GrinWalletStorage::workflowContext(const QJsonObject &document, const QString &workflowId)
{
    if (workflowId.isEmpty()) {
        return QJsonObject();
    }

    return document.value(QStringLiteral("workflow_contexts")).toObject().value(workflowId).toObject();
}
