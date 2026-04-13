#include "grinwalletstorage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSet>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include "../3rdparty/monocypher/monocypher.h"
#include "walletsecurerandom.h"

#ifdef Q_OS_WASM
#include <emscripten.h>

EM_JS(int, wasmEnsurePersistentStorageReady, (), {
    try {
        if (typeof FS === "undefined" || typeof IDBFS === "undefined") {
            try {
                console.error("wallet storage init failed", {
                    hasFS: typeof FS !== "undefined",
                    hasIDBFS: typeof IDBFS !== "undefined"
                });
            } catch (ignored) {}
            return 0;
        }

        if (!Module.grinWalletStorageInitPromise) {
            Module.grinWalletStorageInitPromise = (async () => {
                try {
                    try {
                        FS.mkdir('/persistent');
                    } catch (e) {}

                    if (!Module.grinWalletIdbMounted) {
                        FS.mount(IDBFS, {}, '/persistent');
                        Module.grinWalletIdbMounted = true;
                    }

                    await new Promise((resolve, reject) => {
                        FS.syncfs(true, function(err) {
                            if (err) {
                                reject(err);
                                return;
                            }
                            Module.grinWalletIdbReady = true;
                            resolve();
                        });
                    });
                    return 1;
                } catch (e) {
                    try {
                        console.error("wallet storage init failed", e);
                    } catch (ignored) {}
                    Module.grinWalletIdbReady = false;
                    throw e;
                }
            })();
        }

        return 1;
    } catch (e) {
        try {
            console.error("wallet storage init bridge failed", e);
        } catch (ignored) {}
    }
    return 0;
});

EM_JS(int, wasmFlushPersistentStorage, (), {
    try {
        if (typeof FS === "undefined" || !Module.grinWalletIdbMounted) {
            return 1;
        }

        if (!Module.grinWalletStorageFlushPromise) {
            Module.grinWalletStorageFlushPromise = Promise.resolve();
        }

        Module.grinWalletStorageFlushPromise = Module.grinWalletStorageFlushPromise.then(async () => {
            if (Module.grinWalletStorageInitPromise) {
                await Module.grinWalletStorageInitPromise;
            }
            await new Promise((resolve, reject) => {
                FS.syncfs(false, function(err) {
                    if (err) {
                        reject(err);
                        return;
                    }
                    resolve();
                });
            });
        }).catch((e) => {
            try {
                console.error("wallet storage flush failed", e);
            } catch (ignored) {}
        });
        return 1;
    } catch (e) {
        try {
            console.error("wallet storage flush bridge failed", e);
        } catch (ignored) {}
    }
    return 0;
});
#endif

#include "walletscanner.h"

namespace {

const char *kAppSettingsKey = "app_settings";
const char *kAutoLockOnDeactivateKey = "auto_lock_on_app_deactivate";
const char *kMinimumConfirmationsKey = "minimum_confirmations";
const char *kWalletStorePath = "/grin-wallet/browser-wallet.json";
const char *kMainnetNodeUrl = "https://mainnet.grinffindor.org/v2/foreign";
const char *kTestnetNodeUrl = "https://testnet.grinffindor.org/v2/foreign";
const int kSensitiveEnvelopeVersion = 1;
const int kSensitiveMacBytes = 16;
const char *kProtectedWalletStatesKey = "wallet_states_protected_v2";
QByteArray g_sensitiveDataKey;
QJsonObject g_cachedProtectedEnvelope;

Q_LOGGING_CATEGORY(walletStorageLog, "grinffindor.wallet.storage")

QString storageRootPath();
QString storageFilePath();

void wipeByteArray(QByteArray *value)
{
    if (!value) {
        return;
    }
    if (!value->isEmpty()) {
        crypto_wipe(value->data(), static_cast<size_t>(value->size()));
    }
    value->clear();
    value->squeeze();
}

QByteArray sensitiveEncryptionKey()
{
    if (g_sensitiveDataKey.isEmpty()) {
        return QByteArray();
    }
    return QCryptographicHash::hash(QByteArrayLiteral("grinffindor.wallet-sensitive.v1:") + g_sensitiveDataKey,
                                    QCryptographicHash::Blake2b_256);
}

QJsonObject encryptSensitiveObject(const QJsonObject &sensitive)
{
    if (g_sensitiveDataKey.isEmpty()) {
        return QJsonObject();
    }

    const QByteArray key = sensitiveEncryptionKey();
    const QByteArray plain = QJsonDocument(sensitive).toJson(QJsonDocument::Compact);
    const QByteArray nonce = WalletSecureRandom::bytes(24);
    if (key.size() != 32 || nonce.size() != 24 || plain.isEmpty()) {
        return QJsonObject();
    }

    QByteArray cipher(plain.size(), Qt::Uninitialized);
    QByteArray mac(kSensitiveMacBytes, Qt::Uninitialized);
    static const QByteArray associatedData("grinffindor.wallet-sensitive-envelope", 37);
    crypto_aead_lock(reinterpret_cast<uint8_t *>(cipher.data()),
                     reinterpret_cast<uint8_t *>(mac.data()),
                     reinterpret_cast<const uint8_t *>(key.constData()),
                     reinterpret_cast<const uint8_t *>(nonce.constData()),
                     reinterpret_cast<const uint8_t *>(associatedData.constData()),
                     static_cast<size_t>(associatedData.size()),
                     reinterpret_cast<const uint8_t *>(plain.constData()),
                     static_cast<size_t>(plain.size()));

    QJsonObject envelope;
    envelope.insert(QStringLiteral("version"), kSensitiveEnvelopeVersion);
    envelope.insert(QStringLiteral("algorithm"), QStringLiteral("xchacha20-poly1305"));
    envelope.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
    envelope.insert(QStringLiteral("cipher"), QString::fromUtf8(cipher.toBase64()));
    envelope.insert(QStringLiteral("mac"), QString::fromUtf8(mac.toBase64()));
    return envelope;
}

QJsonObject redactedWalletState()
{
    return GrinWalletStorage::defaultWalletState();
}

QJsonObject redactedWalletStatesObject(const QJsonObject &walletStates)
{
    QJsonObject redacted;
    const QStringList keys = walletStates.keys();
    for (int i = 0; i < keys.size(); ++i) {
        redacted.insert(keys.at(i), redactedWalletState());
    }
    if (!redacted.contains(QStringLiteral("mainnet"))) {
        redacted.insert(QStringLiteral("mainnet"), redactedWalletState());
    }
    if (!redacted.contains(QStringLiteral("testnet"))) {
        redacted.insert(QStringLiteral("testnet"), redactedWalletState());
    }
    return redacted;
}

bool decryptSensitiveObject(const QJsonObject &envelope, QJsonObject *sensitiveOut)
{
    if (!sensitiveOut || g_sensitiveDataKey.isEmpty()) {
        return false;
    }
    if (envelope.value(QStringLiteral("version")).toInt() != kSensitiveEnvelopeVersion) {
        return false;
    }

    const QByteArray key = sensitiveEncryptionKey();
    QByteArray nonce = QByteArray::fromBase64(envelope.value(QStringLiteral("nonce")).toString().toUtf8());
    QByteArray cipher = QByteArray::fromBase64(envelope.value(QStringLiteral("cipher")).toString().toUtf8());
    QByteArray mac = QByteArray::fromBase64(envelope.value(QStringLiteral("mac")).toString().toUtf8());
    if (key.size() != 32 || nonce.size() != 24 || mac.size() != kSensitiveMacBytes) {
        return false;
    }

    QByteArray plain(cipher.size(), Qt::Uninitialized);
    static const QByteArray associatedData("grinffindor.wallet-sensitive-envelope", 37);
    const int ok = crypto_aead_unlock(reinterpret_cast<uint8_t *>(plain.data()),
                                      reinterpret_cast<const uint8_t *>(mac.constData()),
                                      reinterpret_cast<const uint8_t *>(key.constData()),
                                      reinterpret_cast<const uint8_t *>(nonce.constData()),
                                      reinterpret_cast<const uint8_t *>(associatedData.constData()),
                                      static_cast<size_t>(associatedData.size()),
                                      reinterpret_cast<const uint8_t *>(cipher.constData()),
                                      static_cast<size_t>(cipher.size()));
    if (ok != 0) {
        wipeByteArray(&plain);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(plain);
    wipeByteArray(&plain);
    if (!document.isObject()) {
        return false;
    }
    *sensitiveOut = document.object();
    return true;
}

void restoreProtectedWalletStates(QJsonObject *document)
{
    if (!document) {
        return;
    }

    const QJsonObject envelope = document->value(QString::fromUtf8(kProtectedWalletStatesKey)).toObject();
    if (envelope.isEmpty()) {
        return;
    }

    // Cache the encrypted envelope so saveDocument() can re-persist it
    // without a second disk read when the sensitive key is unavailable.
    g_cachedProtectedEnvelope = envelope;

    if (!GrinWalletStorage::hasSensitiveDataKey()) {
        return;
    }

    QJsonObject decrypted;
    if (!decryptSensitiveObject(envelope, &decrypted)) {
        qWarning(walletStorageLog) << "restoreProtectedWalletStates: decrypt failed for protected wallet state envelope";
        return;
    }

    const QJsonObject walletStates = decrypted.value(QStringLiteral("wallet_states")).toObject();
    if (walletStates.isEmpty()) {
        qWarning(walletStorageLog) << "restoreProtectedWalletStates: decrypted payload missing wallet_states object";
        return;
    }

    document->insert(QStringLiteral("wallet_states"), walletStates);
}

void preserveProtectedWalletStatesEnvelope(QJsonObject *document)
{
    if (!document) {
        return;
    }

    if (document->contains(QString::fromUtf8(kProtectedWalletStatesKey))) {
        return;
    }

    // Use the in-memory cached envelope instead of re-reading from disk.
    // The cache is populated during loadDocument() → restoreProtectedWalletStates().
    if (!g_cachedProtectedEnvelope.isEmpty()) {
        document->insert(QString::fromUtf8(kProtectedWalletStatesKey), g_cachedProtectedEnvelope);
    }
}

bool protectWalletStatesForPersistence(QJsonObject *document, const QString &networkName)
{
    if (!document) {
        return false;
    }

    if (!GrinWalletStorage::hasSensitiveDataKey()) {
        preserveProtectedWalletStatesEnvelope(document);
        return true;
    }

    const QJsonObject walletStates = document->value(QStringLiteral("wallet_states")).toObject();
    if (walletStates.isEmpty()) {
        qWarning(walletStorageLog) << "protectWalletStatesForPersistence: wallet_states missing before encryption";
        return false;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("wallet_states"), walletStates);
    const QJsonObject envelope = encryptSensitiveObject(payload);
    if (envelope.isEmpty()) {
        qWarning(walletStorageLog) << "protectWalletStatesForPersistence: failed to encrypt wallet_states";
        return false;
    }

    document->insert(QString::fromUtf8(kProtectedWalletStatesKey), envelope);
    document->insert(QStringLiteral("wallet_states"), redactedWalletStatesObject(walletStates));
    // Keep the in-memory cache in sync with the freshly encrypted envelope.
    g_cachedProtectedEnvelope = envelope;
    GrinWalletStorage::syncActiveNetworkView(document, networkName);
    return true;
}

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
bool ensureStorageReady()
{
    const QFileInfo info(storageFilePath());
#ifdef Q_OS_WASM
    if (wasmEnsurePersistentStorageReady() != 1) {
        qWarning(walletStorageLog) << "ensureStorageReady: wasmEnsurePersistentStorageReady failed for"
                                   << info.absoluteFilePath();
        return false;
    }
    const bool mkpathOk = QDir().mkpath(info.absolutePath());
    if (!mkpathOk) {
        qWarning(walletStorageLog) << "ensureStorageReady: mkpath failed for"
                                   << info.absolutePath()
                                   << "file"
                                   << info.absoluteFilePath();
    }
    return mkpathOk;
#else
    QDir().mkpath(info.absolutePath());
    return true;
#endif
}

/**
 * @brief Flushes pending WASM filesystem changes to persistent backing store.
 */
bool flushStorage()
{
#ifdef Q_OS_WASM
    const bool ok = wasmFlushPersistentStorage() == 1;
    if (!ok) {
        qWarning(walletStorageLog) << "flushStorage: wasmFlushPersistentStorage failed for"
                                   << storageFilePath();
    }
    return ok;
#else
    return true;
#endif
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
    restoreProtectedWalletStates(&document);
    const QString networkName =
        inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    syncActiveNetworkView(&document, networkName);
    return document;
}

/**
 * @brief Loads wallet document from persistent storage.
 * @return Loaded and normalized document.
 */
QJsonObject GrinWalletStorage::loadDocument()
{
    if (!ensureStorageReady()) {
        qWarning(walletStorageLog) << "loadDocument: storage not ready, using default document";
        return defaultDocument();
    }

    QFile file(storageFilePath());
    if (!file.exists()) {
        qWarning(walletStorageLog) << "loadDocument: file does not exist yet at" << file.fileName();
        return defaultDocument();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning(walletStorageLog) << "loadDocument: open failed for"
                                   << file.fileName()
                                   << file.errorString();
        return defaultDocument();
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject()) {
        qWarning(walletStorageLog) << "loadDocument: invalid json at" << file.fileName();
    }
    return doc.isObject() ? normalizeDocumentSchema(doc.object()) : defaultDocument();
}

/**
 * @brief Saves wallet document to persistent storage.
 * @param document Storage document to persist.
 * @return True on successful write.
 */
bool GrinWalletStorage::saveDocument(const QJsonObject &document)
{
    if (!ensureStorageReady()) {
        qWarning(walletStorageLog) << "saveDocument: storage not ready";
        return false;
    }

    // Normalize document and persist the active network-specific top-level view.
    QJsonObject normalized = ensureDocumentSchema(document);
    const QString networkName =
        inferNetworkName(normalized.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         normalized.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());

    persistActiveNetworkView(&normalized, networkName);

    if (!protectWalletStatesForPersistence(&normalized, networkName)) {
        qWarning(walletStorageLog) << "saveDocument: failed to protect wallet_states before write";
        return false;
    }

    const QByteArray json = QJsonDocument(normalized).toJson(QJsonDocument::Indented);
    const QString path = storageFilePath();

#ifdef Q_OS_WASM
    QFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning(walletStorageLog) << "saveDocument: open failed for"
                                   << file.fileName()
                                   << file.errorString();
        return false;
    }

    const qint64 written = file.write(json);
    if (written != json.size()) {
        qWarning(walletStorageLog) << "saveDocument: partial write for"
                                   << file.fileName()
                                   << "written"
                                   << written
                                   << "expected"
                                   << json.size()
                                   << file.errorString();
        file.close();
        return false;
    }

    if (!file.flush()) {
        qWarning(walletStorageLog) << "saveDocument: flush failed for"
                                   << file.fileName()
                                   << file.errorString();
        file.close();
        return false;
    }

    file.close();

#else
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning(walletStorageLog) << "saveDocument: open failed for"
                                   << file.fileName()
                                   << file.errorString();
        return false;
    }

    const qint64 written = file.write(json);
    if (written != json.size()) {
        qWarning(walletStorageLog) << "saveDocument: partial write for"
                                   << file.fileName()
                                   << "written"
                                   << written
                                   << "expected"
                                   << json.size()
                                   << file.errorString();
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) {
        qWarning(walletStorageLog) << "saveDocument: commit failed for"
                                   << file.fileName()
                                   << file.errorString();
        return false;
    }
#endif

    const bool flushOk = flushStorage();
    if (!flushOk) {
        qWarning(walletStorageLog) << "saveDocument: persistent flush failed for" << path;
    }

    return flushOk;
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
    const QJsonObject appSettings = document.value(QLatin1String(kAppSettingsKey)).toObject();
    state.autoLockOnDeactivate = appSettings.value(QLatin1String(kAutoLockOnDeactivateKey)).toBool(false);
    const int storedMinConf = appSettings.value(QLatin1String(kMinimumConfirmationsKey)).toInt(10);
    state.minimumConfirmations = storedMinConf > 0 ? static_cast<qulonglong>(storedMinConf) : 10;

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
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    QJsonObject workflowContexts = document.value(QStringLiteral("workflow_contexts")).toObject();

    QSet<QString> workflowIdsWithOutputs;
    for (int i = 0; i < outputs.size(); ++i) {
        const QString workflowId = outputs.at(i).workflowId.trimmed();
        if (!workflowId.isEmpty()) {
            workflowIdsWithOutputs.insert(workflowId);
        }
    }

    bool transactionsChanged = false;
    for (int i = transactions.size() - 1; i >= 0; --i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString().trimmed();
        const QString status = entry.value(QStringLiteral("status")).toString();

        if (workflowId.isEmpty()
            || status == QStringLiteral("confirmed")
            || status == QStringLiteral("cancelled")
            || status == QStringLiteral("spent")) {
            continue;
        }

        if (workflowIdsWithOutputs.contains(workflowId)) {
            continue;
        }

        const bool broadcasted = entry.value(QStringLiteral("broadcasted")).toBool();
        const bool txReady = entry.value(QStringLiteral("tx_ready")).toBool();
        const bool hasTxSkeleton = !entry.value(QStringLiteral("tx_skeleton")).toObject().isEmpty();
        const bool hasAmount = !entry.value(QStringLiteral("amount")).toString().trimmed().isEmpty();
        const bool hasFee = !entry.value(QStringLiteral("fee")).toString().trimmed().isEmpty();
        const QJsonObject context = workflowContexts.value(workflowId).toObject();
        const bool hasSelectedInputs = !context.value(QStringLiteral("selected_input_commits")).toArray().isEmpty();

        if (broadcasted || txReady || hasTxSkeleton || hasAmount || hasFee || hasSelectedInputs) {
            continue;
        }

        transactions.removeAt(i);
        workflowContexts.remove(workflowId);
        transactionsChanged = true;
    }

    if (transactionsChanged) {
        walletState.insert(QStringLiteral("transactions"), transactions);
        state.document.insert(QStringLiteral("wallet_state"), walletState);
        state.document.insert(QStringLiteral("workflow_contexts"), workflowContexts);
        state.documentChanged = true;
    }

    QSet<QString> activeWorkflowIds;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty()
            && status != QStringLiteral("confirmed")
            && status != QStringLiteral("cancelled")
            && status != QStringLiteral("spent")) {
            activeWorkflowIds.insert(workflowId);
        }
    }

    QSet<QString> activeSelectedInputCommits;
    const QStringList contextKeys = workflowContexts.keys();
    for (int i = 0; i < contextKeys.size(); ++i) {
        const QString workflowId = contextKeys.at(i);
        if (!activeWorkflowIds.contains(workflowId)) {
            continue;
        }
        const QJsonArray commits = workflowContexts.value(workflowId).toObject().value(QStringLiteral("selected_input_commits")).toArray();
        for (int j = 0; j < commits.size(); ++j) {
            const QString commitment = commits.at(j).toString();
            if (!commitment.isEmpty()) {
                activeSelectedInputCommits.insert(commitment);
            }
        }
    }

    bool outputsChanged = false;
    for (int i = 0; i < outputs.size(); ++i) {
        WalletOutput &output = outputs[i];

        if (output.spent) {
            continue;
        }

        if (activeSelectedInputCommits.contains(output.commitment) && !output.locked) {
            output.locked = true;
            outputsChanged = true;
        }

        if (output.onChain && output.height > 0 && output.pending) {
            output.pending = false;
            outputsChanged = true;
        }

        if (output.locked
            && output.onChain
            && !activeSelectedInputCommits.contains(output.commitment)) {
            output.locked = false;
            outputsChanged = true;
        }
    }

    if (outputsChanged) {
        walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
        state.document.insert(QStringLiteral("wallet_state"), walletState);
        state.documentChanged = true;
    }

    const QJsonObject settingsForBalance = document.value(QLatin1String(kAppSettingsKey)).toObject();
    const int minConfSetting = settingsForBalance.value(QLatin1String(kMinimumConfirmationsKey)).toInt(10);
    const qulonglong minConf = minConfSetting > 0 ? static_cast<qulonglong>(minConfSetting) : 10;
    const QJsonObject recalculatedBalances = WalletScanner::balancesFromOutputs(outputs, effectiveHeight, minConf);

    const QJsonObject balances = recalculatedBalances.isEmpty() ? storedBalances : recalculatedBalances;

    if (balances != storedBalances) {
        walletState.insert(QStringLiteral("balances"), balances);
        state.document.insert(QStringLiteral("wallet_state"), walletState);
        state.documentChanged = true;
        state.balancesChanged = true;
    }

    state.totalBalance = amountStringFromJson(balances, QStringLiteral("total"));
    state.spendableBalance = amountStringFromJson(balances, QStringLiteral("spendable"));
    state.lockedBalance = amountStringFromJson(balances, QStringLiteral("locked"));
    state.immatureBalance = amountStringFromJson(balances, QStringLiteral("immature"));
    state.awaitingConfirmationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_confirmation"));
    state.awaitingFinalizationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_finalization"));
    state.revertedBalance = amountStringFromJson(balances, QStringLiteral("reverted"));

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

void GrinWalletStorage::setSensitiveDataKey(const QByteArray &key)
{
    wipeByteArray(&g_sensitiveDataKey);
    g_sensitiveDataKey = key;
}

void GrinWalletStorage::clearSensitiveDataKey()
{
    wipeByteArray(&g_sensitiveDataKey);
    g_cachedProtectedEnvelope = QJsonObject();
}

bool GrinWalletStorage::hasSensitiveDataKey()
{
    return !g_sensitiveDataKey.isEmpty();
}

QJsonObject GrinWalletStorage::protectSensitiveObject(const QJsonObject &sensitive)
{
    return encryptSensitiveObject(sensitive);
}

bool GrinWalletStorage::unprotectSensitiveObject(const QJsonObject &envelope, QJsonObject *sensitiveOut)
{
    return decryptSensitiveObject(envelope, sensitiveOut);
}
