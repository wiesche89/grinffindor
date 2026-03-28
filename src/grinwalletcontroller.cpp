#include "grinwalletcontroller.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QClipboard>
#include <QUrl>
#include <QUuid>
#include <QVector>
#include <cstdlib>
#include <algorithm>

#ifdef Q_OS_WASM
#include <emscripten.h>

EM_JS(void, browserLocalStorageSet, (const char *key, const char *value), {
    try {
        if (typeof localStorage !== "undefined") {
            localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        }
    } catch (e) {}
});

EM_JS(char *, browserLocalStorageGet, (const char *key), {
    try {
        if (typeof localStorage !== "undefined") {
            const value = localStorage.getItem(UTF8ToString(key));
            if (value !== null && value !== undefined) {
                const length = lengthBytesUTF8(value) + 1;
                const buffer = _malloc(length);
                stringToUTF8(value, buffer, length);
                return buffer;
            }
        }
    } catch (e) {}
    return 0;
});

EM_JS(int, browserCopyToClipboard, (const char *value), {
    try {
        const text = UTF8ToString(value);
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text);
            return 1;
        }
        if (typeof document !== "undefined") {
            const area = document.createElement("textarea");
            area.value = text;
            area.style.position = "fixed";
            area.style.left = "-1000px";
            document.body.appendChild(area);
            area.focus();
            area.select();
            const ok = document.execCommand("copy");
            document.body.removeChild(area);
            return ok ? 1 : 0;
        }
    } catch (e) {}
    return 0;
});

EM_JS(int, browserDownloadTextFile, (const char *suggestedName, const char *value), {
    try {
        if (typeof document === "undefined" || typeof Blob === "undefined" || typeof URL === "undefined") {
            return 0;
        }
        const filename = UTF8ToString(suggestedName) || "grinffindor-wallet-backup.json";
        const text = UTF8ToString(value);
        const blob = new Blob([text], { type: "application/json;charset=utf-8" });
        const url = URL.createObjectURL(blob);
        const link = document.createElement("a");
        link.href = url;
        link.download = filename;
        document.body.appendChild(link);
        link.click();
        document.body.removeChild(link);
        setTimeout(function() { URL.revokeObjectURL(url); }, 0);
        return 1;
    } catch (e) {}
    return 0;
});

EM_JS(int, browserRequestPersistentStorage, (), {
    try {
        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist();
            return 1;
        }
    } catch (e) {}
    return 0;
});

EM_JS(char *, browserStoragePersistenceState, (), {
    try {
        if (navigator.storage && navigator.storage.persisted) {
            return Asyncify.handleAsync(async () => {
                try {
                    const persisted = await navigator.storage.persisted();
                    const value = persisted ? "persistent" : "best-effort";
                    const length = lengthBytesUTF8(value) + 1;
                    const buffer = _malloc(length);
                    stringToUTF8(value, buffer, length);
                    return buffer;
                } catch (e) {
                    const value = "best-effort";
                    const length = lengthBytesUTF8(value) + 1;
                    const buffer = _malloc(length);
                    stringToUTF8(value, buffer, length);
                    return buffer;
                }
            });
        }
    } catch (e) {}
    const fallback = "best-effort";
    const length = lengthBytesUTF8(fallback) + 1;
    const buffer = _malloc(length);
    stringToUTF8(fallback, buffer, length);
    return buffer;
});

EM_JS(void, browserInstallWalletShortcutBridge, (), {
    try {
        if (typeof window === "undefined" || window.__grinffindorWalletShortcutBridgeInstalled) {
            return;
        }

        const qtCanvas = function() {
            if (typeof document === "undefined") {
                return null;
            }
            return document.querySelector("canvas");
        };

        const isQtCanvasFocused = function() {
            if (typeof document === "undefined") {
                return false;
            }
            const active = document.activeElement;
            if (!active) {
                return false;
            }
            const tag = (active.tagName || "").toUpperCase();
            return tag === "CANVAS" || active === document.body;
        };

        const shouldIntercept = function(event) {
            if (!event) {
                return false;
            }
            const ctrlOrMeta = !!event.ctrlKey || !!event.metaKey;
            if (!ctrlOrMeta || !!event.altKey) {
                return false;
            }
            const key = (event.key || "").toLowerCase();
            if (key !== "a" && key !== "c") {
                return false;
            }
            return isQtCanvasFocused();
        };

        const redispatchToQtCanvas = function(event, type) {
            const canvas = qtCanvas();
            if (!canvas || event.__grinffindorRedispatched) {
                return;
            }

            const cloned = new KeyboardEvent(type, {
                key: event.key,
                code: event.code,
                ctrlKey: !!event.ctrlKey,
                metaKey: !!event.metaKey,
                shiftKey: !!event.shiftKey,
                altKey: !!event.altKey,
                repeat: !!event.repeat,
                bubbles: true,
                cancelable: true,
                composed: true
            });
            Object.defineProperty(cloned, "__grinffindorRedispatched", {
                value: true,
                enumerable: false
            });
            canvas.dispatchEvent(cloned);
        };

        const ensureHiddenTextarea = function() {
            if (typeof document === "undefined") {
                return null;
            }
            let area = document.getElementById("__grinffindor_shortcut_bridge");
            if (area) {
                return area;
            }
            area = document.createElement("textarea");
            area.id = "__grinffindor_shortcut_bridge";
            area.setAttribute("readonly", "readonly");
            area.style.position = "fixed";
            area.style.left = "-10000px";
            area.style.top = "0";
            area.style.opacity = "0";
            area.style.pointerEvents = "none";
            document.body.appendChild(area);
            return area;
        };

        const bridgeCopyText = function(text) {
            try {
                if (navigator.clipboard && navigator.clipboard.writeText) {
                    navigator.clipboard.writeText(text);
                    return true;
                }
            } catch (e) {}

            try {
                const area = ensureHiddenTextarea();
                if (!area) {
                    return false;
                }
                area.value = text;
                area.focus();
                area.select();
                area.setSelectionRange(0, area.value.length);
                const ok = document.execCommand("copy");
                const canvas = qtCanvas();
                if (canvas) {
                    canvas.focus();
                }
                return !!ok;
            } catch (e) {}
            return false;
        };

        const browserFallbackShortcut = function(event) {
            const ctx = window.__grinffindorShortcutContext || null;
            if (!ctx || !ctx.focused) {
                return false;
            }
            const key = (event.key || "").toLowerCase();
            if (key === "a") {
                ctx.selectedAll = true;
                return true;
            }
            if (key === "c") {
                const text = (ctx.selectedText && ctx.selectedText.length > 0)
                    ? ctx.selectedText
                    : (ctx.selectedAll ? (ctx.text || "") : (ctx.text || ""));
                return bridgeCopyText(text);
            }
            return false;
        };

        window.addEventListener("keydown", function(event) {
            if (shouldIntercept(event)) {
                event.preventDefault();
                browserFallbackShortcut(event);
                redispatchToQtCanvas(event, "keydown");
            }
        }, true);

        window.addEventListener("keyup", function(event) {
            if (shouldIntercept(event)) {
                event.preventDefault();
                redispatchToQtCanvas(event, "keyup");
            }
        }, true);

        window.__grinffindorWalletShortcutBridgeInstalled = true;
    } catch (e) {}
});

EM_JS(void, browserUpdateShortcutContext, (const char *text, const char *selectedText, int focused), {
    try {
        if (typeof window === "undefined") {
            return;
        }
        window.__grinffindorShortcutContext = {
            text: UTF8ToString(text),
            selectedText: UTF8ToString(selectedText),
            focused: !!focused,
            selectedAll: false
        };
    } catch (e) {}
});
#endif

#include "../3rdparty/monocypher/monocypher.h"

#include "wallet/slatev4.h"
#include "wallet/walletoutput.h"
#include "wallet/walletscanner.h"
#include "wallet/walletselection.h"
#include "wallet/walletkeychain.h"
#include "wallet/wallettxbuilder.h"
#include "wallet/binaryslatev4reader.h"
#include "wallet/binaryslatev4writer.h"
#include "wallet/walletcryptobackend.h"
#include "nodeforeignapi.h"
#include "result.h"
#include "tip.h"
#include "outputlisting.h"
#include "outputprintable.h"
#include "transaction.h"

namespace {

const char *kWalletStorePath = "/grin-wallet/browser-wallet.json";
const char *kWalletLocalStorageKey = "grinffindor.browserWallet";
const int kMnemonicEntropyBytes = 32;
const char *kBase58Alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const char *kMainnetNodeUrl = "https://mainnet.grinffindor.org/v2/foreign";
const char *kTestnetNodeUrl = "https://testnet.grinffindor.org/v2/foreign";
const int kSessionAutoLockIntervalMs = 15 * 60 * 1000;
const int kSeedCipherVersion = 3;
const int kSeedCipherArgon2Blocks = 256;
const int kSeedCipherArgon2Passes = 3;
const int kSeedCipherArgon2Lanes = 1;
const int kSeedCipherKeyBytes = 32;
const int kSeedCipherMacBytes = 16;

QString defaultNetworkName()
{
    return QStringLiteral("mainnet");
}

bool isAcceptedNetworkName(const QString &networkName)
{
    const QString normalized = networkName.trimmed().toLower();
    return normalized == QStringLiteral("mainnet") || normalized == QStringLiteral("testnet");
}

QString defaultNodeUrlForNetwork(const QString &networkName)
{
    return networkName.trimmed().toLower() == QStringLiteral("testnet")
        ? QString::fromUtf8(kTestnetNodeUrl)
        : QString::fromUtf8(kMainnetNodeUrl);
}

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

bool isNodeUrlAccepted(const QString &nodeUrl)
{
    const QUrl parsed = QUrl::fromUserInput(nodeUrl.trimmed());
    return parsed.isValid()
        && !parsed.scheme().trimmed().isEmpty()
        && !parsed.host().trimmed().isEmpty()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"));
}

QString storageRootPath()
{
#ifdef Q_OS_WASM
    return QStringLiteral("/persistent");
#else
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData.isEmpty() ? QStringLiteral(".wallet-data") : appData;
#endif
}

QString storageFilePath()
{
    return storageRootPath() + QString::fromUtf8(kWalletStorePath);
}

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

QJsonObject defaultWalletState()
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
    return walletState;
}

QJsonObject defaultWalletMetadata()
{
    return QJsonObject();
}

QJsonObject defaultDocument()
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

QJsonObject walletForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject walletsByNetwork = document.value(QStringLiteral("wallets_by_network")).toObject();
    return walletsByNetwork.value(networkName).toObject();
}

QJsonObject walletStateForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject walletStates = document.value(QStringLiteral("wallet_states")).toObject();
    const QJsonObject state = walletStates.value(networkName).toObject();
    return state.isEmpty() ? defaultWalletState() : state;
}

QJsonObject workflowContextsForNetwork(const QJsonObject &document, const QString &networkName)
{
    const QJsonObject contextsByNetwork = document.value(QStringLiteral("workflow_contexts_by_network")).toObject();
    return contextsByNetwork.value(networkName).toObject();
}

void setWalletForNetwork(QJsonObject *document, const QString &networkName, const QJsonObject &wallet)
{
    if (!document) {
        return;
    }

    QJsonObject walletsByNetwork = document->value(QStringLiteral("wallets_by_network")).toObject();
    walletsByNetwork.insert(networkName, wallet);
    document->insert(QStringLiteral("wallets_by_network"), walletsByNetwork);
}

void setWalletStateForNetwork(QJsonObject *document, const QString &networkName, const QJsonObject &walletState)
{
    if (!document) {
        return;
    }

    QJsonObject walletStates = document->value(QStringLiteral("wallet_states")).toObject();
    walletStates.insert(networkName, walletState);
    document->insert(QStringLiteral("wallet_states"), walletStates);
}

void setWorkflowContextsForNetwork(QJsonObject *document, const QString &networkName, const QJsonObject &contexts)
{
    if (!document) {
        return;
    }

    QJsonObject contextsByNetwork = document->value(QStringLiteral("workflow_contexts_by_network")).toObject();
    contextsByNetwork.insert(networkName, contexts);
    document->insert(QStringLiteral("workflow_contexts_by_network"), contextsByNetwork);
}

void syncActiveNetworkView(QJsonObject *document, const QString &networkName)
{
    if (!document) {
        return;
    }

    document->insert(QStringLiteral("wallet"), walletForNetwork(*document, networkName));
    document->insert(QStringLiteral("wallet_state"), walletStateForNetwork(*document, networkName));
    document->insert(QStringLiteral("workflow_contexts"), workflowContextsForNetwork(*document, networkName));
}

void persistActiveNetworkView(QJsonObject *document, const QString &networkName)
{
    if (!document) {
        return;
    }

    setWalletForNetwork(document,
                        networkName,
                        document->value(QStringLiteral("wallet")).toObject());
    setWalletStateForNetwork(document,
                             networkName,
                             document->value(QStringLiteral("wallet_state")).toObject());
    setWorkflowContextsForNetwork(document,
                                  networkName,
                                  document->value(QStringLiteral("workflow_contexts")).toObject());
}

QJsonObject ensureDocumentSchema(const QJsonObject &rawDocument)
{
    QJsonObject document = rawDocument.isEmpty() ? defaultDocument() : rawDocument;
    const QString networkName = inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
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

QJsonObject normalizeDocumentSchema(const QJsonObject &rawDocument)
{
    QJsonObject document = ensureDocumentSchema(rawDocument);
    const QString networkName = inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                                                 document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    syncActiveNetworkView(&document, networkName);
    return document;
}

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

bool normalizeImportedDocument(const QJsonObject &candidate, QJsonObject *documentOut, QString *errorOut)
{
    if (!documentOut) {
        return false;
    }

    QJsonObject document = defaultDocument();
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
    setWalletForNetwork(&document, importedNetwork, normalizedWallet);
    document.insert(QStringLiteral("wallet"), normalizedWallet);

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
    setWalletStateForNetwork(&document, importedNetwork, normalizedState);

    const QJsonObject normalizedContexts =
        candidate.value(QStringLiteral("workflow_contexts")).isObject()
            ? candidate.value(QStringLiteral("workflow_contexts")).toObject()
            : QJsonObject();
    document.insert(QStringLiteral("workflow_contexts"), normalizedContexts);
    setWorkflowContextsForNetwork(&document, importedNetwork, normalizedContexts);

    *documentOut = document;
    return true;
}

QJsonObject extractImportedBackupDocument(const QByteArray &json, QString *errorOut)
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
    if (root.value(QStringLiteral("backup_kind")).toString() == QStringLiteral("grinffindor.encrypted_wallet_backup")) {
        candidate = root.value(QStringLiteral("document")).toObject();
    }

    QJsonObject normalized;
    if (!normalizeImportedDocument(candidate, &normalized, errorOut)) {
        return QJsonObject();
    }
    return normalized;
}


QJsonObject loadDocument()
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

bool saveDocument(const QJsonObject &document)
{
    ensureStorageReady();
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

QByteArray randomBytes(int size)
{
    QByteArray data(size, Qt::Uninitialized);
    for (int i = 0; i < size; ++i) {
        data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    return data;
}

quint64 amountToNanogrin(const QString &amount)
{
    const QString trimmed = amount.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    const QStringList parts = trimmed.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = parts.size() == 2 ? parts.at(1) : QString();
    if (fractional.size() > 9) {
        fractional = fractional.left(9);
    }
    while (fractional.size() < 9) {
        fractional.append(QLatin1Char('0'));
    }

    bool fracOk = false;
    const quint64 frac = fractional.isEmpty() ? 0 : fractional.toULongLong(&fracOk);
    if (!fractional.isEmpty() && !fracOk) {
        return 0;
    }

    return whole * 1000000000ULL + frac;
}

QString formatNanogrin(quint64 amount)
{
    return QStringLiteral("%1.%2")
        .arg(QString::number(amount / 1000000000ULL))
        .arg(QString::number(amount % 1000000000ULL), 9, QLatin1Char('0'));
}

QStringList &mnemonicWords()
{
    static QStringList words;
    if (!words.isEmpty()) {
        return words;
    }

    QFile file(QStringLiteral(":/qml/src/wallet/resources/bip39_english.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return words;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            words.append(line);
        }
    }
    return words;
}

QByteArray bitsFromBytes(const QByteArray &bytes)
{
    QByteArray bits;
    bits.reserve(bytes.size() * 8);
    for (int i = 0; i < bytes.size(); ++i) {
        const unsigned char value = static_cast<unsigned char>(bytes.at(i));
        for (int bit = 7; bit >= 0; --bit) {
            bits.append((value & (1u << bit)) ? '\x01' : '\x00');
        }
    }
    return bits;
}

QByteArray bytesFromBits(const QByteArray &bits)
{
    QByteArray bytes;
    bytes.reserve(bits.size() / 8);
    for (int i = 0; i + 7 < bits.size(); i += 8) {
        unsigned char value = 0;
        for (int bit = 0; bit < 8; ++bit) {
            value = static_cast<unsigned char>((value << 1) | (bits.at(i + bit) ? 1 : 0));
        }
        bytes.append(static_cast<char>(value));
    }
    return bytes;
}

QString normalizeMnemonic(const QString &mnemonic)
{
    QString normalized = mnemonic.toLower().trimmed();
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized;
}

quint32 nextChildIndexFromState(const QJsonObject &walletState)
{
    return static_cast<quint32>(walletState.value(QStringLiteral("next_child_index")).toInt());
}

QString mnemonicFromEntropy(const QByteArray &entropy)
{
    const QStringList &words = mnemonicWords();
    if (entropy.size() != kMnemonicEntropyBytes || words.size() != 2048) {
        return QString();
    }

    QByteArray bits = bitsFromBytes(entropy);
    const QByteArray checksumBits = bitsFromBytes(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256));
    const int checksumLength = entropy.size() * 8 / 32;
    bits.append(checksumBits.left(checksumLength));

    QStringList mnemonic;
    for (int i = 0; i + 10 < bits.size(); i += 11) {
        int index = 0;
        for (int bit = 0; bit < 11; ++bit) {
            index = (index << 1) | (bits.at(i + bit) ? 1 : 0);
        }
        mnemonic.append(words.at(index));
    }
    return mnemonic.join(QStringLiteral(" "));
}

bool entropyFromMnemonic(const QString &mnemonic, QByteArray *entropyOut)
{
    const QString normalized = normalizeMnemonic(mnemonic);
    const QStringList parts = normalized.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QStringList &words = mnemonicWords();
    if (parts.size() != 24 || words.size() != 2048) {
        return false;
    }

    QByteArray bits;
    for (int i = 0; i < parts.size(); ++i) {
        const int index = words.indexOf(parts.at(i));
        if (index < 0) {
            return false;
        }
        for (int bit = 10; bit >= 0; --bit) {
            bits.append((index & (1 << bit)) ? '\x01' : '\x00');
        }
    }

    const int checksumLength = bits.size() / 33;
    const int entropyLength = bits.size() - checksumLength;
    const QByteArray entropy = bytesFromBits(bits.left(entropyLength));
    if (entropy.size() != kMnemonicEntropyBytes) {
        return false;
    }

    const QByteArray checksumBits = bitsFromBytes(QCryptographicHash::hash(entropy, QCryptographicHash::Sha256));
    if (bits.mid(entropyLength, checksumLength) != checksumBits.left(checksumLength)) {
        return false;
    }

    if (entropyOut) {
        *entropyOut = entropy;
    }
    return true;
}

bool validateMnemonicValue(const QString &mnemonic)
{
    return entropyFromMnemonic(mnemonic, 0);
}

bool isFinalTransactionStatus(const QString &status)
{
    return status == QStringLiteral("confirmed")
        || status == QStringLiteral("cancelled")
        || status == QStringLiteral("spent");
}

QByteArray deriveLegacyKeyMaterial(const QString &password, const QByteArray &salt)
{
    const QByteArray material = password.toUtf8() + salt;
    QByteArray digest = QCryptographicHash::hash(material, QCryptographicHash::Sha256);
    for (int i = 0; i < 120000; ++i) {
        digest = QCryptographicHash::hash(digest + material, QCryptographicHash::Sha256);
    }
    return digest;
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

bool deriveKeyMaterialV3(const QString &password,
                         const QByteArray &salt,
                         int blocks,
                         int passes,
                         QByteArray *keyOut)
{
    if (!keyOut || salt.isEmpty() || blocks < 8 || passes < 1) {
        return false;
    }

    QByteArray keyMaterial(kSeedCipherKeyBytes, Qt::Uninitialized);
    QByteArray workArea(blocks * 1024, Qt::Uninitialized);
    const QByteArray passwordBytes = password.toUtf8();
    static const QByteArray additionalData("grinffindor.seed.v3", 19);

    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = static_cast<uint32_t>(blocks);
    config.nb_passes = static_cast<uint32_t>(passes);
    config.nb_lanes = kSeedCipherArgon2Lanes;

    crypto_argon2_inputs inputs;
    inputs.pass = reinterpret_cast<const uint8_t *>(passwordBytes.constData());
    inputs.salt = reinterpret_cast<const uint8_t *>(salt.constData());
    inputs.pass_size = static_cast<uint32_t>(passwordBytes.size());
    inputs.salt_size = static_cast<uint32_t>(salt.size());

    crypto_argon2_extras extras = crypto_argon2_no_extras;
    extras.ad = reinterpret_cast<const uint8_t *>(additionalData.constData());
    extras.ad_size = static_cast<uint32_t>(additionalData.size());

    crypto_argon2(reinterpret_cast<uint8_t *>(keyMaterial.data()),
                  static_cast<uint32_t>(keyMaterial.size()),
                  workArea.data(),
                  config,
                  inputs,
                  extras);

    crypto_wipe(workArea.data(), static_cast<size_t>(workArea.size()));
    *keyOut = keyMaterial;
    crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
    return true;
}

QJsonObject encryptMnemonic(const QString &mnemonic, const QString &password)
{
    const QByteArray salt = randomBytes(16);
    const QByteArray nonce = randomBytes(24);
    const QByteArray plain = normalizeMnemonic(mnemonic).toUtf8();
    QByteArray keyMaterial;
    if (!deriveKeyMaterialV3(password,
                             salt,
                             kSeedCipherArgon2Blocks,
                             kSeedCipherArgon2Passes,
                             &keyMaterial)) {
        return QJsonObject();
    }

    QByteArray cipher(plain.size(), Qt::Uninitialized);
    QByteArray mac(kSeedCipherMacBytes, Qt::Uninitialized);
    static const QByteArray associatedData("grinffindor.seed-store", 22);
    crypto_aead_lock(reinterpret_cast<uint8_t *>(cipher.data()),
                     reinterpret_cast<uint8_t *>(mac.data()),
                     reinterpret_cast<const uint8_t *>(keyMaterial.constData()),
                     reinterpret_cast<const uint8_t *>(nonce.constData()),
                     reinterpret_cast<const uint8_t *>(associatedData.constData()),
                     static_cast<size_t>(associatedData.size()),
                     reinterpret_cast<const uint8_t *>(plain.constData()),
                     static_cast<size_t>(plain.size()));

    QJsonObject encrypted;
    encrypted.insert(QStringLiteral("version"), kSeedCipherVersion);
    encrypted.insert(QStringLiteral("kdf_algorithm"), QStringLiteral("argon2id"));
    encrypted.insert(QStringLiteral("kdf_blocks"), kSeedCipherArgon2Blocks);
    encrypted.insert(QStringLiteral("kdf_passes"), kSeedCipherArgon2Passes);
    encrypted.insert(QStringLiteral("kdf_lanes"), kSeedCipherArgon2Lanes);
    encrypted.insert(QStringLiteral("salt"), QString::fromUtf8(salt.toBase64()));
    encrypted.insert(QStringLiteral("nonce"), QString::fromUtf8(nonce.toBase64()));
    encrypted.insert(QStringLiteral("cipher"), QString::fromUtf8(cipher.toBase64()));
    encrypted.insert(QStringLiteral("mac"), QString::fromUtf8(mac.toBase64()));
    crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
    return encrypted;
}

bool decryptMnemonic(const QJsonObject &encrypted, const QString &password, QString *mnemonicOut)
{
    const int version = encrypted.value(QStringLiteral("version")).toInt(1);
    const QByteArray salt = QByteArray::fromBase64(encrypted.value(QStringLiteral("salt")).toString().toUtf8());
    const QByteArray nonce = QByteArray::fromBase64(encrypted.value(QStringLiteral("nonce")).toString().toUtf8());
    const QByteArray cipher = QByteArray::fromBase64(encrypted.value(QStringLiteral("cipher")).toString().toUtf8());
    const QByteArray mac = QByteArray::fromBase64(encrypted.value(QStringLiteral("mac")).toString().toUtf8());

    if (version >= kSeedCipherVersion) {
        const int blocks = std::max(8, encrypted.value(QStringLiteral("kdf_blocks")).toInt(kSeedCipherArgon2Blocks));
        const int passes = std::max(1, encrypted.value(QStringLiteral("kdf_passes")).toInt(kSeedCipherArgon2Passes));
        QByteArray keyMaterial;
        if (!deriveKeyMaterialV3(password, salt, blocks, passes, &keyMaterial)) {
            return false;
        }

        if (nonce.size() != 24 || mac.size() != kSeedCipherMacBytes) {
            crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
            return false;
        }

        QByteArray plain(cipher.size(), Qt::Uninitialized);
        static const QByteArray associatedData("grinffindor.seed-store", 22);
        const int unlockResult =
            crypto_aead_unlock(reinterpret_cast<uint8_t *>(plain.data()),
                               reinterpret_cast<const uint8_t *>(mac.constData()),
                               reinterpret_cast<const uint8_t *>(keyMaterial.constData()),
                               reinterpret_cast<const uint8_t *>(nonce.constData()),
                               reinterpret_cast<const uint8_t *>(associatedData.constData()),
                               static_cast<size_t>(associatedData.size()),
                               reinterpret_cast<const uint8_t *>(cipher.constData()),
                               static_cast<size_t>(cipher.size()));
        crypto_wipe(keyMaterial.data(), static_cast<size_t>(keyMaterial.size()));
        if (unlockResult != 0) {
            crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
            return false;
        }

        const QString mnemonic = normalizeMnemonic(QString::fromUtf8(plain));
        crypto_wipe(plain.data(), static_cast<size_t>(plain.size()));
        if (!validateMnemonicValue(mnemonic)) {
            return false;
        }
        if (mnemonicOut) {
            *mnemonicOut = mnemonic;
        }
        return true;
    }

    QByteArray encryptionKey;
    QByteArray macKey;
    if (version == 2) {
        const int iterations = std::max(1, encrypted.value(QStringLiteral("kdf_iterations")).toInt(240000));
        const QByteArray keyMaterial = deriveKeyMaterialV2(password, salt, iterations, 64);
        encryptionKey = keyMaterial.left(32);
        macKey = keyMaterial.mid(32, 32);
    } else {
        const QByteArray legacyKey = deriveLegacyKeyMaterial(password, salt);
        encryptionKey = legacyKey;
        macKey = legacyKey;
    }

    const QByteArray expectedMac = QCryptographicHash::hash(macKey + nonce + cipher + macKey, QCryptographicHash::Sha256);
    if (expectedMac != mac) {
        return false;
    }

    const QString mnemonic = normalizeMnemonic(QString::fromUtf8(xorStream(cipher, encryptionKey, nonce)));
    if (!validateMnemonicValue(mnemonic)) {
        return false;
    }
    if (mnemonicOut) {
        *mnemonicOut = mnemonic;
    }
    return true;
}

QString seedFingerprintForMnemonic(const QString &mnemonic)
{
    return QString::fromUtf8(
        QCryptographicHash::hash(normalizeMnemonic(mnemonic).toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

QString generateWorkflowId()
{
    return QString::fromUtf8(randomBytes(16).toHex());
}

QString amountStringFromJson(const QJsonObject &balances, const QString &field)
{
    return balances.value(field).toString(QStringLiteral("0.000000000"));
}

WalletOutput findTrackedOutputByCommitment(const QList<WalletOutput> &outputs, const QString &commitment)
{
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commitment) {
            return outputs.at(i);
        }
    }
    return WalletOutput();
}

QString syntheticWorkflowIdForCommitment(const QString &commitment)
{
    return QStringLiteral("rescan-%1").arg(commitment.left(24));
}

QStringList transactionOutputCommitments(const QJsonObject &entry)
{
    QStringList commitments;
    const QString directCommitment = entry.value(QStringLiteral("commitment")).toString();
    if (!directCommitment.isEmpty()) {
        commitments.append(directCommitment);
    }

    const QJsonArray outputs = entry.value(QStringLiteral("tx_skeleton"))
                               .toObject()
                               .value(QStringLiteral("body"))
                               .toObject()
                               .value(QStringLiteral("outputs"))
                               .toArray();
    for (int i = 0; i < outputs.size(); ++i) {
        const QString commitment = outputs.at(i).toObject().value(QStringLiteral("commit")).toString();
        if (!commitment.isEmpty() && !commitments.contains(commitment)) {
            commitments.append(commitment);
        }
    }

    return commitments;
}

QJsonObject filterWorkflowContextsForTransactions(const QJsonObject &contexts,
                                                  const QJsonArray &transactions)
{
    QJsonObject filtered;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        const QString status = entry.value(QStringLiteral("status")).toString();
        if (workflowId.isEmpty() || isFinalTransactionStatus(status)) {
            continue;
        }
        if (contexts.contains(workflowId)) {
            filtered.insert(workflowId, contexts.value(workflowId).toObject());
        }
    }
    return filtered;
}

QString modeFromOutputs(const QList<WalletOutput> &outputs, const QString &fallbackMode)
{
    if (!fallbackMode.trimmed().isEmpty()) {
        return fallbackMode;
    }

    bool hasChange = false;
    bool hasReceiveLike = false;
    for (int i = 0; i < outputs.size(); ++i) {
        const QString source = outputs.at(i).source;
        if (source == QStringLiteral("change")) {
            hasChange = true;
        }
        if (source == QStringLiteral("receive") || source == QStringLiteral("invoice")) {
            hasReceiveLike = true;
        }
    }

    if (hasChange) {
        return QStringLiteral("send");
    }
    if (hasReceiveLike) {
        return QStringLiteral("receive");
    }
    return QStringLiteral("receive");
}

QString encodeBase58(const QByteArray &input)
{
    if (input.isEmpty()) {
        return QString();
    }

    QVector<int> digits(1, 0);
    for (int i = 0; i < input.size(); ++i) {
        int carry = static_cast<unsigned char>(input.at(i));
        for (int j = 0; j < digits.size(); ++j) {
            carry += digits[j] << 8;
            digits[j] = carry % 58;
            carry /= 58;
        }
        while (carry > 0) {
            digits.append(carry % 58);
            carry /= 58;
        }
    }

    QString result;
    for (int i = 0; i < input.size() && input.at(i) == '\0'; ++i) {
        result.append(QLatin1Char('1'));
    }
    for (int i = digits.size() - 1; i >= 0; --i) {
        result.append(QLatin1Char(kBase58Alphabet[digits.at(i)]));
    }
    return result;
}

QByteArray decodeBase58(const QString &text)
{
    QByteArray output;
    if (text.isEmpty()) {
        return output;
    }

    QVector<int> bytes(1, 0);
    for (int i = 0; i < text.size(); ++i) {
        const int value = QByteArray(kBase58Alphabet).indexOf(text.at(i).toLatin1());
        if (value < 0) {
            return QByteArray();
        }

        int carry = value;
        for (int j = 0; j < bytes.size(); ++j) {
            carry += bytes[j] * 58;
            bytes[j] = carry & 0xff;
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.append(carry & 0xff);
            carry >>= 8;
        }
    }

    for (int i = 0; i < text.size() && text.at(i) == QLatin1Char('1'); ++i) {
        output.append('\0');
    }
    for (int i = bytes.size() - 1; i >= 0; --i) {
        output.append(static_cast<char>(bytes.at(i)));
    }
    return output;
}

QString formatArmored(const QString &data)
{
    QString out;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) {
            if (i % (15 * 200) == 0) {
                out.append(QLatin1Char('\n'));
            } else if (i % 15 == 0) {
                out.append(QLatin1Char(' '));
            }
        }
        out.append(data.at(i));
    }
    return out;
}

QString encodeSlatepackArmor(const QString &payloadJson, const QString &sender)
{
    QJsonObject envelope;
    QJsonObject version;
    version.insert(QStringLiteral("major"), 1);
    version.insert(QStringLiteral("minor"), 0);
    envelope.insert(QStringLiteral("slatepack"), version);
    envelope.insert(QStringLiteral("mode"), 0);
    if (!sender.trimmed().isEmpty()) {
        envelope.insert(QStringLiteral("sender"), sender.trimmed());
    }
    envelope.insert(QStringLiteral("payload"), QString::fromUtf8(payloadJson.toUtf8().toBase64()));

    const QByteArray serialized = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    const QByteArray checksum = QCryptographicHash::hash(
        QCryptographicHash::hash(serialized, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256).left(4);
    return QStringLiteral("BEGINSLATEPACK. %1. ENDSLATEPACK.\n").arg(formatArmored(encodeBase58(checksum + serialized)));
}

QString userFacingSlatepackParseNote(const QString &parseError);
QString buildSlatepackDiagnostic(const QString &kind,
                                 const QByteArray &payload,
                                 const QString &note);

QString decodeSlatepackArmor(const QString &slatepack)
{
    QString cleaned = slatepack;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() < 3
        || parts.at(0) != QStringLiteral("BEGINSLATEPACK")
        || parts.at(2) != QStringLiteral("ENDSLATEPACK")) {
        return QString();
    }

    const QByteArray decoded = decodeBase58(parts.at(1));
    if (decoded.size() < 5) {
        return QString();
    }

    const QByteArray checksum = decoded.left(4);
    const QByteArray payload = decoded.mid(4);
    const QByteArray expected = QCryptographicHash::hash(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256),
        QCryptographicHash::Sha256).left(4);
    if (checksum != expected) {
        return QString();
    }

    QString decodedPayload;
    QString parseError;
    if (!BinarySlateV4Reader::decodeSlatepackPayload(payload, QByteArray(), &decodedPayload, &parseError)) {
        return buildSlatepackDiagnostic(QStringLiteral("armored"), payload, parseError);
    }
    return decodedPayload;
}

QString userFacingSlatepackParseNote(const QString &parseError)
{
    const QString trimmed = parseError.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("Slatepack payload could not be parsed.");
    }

    if (trimmed.contains(QStringLiteral("Wallet secret is unavailable"), Qt::CaseInsensitive)) {
        return QStringLiteral("This Slatepack is encrypted. Unlock the wallet that owns the recipient address before decoding it.");
    }
    if (trimmed.contains(QStringLiteral("not addressed to this wallet"), Qt::CaseInsensitive)) {
        return QStringLiteral("This Slatepack is encrypted for a different recipient wallet.");
    }
    if (trimmed.contains(QStringLiteral("authentication failed"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("invalid base64"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("truncated"), Qt::CaseInsensitive)
        || trimmed.contains(QStringLiteral("malformed"), Qt::CaseInsensitive)) {
        return QStringLiteral("The Slatepack looks damaged or incomplete. Copy it again and make sure no characters are missing.");
    }

    return trimmed;
}

QString buildSlatepackDiagnostic(const QString &kind,
                                const QByteArray &payload,
                                const QString &note)
{
    QJsonObject diagnostic;
    diagnostic.insert(QStringLiteral("external_slatepack"), true);
    diagnostic.insert(QStringLiteral("diagnostic_kind"), kind);
    diagnostic.insert(QStringLiteral("payload_size"), payload.size());
    diagnostic.insert(QStringLiteral("payload_hex_preview"), QString::fromUtf8(payload.left(96).toHex()));
    diagnostic.insert(QStringLiteral("note"), userFacingSlatepackParseNote(note));
    return QString::fromUtf8(QJsonDocument(diagnostic).toJson(QJsonDocument::Indented));
}

QString decodeIncomingSlatepack(const QString &input, const QByteArray &decryptionKey)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QJsonDocument jsonDocument = QJsonDocument::fromJson(trimmed.toUtf8());
    if (jsonDocument.isObject()) {
        QString decodedPayload;
        QString parseError;
        if (BinarySlateV4Reader::decodeSlatepackPayload(trimmed.toUtf8(), decryptionKey, &decodedPayload, &parseError)) {
            return decodedPayload;
        }

        const QJsonObject object = jsonDocument.object();
        if (object.contains(QStringLiteral("slatepack")) || object.contains(QStringLiteral("payload"))) {
            const QByteArray payload = QByteArray::fromBase64(object.value(QStringLiteral("payload")).toString().toUtf8());
            return buildSlatepackDiagnostic(QStringLiteral("json"), payload, parseError);
        }

        return QString::fromUtf8(jsonDocument.toJson(QJsonDocument::Indented));
    }

    QString cleaned = trimmed;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() >= 3
        && parts.at(0) == QStringLiteral("BEGINSLATEPACK")
        && parts.at(2) == QStringLiteral("ENDSLATEPACK")) {
        const QByteArray decoded = decodeBase58(parts.at(1));
        if (decoded.size() >= 5) {
            const QByteArray payload = decoded.mid(4);
            QString decodedPayload;
            QString parseError;
            if (BinarySlateV4Reader::decodeSlatepackPayload(payload, decryptionKey, &decodedPayload, &parseError)) {
                return decodedPayload;
            }
            return buildSlatepackDiagnostic(QStringLiteral("armored"), payload, parseError);
        }
    }

    return decodeSlatepackArmor(trimmed);
}

bool invokeNoArgMethod(QObject *object, const char *methodName)
{
    return object && QMetaObject::invokeMethod(object, methodName, Qt::DirectConnection);
}

QString focusedObjectText(QObject *object)
{
    if (!object) {
        return QString();
    }

    const QVariant selectedText = object->property("selectedText");
    if (selectedText.isValid()) {
        const QString selected = selectedText.toString();
        if (!selected.isEmpty()) {
            return selected;
        }
    }

    const QVariant text = object->property("text");
    if (text.isValid()) {
        return text.toString();
    }

    return QString();
}

} // namespace

GrinWalletController::GrinWalletController(QObject *parent) :
    QObject(parent),
    m_nodeApi(0),
    m_autoRefreshTimer(0),
    m_sessionLockTimer(0),
    m_walletExists(false),
    m_walletUnlocked(false),
    m_selectedNetwork(defaultNetworkName()),
    m_storagePersistenceState(QStringLiteral("unknown")),
    m_chainHeight(0),
    m_syncStatus(QStringLiteral("Idle")),
    m_totalBalance(QStringLiteral("0.000000000")),
    m_spendableBalance(QStringLiteral("0.000000000")),
    m_lockedBalance(QStringLiteral("0.000000000")),
    m_immatureBalance(QStringLiteral("0.000000000")),
    m_scanHeight(0),
    m_walletScanInFlight(false),
    m_seedScanActive(false),
    m_seedScanNextIndex(1),
    m_broadcastStatusRefreshInFlight(false),
    m_kernelStatusCheckInFlight(false)
{
}

bool GrinWalletController::walletExists() const { return m_walletExists; }
bool GrinWalletController::walletUnlocked() const { return m_walletUnlocked; }
QString GrinWalletController::walletName() const { return m_walletName; }
QString GrinWalletController::mnemonicPreview() const { return m_mnemonicPreview; }
QString GrinWalletController::seedFingerprint() const { return m_seedFingerprint; }
QString GrinWalletController::selectedNetwork() const { return m_selectedNetwork; }
QString GrinWalletController::nodeUrl() const { return m_nodeUrl; }
QString GrinWalletController::storagePersistenceState() const { return m_storagePersistenceState; }
qulonglong GrinWalletController::chainHeight() const { return m_chainHeight; }
QString GrinWalletController::syncStatus() const { return m_syncStatus; }
QString GrinWalletController::totalBalance() const { return m_totalBalance; }
QString GrinWalletController::spendableBalance() const { return m_spendableBalance; }
QString GrinWalletController::lockedBalance() const { return m_lockedBalance; }
QString GrinWalletController::immatureBalance() const { return m_immatureBalance; }
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }
QString GrinWalletController::lastError() const { return m_lastError; }
QString GrinWalletController::lastInfo() const { return m_lastInfo; }
QString GrinWalletController::workflowId() const { return m_workflowId; }
QString GrinWalletController::workflowState() const { return m_workflowState; }
QString GrinWalletController::workflowMode() const { return m_workflowMode; }
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }
QVariantList GrinWalletController::transactionHistory() const
{
    QVariantList history;
    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    history.reserve(transactions.size());
    for (int i = transactions.size() - 1; i >= 0; --i) {
        history.append(transactions.at(i).toObject().toVariantMap());
    }
    return history;
}

QVariantList GrinWalletController::walletOutputs() const
{
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    std::sort(outputs.begin(), outputs.end(), [](const WalletOutput &left, const WalletOutput &right) {
        if (left.spent != right.spent) {
            return !left.spent && right.spent;
        }
        if (left.locked != right.locked) {
            return left.locked && !right.locked;
        }
        if (left.pending != right.pending) {
            return left.pending && !right.pending;
        }
        if (left.onChain != right.onChain) {
            return left.onChain && !right.onChain;
        }
        if (left.height != right.height) {
            return left.height > right.height;
        }
        return left.commitment < right.commitment;
    });

    QVariantList list;
    list.reserve(outputs.size());
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        QJsonObject entry = output.toJson();

        QString status;
        if (output.spent) {
            status = QStringLiteral("spent");
        } else if (output.pending) {
            status = QStringLiteral("pending");
        } else if (output.locked) {
            status = QStringLiteral("locked");
        } else if (output.coinbase && (!output.onChain || output.height == 0 || m_chainHeight < output.height + 1000)) {
            status = QStringLiteral("immature");
        } else if (!output.onChain) {
            status = QStringLiteral("local");
        } else {
            status = QStringLiteral("spendable");
        }

        const bool mature = !output.coinbase || (output.height > 0 && m_chainHeight >= output.height + 1000);
        const bool confirmed = output.onChain && output.height > 0 && m_chainHeight >= output.height + 10;
        const qint64 confirmations =
            output.height > 0 && m_chainHeight >= output.height
                ? static_cast<qint64>(m_chainHeight - output.height + 1)
                : 0;
        const bool spendable = !output.spent && !output.locked && !output.pending && output.onChain && mature && confirmed;

        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("mature"), mature);
        entry.insert(QStringLiteral("confirmed"), confirmed);
        entry.insert(QStringLiteral("confirmations"), confirmations);
        entry.insert(QStringLiteral("spendable"), spendable);
        list.append(entry.toVariantMap());
    }

    return list;
}

void GrinWalletController::initialize()
{
    qApp->installEventFilter(this);
    loadFromStorage();
    connectNodeClient();
    startAutoRefresh();
    refreshStoragePersistenceState();
#ifdef Q_OS_WASM
    browserInstallWalletShortcutBridge();
#endif
    refreshNodeStatus();
}

bool GrinWalletController::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    if (!event || event->type() != QEvent::KeyPress) {
        return QObject::eventFilter(watched, event);
    }

    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
    if (!(keyEvent->modifiers() & Qt::ControlModifier) || (keyEvent->modifiers() & Qt::AltModifier)) {
        return QObject::eventFilter(watched, event);
    }

    const int key = keyEvent->key();
    if (key != Qt::Key_A && key != Qt::Key_C) {
        return QObject::eventFilter(watched, event);
    }

    QObject *focusObject = qApp->focusObject();
    if (!focusObject) {
        return QObject::eventFilter(watched, event);
    }

    if (key == Qt::Key_A) {
        if (invokeNoArgMethod(focusObject, "selectAll")) {
            keyEvent->accept();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

    if (invokeNoArgMethod(focusObject, "copy")) {
        keyEvent->accept();
        return true;
    }

    const QString copiedText = focusedObjectText(focusObject);
    if (!copiedText.isEmpty() && copyTextToClipboard(copiedText)) {
        keyEvent->accept();
        return true;
    }

    return QObject::eventFilter(watched, event);
}

QString GrinWalletController::generateMnemonic() const
{
    return mnemonicFromEntropy(randomBytes(kMnemonicEntropyBytes));
}

bool GrinWalletController::validateMnemonic(const QString &mnemonic) const
{
    return validateMnemonicValue(mnemonic);
}

void GrinWalletController::createWallet(const QString &walletName, const QString &password)
{
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }

    const QString mnemonic = generateMnemonic();
    if (mnemonic.isEmpty()) {
        setLastError(QStringLiteral("Failed to generate a valid seed phrase."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject wallet;
    const QJsonObject encryptedSeed = encryptMnemonic(mnemonic, password);
    if (encryptedSeed.isEmpty()) {
        setLastError(QStringLiteral("Failed to encrypt wallet seed for local storage."));
        return;
    }
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), seedFingerprintForMnemonic(mnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptedSeed);
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("generated"));
    wallet.insert(QStringLiteral("network"), resolvedNetworkName());
    document.insert(QStringLiteral("wallet"), wallet);
    setWalletForNetwork(&document, resolvedNetworkName(), wallet);
    setWalletStateForNetwork(&document, resolvedNetworkName(), defaultWalletState());
    setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());

    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = mnemonic;
    m_mnemonicPreview = mnemonic;
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet created locally. Save the seed phrase now - it will not be shown again after this session."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::importWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{
    restoreWallet(walletName, mnemonic, password);
}

void GrinWalletController::restoreWallet(const QString &walletName, const QString &mnemonic, const QString &password)
{
    const QString normalizedMnemonic = normalizeMnemonic(mnemonic);
    if (walletName.trimmed().isEmpty() || password.isEmpty()) {
        setLastError(QStringLiteral("Wallet name and password are required."));
        return;
    }
    if (!validateMnemonicValue(normalizedMnemonic)) {
        setLastError(QStringLiteral("Mnemonic is not valid BIP39 input."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject wallet;
    const QJsonObject encryptedSeed = encryptMnemonic(normalizedMnemonic, password);
    if (encryptedSeed.isEmpty()) {
        setLastError(QStringLiteral("Failed to encrypt wallet seed for local storage."));
        return;
    }
    wallet.insert(QStringLiteral("name"), walletName.trimmed());
    wallet.insert(QStringLiteral("seed_fingerprint"), seedFingerprintForMnemonic(normalizedMnemonic));
    wallet.insert(QStringLiteral("encrypted_seed"), encryptedSeed);
    wallet.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    wallet.insert(QStringLiteral("seed_origin"), QStringLiteral("restored"));
    wallet.insert(QStringLiteral("network"), resolvedNetworkName());
    document.insert(QStringLiteral("wallet"), wallet);
    setWalletForNetwork(&document, resolvedNetworkName(), wallet);
    setWalletStateForNetwork(&document, resolvedNetworkName(), defaultWalletState());
    setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());

    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet in browser storage."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = walletName.trimmed();
    m_sessionMnemonic = normalizedMnemonic;
    m_mnemonicPreview.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    emit walletChanged();
    refreshStateFromStorage();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet restored locally. Seed is encrypted in local storage and stays hidden after setup."));
    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::unlockWallet(const QString &password)
{
    QJsonObject document = loadDocument();
    QJsonObject wallet = walletForNetwork(document, resolvedNetworkName());
    QString mnemonic;
    if (wallet.isEmpty()
        || !decryptMnemonic(wallet.value(QStringLiteral("encrypted_seed")).toObject(), password, &mnemonic)) {
        setLastError(QStringLiteral("Failed to unlock wallet. Password is invalid or local data is corrupted."));
        return;
    }

    m_walletExists = true;
    m_walletUnlocked = true;
    m_walletName = wallet.value(QStringLiteral("name")).toString();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    m_sessionMnemonic = mnemonic;
    m_mnemonicPreview.clear();
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet unlocked locally."));

    const QJsonObject encryptedSeed = wallet.value(QStringLiteral("encrypted_seed")).toObject();
    if (encryptedSeed.value(QStringLiteral("version")).toInt(1) < kSeedCipherVersion) {
        const QJsonObject upgradedSeed = encryptMnemonic(mnemonic, password);
        if (!upgradedSeed.isEmpty()) {
            wallet.insert(QStringLiteral("encrypted_seed"), upgradedSeed);
            document.insert(QStringLiteral("wallet"), wallet);
            setWalletForNetwork(&document, resolvedNetworkName(), wallet);
            saveDocument(document);
        }
    }

    if (m_scanHeight == 0) {
        rescanWallet();
    }
}

void GrinWalletController::lockWallet()
{
    m_walletUnlocked = false;
    m_sessionMnemonic.clear();
    m_mnemonicPreview.clear();
    if (m_sessionLockTimer) {
        m_sessionLockTimer->stop();
    }
    emit walletChanged();
    setLastInfo(QStringLiteral("Wallet locked. Seed material cleared from the UI state."));
}

void GrinWalletController::clearLastError()
{
    if (m_lastError.isEmpty()) {
        return;
    }
    setLastError(QString());
}

void GrinWalletController::dismissMnemonicPreview()
{
    if (m_mnemonicPreview.isEmpty()) {
        return;
    }

    m_mnemonicPreview.clear();
    emit walletChanged();
    setLastInfo(QStringLiteral("Seed phrase hidden. Use your password to unlock the wallet next time."));
}

bool GrinWalletController::revealSeedPhrase(const QString &password)
{
    if (password.isEmpty()) {
        setLastError(QStringLiteral("Password is required to reveal the seed phrase."));
        return false;
    }

    const QJsonObject document = loadDocument();
    const QJsonObject wallet = walletForNetwork(document, resolvedNetworkName());
    QString mnemonic;
    if (wallet.isEmpty()
        || !decryptMnemonic(wallet.value(QStringLiteral("encrypted_seed")).toObject(), password, &mnemonic)) {
        setLastError(QStringLiteral("Failed to reveal seed phrase. Password is invalid or local data is corrupted."));
        return false;
    }

    m_mnemonicPreview = mnemonic;
    emit walletChanged();
    touchWalletSession();
    setLastError(QString());
    setLastInfo(QStringLiteral("Seed phrase revealed for the active wallet."));
    return true;
}

void GrinWalletController::deleteWallet()
{
    QJsonObject document = loadDocument();
    document.insert(QStringLiteral("wallet"), defaultWalletMetadata());
    setWalletForNetwork(&document, resolvedNetworkName(), defaultWalletMetadata());
    setWalletStateForNetwork(&document, resolvedNetworkName(), defaultWalletState());
    setWorkflowContextsForNetwork(&document, resolvedNetworkName(), QJsonObject());
    syncActiveNetworkView(&document, resolvedNetworkName());

    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to delete the local wallet configuration."));
        return;
    }

    clearWorkflow();
    loadFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Local wallet configuration deleted. You can now create or restore a wallet."));
}

QString GrinWalletController::exportEncryptedWalletBackup() const
{
    const QJsonObject document = loadDocument();
    const QJsonObject wallet = walletForNetwork(document, resolvedNetworkName());
    if (wallet.isEmpty()) {
        return QString();
    }

    QJsonObject backup;
    backup.insert(QStringLiteral("backup_kind"), QStringLiteral("grinffindor.encrypted_wallet_backup"));
    backup.insert(QStringLiteral("backup_version"), 1);
    backup.insert(QStringLiteral("exported_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    backup.insert(QStringLiteral("wallet_name"), wallet.value(QStringLiteral("name")).toString());
    backup.insert(QStringLiteral("seed_fingerprint"), wallet.value(QStringLiteral("seed_fingerprint")).toString());
    backup.insert(QStringLiteral("document"), document);
    return QString::fromUtf8(QJsonDocument(backup).toJson(QJsonDocument::Indented));
}

bool GrinWalletController::importEncryptedWalletBackup(const QString &backupJson)
{
    if (m_walletExists) {
        setLastError(QStringLiteral("Delete the current local wallet for this network before importing an encrypted backup."));
        return false;
    }

    const QString trimmed = backupJson.trimmed();
    if (trimmed.isEmpty()) {
        setLastError(QStringLiteral("Encrypted wallet backup text is required."));
        return false;
    }

    QString validationError;
    const QJsonObject imported = extractImportedBackupDocument(trimmed.toUtf8(), &validationError);
    if (imported.isEmpty()) {
        setLastError(validationError.isEmpty()
                         ? QStringLiteral("Encrypted wallet backup is invalid.")
                         : validationError);
        return false;
    }

    if (!saveDocument(imported)) {
        setLastError(QStringLiteral("Failed to persist imported wallet backup."));
        return false;
    }

    clearWorkflow();
    loadFromStorage();
    setLastError(QString());
    setLastInfo(QStringLiteral("Encrypted wallet backup imported. Unlock it with the backup password."));
    return true;
}

bool GrinWalletController::setNodeUrl(const QString &nodeUrl)
{
    const QString trimmed = nodeUrl.trimmed();
    if (!isNodeUrlAccepted(trimmed)) {
        setLastError(QStringLiteral("Node URL must be a valid http or https endpoint."));
        return false;
    }

    QJsonObject document = loadDocument();
    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("network"),
                inferNetworkName(node.value(QStringLiteral("network")).toString(), trimmed));
    node.insert(QStringLiteral("url"), trimmed);
    document.insert(QStringLiteral("node"), node);
    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist node settings."));
        return false;
    }

    m_selectedNetwork = node.value(QStringLiteral("network")).toString(defaultNetworkName());
    m_nodeUrl = trimmed;
    emit nodeConfigChanged();
    connectNodeClient();
    setLastError(QString());
    setLastInfo(QStringLiteral("External node updated. Reconnecting to %1").arg(trimmed));
    refreshNodeStatus();
    return true;
}

bool GrinWalletController::setSelectedNetwork(const QString &networkName)
{
    const QString normalized = networkName.trimmed().toLower();
    if (!isAcceptedNetworkName(normalized)) {
        setLastError(QStringLiteral("Wallet network must be either mainnet or testnet."));
        return false;
    }

    QJsonObject document = loadDocument();
    persistActiveNetworkView(&document, resolvedNetworkName());
    QJsonObject node = document.value(QStringLiteral("node")).toObject();
    node.insert(QStringLiteral("network"), normalized);
    node.insert(QStringLiteral("url"), defaultNodeUrlForNetwork(normalized));
    document.insert(QStringLiteral("node"), node);
    syncActiveNetworkView(&document, normalized);
    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist wallet network settings."));
        return false;
    }

    loadFromStorage();
    connectNodeClient();
    setLastError(QString());
    setLastInfo(QStringLiteral("Wallet network switched to %1. Reconnecting to %2")
                    .arg(normalized, m_nodeUrl));
    refreshNodeStatus();
    return true;
}

void GrinWalletController::resetNodeUrl()
{
    setNodeUrl(defaultNodeUrlForNetwork(resolvedNetworkName()));
}

void GrinWalletController::refreshNodeStatus()
{
    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    m_syncStatus = QStringLiteral("Querying node...");
    emit statusChanged();
    m_nodeApi->getTipAsync();
}

void GrinWalletController::syncWallet()
{
    touchWalletSession();
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before running a wallet sync."));
        setLastInfo(QStringLiteral("Wallet sync was skipped because the wallet is locked."));
        return;
    }

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        setLastError(QStringLiteral("Node tip is not available yet. Try sync again after refresh."));
        return;
    }
    requestWalletScan();
}

void GrinWalletController::rescanWallet()
{
    touchWalletSession();
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before starting a full rescan."));
        setLastInfo(QStringLiteral("Full rescan was skipped because the wallet is locked."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastError(QStringLiteral("A wallet scan is already running."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonObject balances;
    balances.insert(QStringLiteral("total"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("spendable"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("locked"), QStringLiteral("0.000000000"));
    balances.insert(QStringLiteral("immature"), QStringLiteral("0.000000000"));
    walletState.insert(QStringLiteral("outputs"), QJsonArray());
    walletState.insert(QStringLiteral("balances"), balances);
    walletState.insert(QStringLiteral("transaction_rescan_backup"),
                       walletState.value(QStringLiteral("transactions")).toArray());
    walletState.insert(QStringLiteral("transactions"), QJsonArray());
    walletState.insert(QStringLiteral("scan_height"), 0);
    walletState.insert(QStringLiteral("restore_leaf_index"), 0);
    walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("full-rescan"));
    walletState.insert(QStringLiteral("last_synced_at"), QString());
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    m_seedScanNextIndex = 1;
    setLastError(QString());
    setLastInfo(QStringLiteral("Full wallet rescan queued from the beginning."));

    if (m_chainHeight == 0) {
        refreshNodeStatus();
        return;
    }

    requestWalletScan();
}

QString GrinWalletController::requestPasteText() const
{
#ifdef Q_OS_WASM
    const char *value = emscripten_run_script_string(
        "(function(){"
        "  var text = window.prompt('Paste text here', '');"
        "  return text === null ? '' : text;"
        "})()");
    return QString::fromUtf8(value ? value : "");
#else
    const QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString();
#endif
}

bool GrinWalletController::copyTextToClipboard(const QString &text) const
{
    if (text.isEmpty()) {
        return false;
    }
#ifdef Q_OS_WASM
    return browserCopyToClipboard(text.toUtf8().constData()) == 1;
#else
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        return false;
    }
    clipboard->setText(text);
    return true;
#endif
}

bool GrinWalletController::downloadTextFile(const QString &suggestedName, const QString &text) const
{
    if (text.isEmpty()) {
        return false;
    }
#ifdef Q_OS_WASM
    const QString fileName = suggestedName.trimmed().isEmpty()
        ? QStringLiteral("grinffindor-wallet-backup.json")
        : suggestedName.trimmed();
    return browserDownloadTextFile(fileName.toUtf8().constData(), text.toUtf8().constData()) == 1;
#else
    Q_UNUSED(suggestedName);
    Q_UNUSED(text);
    return false;
#endif
}

void GrinWalletController::requestPersistentBrowserStorage()
{
#ifdef Q_OS_WASM
    browserRequestPersistentStorage();
    setLastInfo(QStringLiteral("Browser persistent-storage request sent. Re-check the storage status after the browser responds."));
#else
    setLastInfo(QStringLiteral("Persistent browser storage is only relevant for the WASM/browser wallet."));
#endif
    refreshStoragePersistenceState();
}

void GrinWalletController::updateBrowserShortcutContext(const QString &text,
                                                        const QString &selectedText,
                                                        bool focused) const
{
#ifdef Q_OS_WASM
    const QByteArray textUtf8 = text.toUtf8();
    const QByteArray selectedUtf8 = selectedText.toUtf8();
    browserUpdateShortcutContext(textUtf8.constData(), selectedUtf8.constData(), focused ? 1 : 0);
#else
    Q_UNUSED(text)
    Q_UNUSED(selectedText)
    Q_UNUSED(focused)
#endif
}

bool GrinWalletController::isValidNodeUrl(const QString &nodeUrl) const
{
    return isNodeUrlAccepted(nodeUrl);
}

QString GrinWalletController::createSlatepackTemplate(const QString &sender) const
{
    QJsonObject slate;
    slate.insert(QStringLiteral("ver"), QStringLiteral("4:3"));
    slate.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    slate.insert(QStringLiteral("sta"), QStringLiteral("S1"));
    slate.insert(QStringLiteral("amt"), QStringLiteral("0.000000000"));
    slate.insert(QStringLiteral("wallet"), m_walletName);
    slate.insert(QStringLiteral("network"), resolvedNetworkName());
    return encodeSlatepack(QString::fromUtf8(QJsonDocument(slate).toJson(QJsonDocument::Compact)),
                           sender.trimmed());
}

void GrinWalletController::startSendWorkflow(const QString &amount, const QString &note)
{
    touchWalletSession();
    const quint64 requestedAmount = amountToNanogrin(amount);
    if (requestedAmount == 0) {
        setLastError(QStringLiteral("Send amount must be greater than zero."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const WalletSelection::Result selection =
        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, m_chainHeight);
    if (!selection.success) {
        setLastError(selection.error);
        return;
    }

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                outputs[i].locked = true;
                outputs[i].workflowId.clear();
            }
        }
    }

    SlateV4 slate;
    const QString workflowId = generateWorkflowId();
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                outputs[i].workflowId = workflowId;
            }
        }
    }
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    const WalletCryptoBackend::ParticipantContext senderContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    slate.state = SlateV4::Standard1;
    slate.amount = amount.trimmed();
    slate.fee = QStringLiteral("%1.%2")
        .arg(QString::number(selection.fee / 1000000000ULL))
        .arg(QString::number(selection.fee % 1000000000ULL), 9, QLatin1Char('0'));
    slate.offset = WalletCryptoBackend::createOffset(m_seedFingerprint, slate.id);
    slate.signatures.append(WalletCryptoBackend::createParticipantData(senderContext));
    const QString localSlatepackAddress = currentSlatepackAddress();
    const QString localPaymentProofAddress = currentPaymentProofAddress();
    slate.hasPaymentProof = !localPaymentProofAddress.isEmpty();
    if (slate.hasPaymentProof) {
        slate.paymentProof.senderAddress = localPaymentProofAddress;
    }
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("grin-browser-wallet"));
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    slate.metadata.insert(QStringLiteral("wallet"), m_walletName);
    slate.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
    slate.metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    QJsonObject localContext;
    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    QJsonArray selectedCommitments;
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        selectedCommitments.append(selection.selectedOutputs.at(i).commitment);
    }
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    if (selection.change > 0) {
        const QString changeAmount = QStringLiteral("%1.%2")
            .arg(QString::number(selection.change / 1000000000ULL))
            .arg(QString::number(selection.change % 1000000000ULL), 9, QLatin1Char('0'));
        WalletOutput changeOutput;
        SlateV4::Commit changeCommit;
        QString outputError;
        if (buildOwnedOutput(QStringLiteral("change"), changeAmount, &changeOutput, &changeCommit, &outputError)) {
            storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (!outputError.isEmpty()) {
            setLastInfo(QStringLiteral("Change output fallback used: %1").arg(outputError));
        }
    }
    storeWorkflowContext(workflowId, localContext);
    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());
    const QString decoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(
            slate,
            &armoredSlatepack,
            &writerError,
            localSlatepackAddress,
            QStringList(),
            currentSlatepackSecret())) {
        armoredSlatepack = encodeSlatepackArmor(decoded, localSlatepackAddress);
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(slate.workflowId(), slate.modeCode(), slate.stateCode(), armoredSlatepack, decoded);
    setLastInfo(QStringLiteral("SEND workflow started at S1. Share the generated Slatepack with the receiver."));
}

void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    touchWalletSession();
    SlateV4 slate;
    const QString workflowId = generateWorkflowId();
    const WalletCryptoBackend::ParticipantContext receiverContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    slate.state = SlateV4::Invoice1;
    slate.amount = amount.trimmed();
    slate.offset = WalletCryptoBackend::createOffset(m_seedFingerprint, slate.id);
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiverContext));
    WalletOutput invoiceOutput;
    SlateV4::Commit invoiceCommit;
    QString outputError;
    if (buildOwnedOutput(QStringLiteral("invoice"), slate.amount, &invoiceOutput, &invoiceCommit, &outputError)) {
        slate.commitments.append(invoiceCommit);
        storeOwnedOutput(invoiceOutput);
    } else {
        const WalletCryptoBackend::CommitmentResult fallbackCommit =
            WalletCryptoBackend::createCommitment(m_seedFingerprint, slate.id, QStringLiteral("invoice"), slate.amount);
        if (!fallbackCommit.success) {
            setLastError(!outputError.isEmpty()
                ? QStringLiteral("Failed to derive invoice output: %1").arg(outputError)
                : (fallbackCommit.error.isEmpty()
                    ? QStringLiteral("Failed to create invoice commitment.")
                    : fallbackCommit.error));
            return;
        }

        slate.commitments.append(fallbackCommit.commit);
        storeOwnedOutput(QStringLiteral("invoice"), slate.amount, fallbackCommit.commit);
    }
    const QString localSlatepackAddress = currentSlatepackAddress();
    const QString localPaymentProofAddress = currentPaymentProofAddress();
    slate.hasPaymentProof = !localPaymentProofAddress.isEmpty();
    if (slate.hasPaymentProof) {
        slate.paymentProof.receiverAddress = localPaymentProofAddress;
    }
    slate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("grin-browser-wallet"));
    slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    slate.metadata.insert(QStringLiteral("note"), note.trimmed());
    slate.metadata.insert(QStringLiteral("wallet"), m_walletName);
    slate.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
    slate.metadata.insert(QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());
    const QString decoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(
            slate,
            &armoredSlatepack,
            &writerError,
            localSlatepackAddress,
            QStringList(),
            currentSlatepackSecret())) {
        armoredSlatepack = encodeSlatepackArmor(decoded, localSlatepackAddress);
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(slate.workflowId(), slate.modeCode(), slate.stateCode(), armoredSlatepack, decoded);
    setLastInfo(QStringLiteral("RECEIVE workflow started at I1. Share the invoice Slatepack with the sender."));
}

void GrinWalletController::processWorkflowSlatepack(const QString &slatepack)
{
    touchWalletSession();
    QByteArray decryptionKey;
    if (m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            decryptionKey = keychain.slatepackSecretKey();
        }
    }

    const QString decoded = decodeIncomingSlatepack(slatepack, decryptionKey);
    if (decoded.isEmpty()) {
        setLastError(QStringLiteral("Incoming Slatepack could not be decoded."));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(decoded.toUtf8());
    if (!document.isObject()) {
        setLastError(QStringLiteral("Decoded Slatepack is not valid JSON."));
        return;
    }
    if (document.object().value(QStringLiteral("encrypted_slatepack")).toBool()) {
        const QString info = document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Encrypted Slatepack could not be decrypted."));
        setLastError(info);
        setWorkflow(QString(), QString(), QString(), slatepack, QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return;
    }
    if (document.object().value(QStringLiteral("external_slatepack")).toBool()) {
        setLastError(document.object().value(QStringLiteral("note")).toString(
            QStringLiteral("Incoming Slatepack armor was recognized, but payload parsing failed.")));
        setWorkflow(QString(), QString(), QString(), slatepack, QString::fromUtf8(document.toJson(QJsonDocument::Indented)));
        return;
    }

    SlateV4 slate = SlateV4::fromJson(document.object());
    if (slate.workflowId().isEmpty() && !slate.id.isEmpty() && slate.state != SlateV4::Unknown) {
        slate.metadata.insert(QStringLiteral("workflow_id"), slate.id);
        slate.metadata.insert(QStringLiteral("workflow"),
                              slate.metadata.value(QStringLiteral("external_binary")).toBool()
                                  ? QStringLiteral("external-grin-slatepack")
                                  : QStringLiteral("imported-slatepack"));
    }
    if (slate.network().trimmed().isEmpty()) {
        slate.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
    }
    if (slate.network().trimmed().toLower() != resolvedNetworkName()) {
        setLastError(QStringLiteral("Incoming Slatepack targets %1, but the wallet is currently set to %2.")
                         .arg(slate.network().trimmed(), resolvedNetworkName()));
        return;
    }

    const QString workflowId = slate.workflowId();
    const QString mode = slate.modeCode();
    const QString state = slate.stateCode();
    if (workflowId.isEmpty() || mode == QStringLiteral("unknown") || state == QStringLiteral("NA")) {
        setLastError(QStringLiteral("Incoming Slatepack is missing workflow metadata."));
        return;
    }

    if (slate.isFinalState()) {
        setWorkflow(workflowId, mode, state, slatepack, QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented)));
        setLastInfo(QStringLiteral("Workflow %1 is already complete at %2.").arg(workflowId, state));
        return;
    }

    const QString localRoleTag =
        (state == QStringLiteral("S1")) ? QStringLiteral("receiver")
      : (state == QStringLiteral("S2")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I1")) ? QStringLiteral("sender")
      : (state == QStringLiteral("I2")) ? QStringLiteral("receiver")
      : QString();

    if (localRoleTag.isEmpty()) {
        setLastError(QStringLiteral("Unsupported workflow transition."));
        return;
    }

    const QString localSlatepackAddress = currentSlatepackAddress();
    const QString localPaymentProofAddress = currentPaymentProofAddress();
    if (!localPaymentProofAddress.isEmpty() && slate.hasPaymentProof) {
        if (mode == QStringLiteral("send")) {
            if (slate.paymentProof.senderAddress.trimmed().isEmpty()) {
                slate.paymentProof.senderAddress =
                    (localRoleTag == QStringLiteral("sender")) ? localPaymentProofAddress : slate.paymentProof.senderAddress;
            }
            if (slate.paymentProof.receiverAddress.trimmed().isEmpty()) {
                slate.paymentProof.receiverAddress =
                    (localRoleTag == QStringLiteral("receiver")) ? localPaymentProofAddress : slate.paymentProof.receiverAddress;
            }
        } else if (mode == QStringLiteral("invoice")) {
            if (slate.paymentProof.senderAddress.trimmed().isEmpty()) {
                slate.paymentProof.senderAddress =
                    (localRoleTag == QStringLiteral("sender")) ? localPaymentProofAddress : slate.paymentProof.senderAddress;
            }
            if (slate.paymentProof.receiverAddress.trimmed().isEmpty()) {
                slate.paymentProof.receiverAddress =
                    (localRoleTag == QStringLiteral("receiver")) ? localPaymentProofAddress : slate.paymentProof.receiverAddress;
            }
        }
    }

    QString cryptoError;
    if (localRoleTag == QStringLiteral("sender")) {
        QString selectedFee;
        if (!ensureWorkflowSelectionContext(workflowId, slate.amount, &selectedFee, &cryptoError)) {
            setLastError(cryptoError.isEmpty()
                ? QStringLiteral("Failed to select sender outputs for workflow funding.")
                : cryptoError);
            return;
        }
        if (!selectedFee.isEmpty()) {
            slate.fee = selectedFee;
        }
    }

    if (slate.metadata.value(QStringLiteral("external_binary")).toBool()
        && state == QStringLiteral("S1")
        && localRoleTag == QStringLiteral("receiver")) {
        const QString receiverOffset = WalletCryptoBackend::createOffset(
            m_seedFingerprint, slate.workflowId() + QStringLiteral(":receiver"));
        const QString adjustedOffset = WalletCryptoBackend::addOffsets(slate.offset, receiverOffset, &cryptoError);
        if (adjustedOffset.isEmpty()) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to adjust receiver offset.") : cryptoError);
            return;
        }
        slate.offset = adjustedOffset;
        slate.metadata.insert(QStringLiteral("receiver_offset"), receiverOffset);
    }

    if (slate.metadata.value(QStringLiteral("external_binary")).toBool()
        && state == QStringLiteral("I1")
        && localRoleTag == QStringLiteral("sender")) {
        const QJsonObject localContext = workflowContext(workflowId);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
        const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
        QStringList positiveBlinds;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const WalletOutput input = findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
            if (!input.blindingFactor.isEmpty()) {
                positiveBlinds.append(input.blindingFactor);
            }
        }

        QStringList negativeBlinds;
        const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
        if (!changeCommit.isEmpty()) {
            const WalletOutput changeOutput = findTrackedOutputByCommitment(trackedOutputs, changeCommit);
            if (!changeOutput.blindingFactor.isEmpty()) {
                negativeBlinds.append(changeOutput.blindingFactor);
            }
        }

        const QString senderBlind = WalletCryptoBackend::combineBlindingFactors(
            positiveBlinds,
            negativeBlinds,
            &cryptoError);
        if (senderBlind.isEmpty()) {
            setLastError(cryptoError.isEmpty()
                ? QStringLiteral("Failed to derive sender excess for invoice response.")
                : cryptoError);
            return;
        }

        const QString senderOffset = WalletCryptoBackend::createOffset(
            m_seedFingerprint, slate.workflowId() + QStringLiteral(":sender"));
        const QString adjustedOffset = WalletCryptoBackend::addOffsets(slate.offset, senderOffset, &cryptoError);
        if (adjustedOffset.isEmpty()) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to adjust sender offset.") : cryptoError);
            return;
        }

        slate.offset = adjustedOffset;
        slate.metadata.insert(QStringLiteral("sender_blind"), senderBlind);
        slate.metadata.insert(QStringLiteral("sender_offset"), senderOffset);
    }

    if (localRoleTag == QStringLiteral("receiver") && slate.commitments.isEmpty()) {
        WalletOutput receiveOutput;
        SlateV4::Commit receiveCommit;
        const QString receiverSource =
            (mode == QStringLiteral("invoice")) ? QStringLiteral("invoice") : QStringLiteral("receive");
        if (!ensureReceiverOutputContext(
                workflowId,
                slate.amount,
                receiverSource,
                &receiveOutput,
                &receiveCommit,
                &cryptoError)) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to derive receiver output.") : cryptoError);
            return;
        }

        slate.commitments.append(receiveCommit);
        slate.metadata.insert(QStringLiteral("receiver_blind"), receiveOutput.blindingFactor);
        slate.metadata.insert(QStringLiteral("receiver_child_index"), static_cast<int>(receiveOutput.childIndex));
        slate.metadata.insert(QStringLiteral("receiver_key_path"), receiveOutput.keyPath);
    }

    if (!WalletCryptoBackend::applyRound2Signature(&slate, m_seedFingerprint, localRoleTag, &cryptoError)) {
        setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to apply round 2 signature.") : cryptoError);
        return;
    }

    if (localRoleTag == QStringLiteral("receiver")
        && m_walletUnlocked
        && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()
            && !WalletCryptoBackend::signPaymentProof(&slate, keychain, &cryptoError)
            && slate.hasPaymentProof) {
            setLastInfo(cryptoError);
        }
    }

    slate.advanceState();
    const QString nextState = slate.stateCode();
    const bool externalBinary = slate.metadata.value(QStringLiteral("external_binary")).toBool();
    slate.metadata.insert(QStringLiteral("processed_by"), m_walletName);
    slate.metadata.insert(QStringLiteral("processed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if ((nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3"))
        && !WalletCryptoBackend::finalizeSlate(&slate, &cryptoError)) {
        setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to finalize slate signature.") : cryptoError);
        return;
    }

    if (slate.hasPaymentProof && !slate.paymentProof.receiverSignature.isEmpty()) {
        if (WalletCryptoBackend::verifyPaymentProof(slate, &cryptoError)) {
            slate.metadata.insert(QStringLiteral("payment_proof_valid"), true);
            slate.metadata.insert(QStringLiteral("payment_proof_status"), QStringLiteral("verified"));
        } else {
            slate.metadata.insert(QStringLiteral("payment_proof_valid"), false);
            slate.metadata.insert(QStringLiteral("payment_proof_status"), QStringLiteral("invalid"));
            slate.metadata.insert(QStringLiteral("payment_proof_error"), cryptoError);
        }
    }

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        const QJsonObject localContext = workflowContext(workflowId);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        if (!selectedCommitments.isEmpty()) {
            const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
            const QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
            QList<WalletOutput> selectedInputs;
            for (int i = 0; i < selectedCommitments.size(); ++i) {
                selectedInputs.append(findTrackedOutputByCommitment(
                    trackedOutputs, selectedCommitments.at(i).toString()));
            }

            WalletOutput receiverOutput;
            if (!slate.commitments.isEmpty()) {
                receiverOutput.commitment = slate.commitments.first().commitment;
                receiverOutput.proof = slate.commitments.first().proof;
                receiverOutput.amount = slate.amount;
            }

            WalletOutput changeOutput;
            const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
            if (!changeCommit.isEmpty()) {
                changeOutput = findTrackedOutputByCommitment(trackedOutputs, changeCommit);
                if (changeOutput.commitment.isEmpty()) {
                    changeOutput.commitment = changeCommit;
                    changeOutput.proof = localContext.value(QStringLiteral("change_proof")).toString();
                    changeOutput.amount = localContext.value(QStringLiteral("change_amount_display")).toString();
                }
            }

            const WalletTxBuilder::BuildResult txBuild = WalletTxBuilder::buildTransactionSkeleton(
                slate,
                selectedInputs,
                receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput,
                changeOutput.commitment.isEmpty() ? 0 : &changeOutput);
            if (txBuild.success) {
                slate.metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                slate.metadata.insert(QStringLiteral("tx_ready"), true);
                finalizeWorkflowOutputs(slate, false);
            } else {
                slate.metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
            }
        }
    }

    const QString updatedDecoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    QString updatedSlatepack;
    const QStringList outgoingRecipients = outgoingSlatepackRecipients(slate);
    const QString outgoingSender =
        (externalBinary && outgoingRecipients.isEmpty()) ? QString() : localSlatepackAddress;
    if (!BinarySlateV4Writer::encodeSlatepack(
            slate,
            &updatedSlatepack,
            &cryptoError,
            outgoingSender,
            outgoingRecipients,
            currentSlatepackSecret())) {
        if (externalBinary) {
            setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to encode binary Slatepack.") : cryptoError);
            return;
        }
        updatedSlatepack = encodeSlatepackArmor(updatedDecoded, localSlatepackAddress);
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(workflowId, mode, nextState, updatedSlatepack, updatedDecoded);

    if (nextState == QStringLiteral("S3") || nextState == QStringLiteral("I3")) {
        setLastInfo(QStringLiteral("Workflow %1 advanced to %2 and reached the final exchange step.").arg(workflowId, nextState));
    } else {
        setLastInfo(QStringLiteral("Workflow %1 advanced from %2 to %3.").arg(workflowId, state, nextState));
    }
}

void GrinWalletController::clearWorkflow()
{
    setWorkflow(QString(), QString(), QString(), QString(), QString());
    setLastInfo(QStringLiteral("Workflow state cleared."));
}

void GrinWalletController::broadcastCurrentWorkflowTransaction()
{
    touchWalletSession();
    const QJsonDocument workflowDoc = QJsonDocument::fromJson(m_workflowDecoded.toUtf8());
    if (!workflowDoc.isObject()) {
        setLastError(QStringLiteral("Current workflow does not contain decodable transaction data."));
        return;
    }

    const QJsonObject txSkeleton = workflowDoc.object().value(QStringLiteral("tx_skeleton")).toObject();
    if (txSkeleton.isEmpty()) {
        setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
        return;
    }

    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }
    if (!m_pendingBroadcastWorkflowId.isEmpty()) {
        setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
        return;
    }

    const SlateV4 slate = SlateV4::fromJson(workflowDoc.object());
    persistWorkflowTransaction(slate, false);
    updateTransactionEntry(slate.workflowId(), [](QJsonObject &entry) {
        entry.insert(QStringLiteral("status"), QStringLiteral("broadcast_pending"));
        entry.insert(QStringLiteral("broadcasted"), false);
        entry.insert(QStringLiteral("last_broadcast_attempt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        entry.insert(QStringLiteral("broadcast_attempts"), entry.value(QStringLiteral("broadcast_attempts")).toInt() + 1);
        entry.remove(QStringLiteral("broadcast_error"));
    });
    m_pendingBroadcastWorkflowId = slate.workflowId();
    m_nodeApi->pushTransactionAsync(Transaction::fromJson(txSkeleton), true);
}

void GrinWalletController::broadcastTransaction(const QString &workflowId)
{
    touchWalletSession();
    const QJsonArray transactions = loadDocument()
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
            setLastError(QStringLiteral("No transaction skeleton is available for broadcast."));
            return;
        }
        if (!m_nodeApi) {
            connectNodeClient();
        }
        if (!m_nodeApi) {
            setLastError(QStringLiteral("Node client is not configured."));
            return;
        }
        if (!m_pendingBroadcastWorkflowId.isEmpty()) {
            setLastError(QStringLiteral("Another transaction broadcast is already in progress."));
            return;
        }

        SlateV4 slate;
        slate.id = tx.value(QStringLiteral("slate_id")).toString();
        slate.amount = tx.value(QStringLiteral("amount")).toString();
        slate.fee = tx.value(QStringLiteral("fee")).toString();
        slate.offset = tx.value(QStringLiteral("offset")).toString();
        slate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
        slate.setStateFromCode(tx.value(QStringLiteral("state")).toString());
        persistWorkflowTransaction(slate, false);
        updateTransactionEntry(workflowId, [](QJsonObject &entry) {
            entry.insert(QStringLiteral("status"), QStringLiteral("broadcast_pending"));
            entry.insert(QStringLiteral("broadcasted"), false);
            entry.insert(QStringLiteral("last_broadcast_attempt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            entry.insert(QStringLiteral("broadcast_attempts"), entry.value(QStringLiteral("broadcast_attempts")).toInt() + 1);
            entry.remove(QStringLiteral("broadcast_error"));
        });
        m_pendingBroadcastWorkflowId = workflowId;
        m_nodeApi->pushTransactionAsync(Transaction::fromJson(txSkeleton), true);
        return;
    }

    setLastError(QStringLiteral("Transaction not found in wallet history."));
}

void GrinWalletController::cancelTransaction(const QString &workflowId)
{
    touchWalletSession();
    if (workflowId.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Workflow id is required for cancel."));
        return;
    }

    const QJsonArray existingTransactions = loadDocument()
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
            setLastError(QStringLiteral("Confirmed transactions can no longer be cancelled."));
            return;
        }
        if (tx.value(QStringLiteral("broadcasted")).toBool()
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("broadcasted")
            || tx.value(QStringLiteral("status")).toString() == QStringLiteral("in_mempool")) {
            setLastError(QStringLiteral("Broadcasted transactions can no longer be cancelled locally."));
            return;
        }
        break;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (outputs[i].spent) {
            continue;
        }

        if (outputs[i].source == QStringLiteral("change")
            || outputs[i].source == QStringLiteral("receive")
            || outputs[i].source == QStringLiteral("invoice")) {
            outputs.removeAt(i);
            --i;
            continue;
        }

        outputs[i].locked = false;
        outputs[i].pending = false;
        outputs[i].workflowId.clear();
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
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.remove(workflowId);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    saveDocument(document);
    refreshStateFromStorage();

    if (m_workflowId == workflowId) {
        clearWorkflow();
    }
    setLastError(QString());
    setLastInfo(QStringLiteral("Transaction %1 cancelled and locks released.").arg(workflowId));
}

QString GrinWalletController::encodeSlatepack(const QString &slateJson, const QString &sender) const
{
    const QString trimmed = slateJson.trimmed();
    const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8());
    if (document.isObject()) {
        const SlateV4 slate = SlateV4::fromJson(document.object());
        if (slate.state != SlateV4::Unknown && !slate.id.trimmed().isEmpty()) {
            QString binarySlatepack;
            QString writerError;
            if (BinarySlateV4Writer::encodeSlatepack(
                    slate,
                    &binarySlatepack,
                    &writerError,
                    sender.trimmed(),
                    QStringList(),
                    currentSlatepackSecret())) {
                return binarySlatepack;
            }
        }
    }

    return encodeSlatepackArmor(trimmed, sender.trimmed());
}

QString GrinWalletController::decodeSlatepack(const QString &slatepack) const
{
    QByteArray decryptionKey;
    if (m_walletUnlocked && !m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            decryptionKey = keychain.slatepackSecretKey();
        }
    }
    return decodeIncomingSlatepack(slatepack, decryptionKey);
}

QString GrinWalletController::currentSlatepackAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::slatepackAddress(keychain) : QString();
}

QString GrinWalletController::currentPaymentProofAddress() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QString();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? WalletCryptoBackend::paymentProofAddress(keychain) : QString();
}

QByteArray GrinWalletController::currentSlatepackSecret() const
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        return QByteArray();
    }

    const WalletKeychain keychain(m_sessionMnemonic);
    return keychain.isValid() ? keychain.slatepackSecretKey() : QByteArray();
}

QStringList GrinWalletController::outgoingSlatepackRecipients(const SlateV4 &slate) const
{
    QStringList recipients;
    const QString localAddress = currentSlatepackAddress();
    const QString sender = slate.metadata.value(QStringLiteral("slatepack_sender")).toString().trimmed();
    if (!sender.isEmpty() && sender != localAddress) {
        recipients.append(sender);
    }

    const QJsonArray explicitRecipients = slate.metadata.value(QStringLiteral("slatepack_recipients")).toArray();
    for (int i = 0; i < explicitRecipients.size(); ++i) {
        const QString recipient = explicitRecipients.at(i).toString().trimmed();
        if (!recipient.isEmpty() && recipient != localAddress && !recipients.contains(recipient)) {
            recipients.append(recipient);
        }
    }

    return recipients;
}

void GrinWalletController::loadFromStorage()
{
    const QJsonObject document = loadDocument();
    const QString activeNetwork =
        inferNetworkName(document.value(QStringLiteral("node")).toObject().value(QStringLiteral("network")).toString(),
                         document.value(QStringLiteral("node")).toObject().value(QStringLiteral("url")).toString());
    const QJsonObject wallet = walletForNetwork(document, activeNetwork);

    m_walletExists = !wallet.isEmpty();
    m_walletUnlocked = false;
    m_walletName = wallet.value(QStringLiteral("name")).toString();
    m_sessionMnemonic.clear();
    m_seedFingerprint = wallet.value(QStringLiteral("seed_fingerprint")).toString();
    m_mnemonicPreview.clear();
    const QJsonObject node = document.value(QStringLiteral("node")).toObject();
    const QString storedNodeUrl = node.value(QStringLiteral("url")).toString();
    m_selectedNetwork = activeNetwork;
    m_nodeUrl = isNodeUrlAccepted(storedNodeUrl)
        ? storedNodeUrl
        : defaultNodeUrlForNetwork(m_selectedNetwork);

    emit walletChanged();
    emit nodeConfigChanged();
    refreshStateFromStorage();
}

QString GrinWalletController::resolvedNetworkName() const
{
    return isAcceptedNetworkName(m_selectedNetwork) ? m_selectedNetwork : defaultNetworkName();
}

void GrinWalletController::setLastError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
}

void GrinWalletController::setLastInfo(const QString &info)
{
    m_lastInfo = info;
    emit lastInfoChanged();
}

void GrinWalletController::setWorkflow(const QString &id, const QString &mode, const QString &state, const QString &slatepack, const QString &decoded)
{
    m_workflowId = id;
    m_workflowMode = mode;
    m_workflowState = state;
    m_workflowSlatepack = slatepack;
    m_workflowDecoded = decoded;
    emit workflowChanged();
}

void GrinWalletController::connectNodeClient()
{
    if (m_nodeApi) {
        m_nodeApi->deleteLater();
        m_nodeApi = 0;
    }
    if (m_nodeUrl.trimmed().isEmpty()) {
        return;
    }

    m_nodeApi = new NodeForeignApi(m_nodeUrl, QString());
    m_nodeApi->setParent(this);

    connect(m_nodeApi, &NodeForeignApi::getTipFinished, this, [this](const Result<Tip> &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Node query failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            return;
        }

        const Tip tip = result.value();
        m_chainHeight = tip.height();
        m_syncStatus = QStringLiteral("Connected to external node");
        refreshTransactionConfirmations();
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QStringLiteral("Node tip updated to height %1.").arg(QString::number(m_chainHeight)));

        if (m_walletUnlocked
            && !m_sessionMnemonic.trimmed().isEmpty()
            && !m_walletScanInFlight
            && !m_seedScanActive) {
            if (m_scanHeight == 0) {
                rescanWallet();
            } else {
                requestWalletScan();
            }
        }

        refreshBroadcastStatuses();
        recoverPendingBroadcasts();
    });

    connect(m_nodeApi, &NodeForeignApi::getOutputsFinished, this, [this](const Result<QList<OutputPrintable> > &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Wallet scan failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            m_walletScanInFlight = false;
            return;
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
        tracked = WalletScanner::reconcileTrackedOutputs(tracked, result.value());

        walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_chainHeight));
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("tracked-outputs"));
        walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        document.insert(QStringLiteral("wallet_state"), walletState);

        if (!saveDocument(document)) {
            setLastError(QStringLiteral("Failed to persist wallet scan results."));
            return;
        }

        refreshStateFromStorage();
        m_syncStatus = QStringLiteral("Wallet outputs synced");
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QStringLiteral("Wallet scan updated %1 tracked outputs from node data.")
                        .arg(QString::number(tracked.size())));
        m_walletScanInFlight = false;
    });

    connect(m_nodeApi, &NodeForeignApi::getUnspentOutputsFinished, this, [this](const Result<OutputListing> &result) {
        if (result.hasError()) {
            m_syncStatus = QStringLiteral("Seed scan failed");
            emit statusChanged();
            setLastError(result.errorMessage());
            m_seedScanActive = false;
            m_walletScanInFlight = false;
            return;
        }

        if (!m_seedScanActive || !m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
            return;
        }

        WalletKeychain keychain(m_sessionMnemonic);
        if (!keychain.isValid()) {
            setLastError(QStringLiteral("Wallet keychain could not be derived for seed scan."));
            m_seedScanActive = false;
            m_walletScanInFlight = false;
            return;
        }

        const QList<WalletOutput> discovered =
            WalletScanner::discoverOwnedOutputs(result.value().outputs(), keychain);
        for (int i = 0; i < discovered.size(); ++i) {
            bool exists = false;
            for (int j = 0; j < m_seedScanDiscovered.size(); ++j) {
                if (m_seedScanDiscovered.at(j).commitment == discovered.at(i).commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                m_seedScanDiscovered.append(discovered.at(i));
            }
        }

        const OutputListing listing = result.value();
        if (listing.lastRetrievedIndex() > 0 && listing.highestIndex() > 0
            && listing.lastRetrievedIndex() < listing.highestIndex()) {
            m_seedScanNextIndex = listing.lastRetrievedIndex() + 1;
            QJsonObject document = loadDocument();
            QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
            walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(listing.lastRetrievedIndex()));
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            m_syncStatus = QStringLiteral("Seed scan page %1 / %2")
                               .arg(QString::number(listing.lastRetrievedIndex()))
                               .arg(QString::number(listing.highestIndex()));
            emit statusChanged();
            m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
            return;
        }
        if (listing.lastRetrievedIndex() == 0 && discovered.size() >= 1000) {
            m_seedScanNextIndex += 1000;
            QJsonObject document = loadDocument();
            QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
            walletState.insert(QStringLiteral("restore_leaf_index"), QString::number(m_seedScanNextIndex - 1));
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            m_syncStatus = QStringLiteral("Seed scan page starting at %1")
                               .arg(QString::number(m_seedScanNextIndex));
            emit statusChanged();
            m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
            return;
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QList<WalletOutput> tracked = WalletScanner::outputsFromState(walletState);
        for (int i = 0; i < m_seedScanDiscovered.size(); ++i) {
            bool exists = false;
            for (int j = 0; j < tracked.size(); ++j) {
                if (tracked.at(j).commitment == m_seedScanDiscovered.at(i).commitment) {
                    WalletOutput merged = tracked.at(j);
                    const WalletOutput discovered = m_seedScanDiscovered.at(i);
                    merged.proof = discovered.proof;
                    merged.amount = discovered.amount;
                    merged.keyPath = discovered.keyPath;
                    merged.blindingFactor = discovered.blindingFactor;
                    merged.childIndex = discovered.childIndex;
                    merged.height = discovered.height;
                    merged.coinbase = discovered.coinbase;
                    merged.onChain = discovered.onChain;
                    merged.spent = discovered.spent;
                    if (discovered.onChain && !discovered.spent) {
                        merged.pending = false;
                        merged.locked = false;
                    }
                    tracked[j] = merged;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                tracked.append(m_seedScanDiscovered.at(i));
            }
        }

        quint32 nextChildIndex = nextChildIndexFromState(walletState);
        for (int i = 0; i < tracked.size(); ++i) {
            if (tracked.at(i).childIndex + 1 > nextChildIndex) {
                nextChildIndex = tracked.at(i).childIndex + 1;
            }
        }

        walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(tracked));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(tracked, m_chainHeight));
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        walletState.insert(QStringLiteral("restore_leaf_index"),
                           QString::number(listing.lastRetrievedIndex() > 0
                                               ? listing.lastRetrievedIndex()
                                               : (m_seedScanNextIndex > 0 ? m_seedScanNextIndex - 1 : 1)));
        walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(nextChildIndex));
        const QString previousSyncMode = walletState.value(QStringLiteral("last_sync_mode")).toString();
        const bool rebuildingTransactions = previousSyncMode == QStringLiteral("full-rescan");
        if (rebuildingTransactions) {
            const QJsonArray rebuiltTransactions =
                rebuildTransactionHistoryFromOutputs(
                    tracked,
                    walletState.value(QStringLiteral("transaction_rescan_backup")).toArray());
            walletState.insert(QStringLiteral("transactions"), rebuiltTransactions);
            document.insert(QStringLiteral("workflow_contexts"),
                            filterWorkflowContextsForTransactions(
                                document.value(QStringLiteral("workflow_contexts")).toObject(),
                                rebuiltTransactions));
        }
        walletState.remove(QStringLiteral("transaction_rescan_backup"));
        walletState.insert(QStringLiteral("last_sync_mode"), QStringLiteral("seed-rewind"));
        walletState.insert(QStringLiteral("last_synced_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        document.insert(QStringLiteral("wallet_state"), walletState);

        if (!saveDocument(document)) {
            setLastError(QStringLiteral("Failed to persist seed scan results."));
            return;
        }

        refreshStateFromStorage();
        m_syncStatus = QStringLiteral("Seed scan complete");
        emit statusChanged();
        setLastError(QString());
        setLastInfo(QString());
        m_seedScanActive = false;
        m_walletScanInFlight = false;
    });

    connect(m_nodeApi, &NodeForeignApi::getUnconfirmedTransactionsFinished, this, [this](const Result<QList<PoolEntry> > &result) {
        m_broadcastStatusRefreshInFlight = false;
        if (result.hasError()) {
            return;
        }

        QSet<QString> mempoolExcesses;
        const QList<PoolEntry> entries = result.value();
        for (int i = 0; i < entries.size(); ++i) {
            const QVector<TxKernel> kernels = entries.at(i).tx().body().kernels();
            for (int j = 0; j < kernels.size(); ++j) {
                if (!kernels.at(j).excess().isEmpty()) {
                    mempoolExcesses.insert(kernels.at(j).excess());
                }
            }
        }

        QJsonObject document = loadDocument();
        QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
        QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
        m_kernelStatusQueue.clear();

        for (int i = 0; i < transactions.size(); ++i) {
            QJsonObject entry = transactions.at(i).toObject();
            const QString status = entry.value(QStringLiteral("status")).toString();
            if (!entry.value(QStringLiteral("broadcasted")).toBool()
                && status != QStringLiteral("broadcast_pending")) {
                continue;
            }
            if (status == QStringLiteral("confirmed") || status == QStringLiteral("cancelled")) {
                continue;
            }

            const QString excess = kernelExcessFromEntry(entry);
            if (excess.isEmpty()) {
                continue;
            }

            entry.insert(QStringLiteral("kernel_excess"), excess);
            entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            entry.insert(QStringLiteral("broadcasted"),
                         entry.value(QStringLiteral("broadcasted")).toBool()
                             || mempoolExcesses.contains(excess));
            entry.insert(QStringLiteral("status"),
                         mempoolExcesses.contains(excess)
                             ? QStringLiteral("in_mempool")
                             : (status == QStringLiteral("broadcast_pending")
                                   ? QStringLiteral("broadcast_pending")
                                   : QStringLiteral("broadcasted")));
            entry.insert(QStringLiteral("confirmations"), 0);
            transactions.replace(i, entry);
            m_kernelStatusQueue.append(qMakePair(entry.value(QStringLiteral("workflow_id")).toString(), excess));
        }

        walletState.insert(QStringLiteral("transactions"), transactions);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
        refreshStateFromStorage();
        startNextKernelStatusCheck();
    });

    connect(m_nodeApi, &NodeForeignApi::getKernelFinished, this, [this](const Result<LocatedTxKernel> &result) {
        m_kernelStatusCheckInFlight = false;
        if (!m_currentKernelWorkflowId.isEmpty() && !result.hasError()) {
            updateTransactionEntry(m_currentKernelWorkflowId, [this, &result](QJsonObject &entry) {
                entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
                entry.insert(QStringLiteral("broadcasted"), true);
                entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(result.value().height()));
                entry.insert(QStringLiteral("confirmations"),
                             static_cast<qint64>(m_chainHeight >= result.value().height()
                                 ? (m_chainHeight - result.value().height() + 1)
                                 : 0));
                entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
            });
            finalizeBroadcastedWorkflow(m_currentKernelWorkflowId);
        }

        m_currentKernelWorkflowId.clear();
        m_currentKernelExcess.clear();
        startNextKernelStatusCheck();
    });

    connect(m_nodeApi, &NodeForeignApi::pushTransactionFinished, this, [this](const Result<bool> &result) {
        const QString workflowId = m_pendingBroadcastWorkflowId;
        m_pendingBroadcastWorkflowId.clear();

        if (result.hasError() || !result.value()) {
            if (!workflowId.isEmpty()) {
                updateTransactionEntry(workflowId, [&result](QJsonObject &entry) {
                    entry.insert(QStringLiteral("status"), QStringLiteral("broadcast_failed"));
                    entry.insert(QStringLiteral("broadcasted"), false);
                    entry.insert(QStringLiteral("broadcast_error"),
                                 result.hasError()
                                     ? result.errorMessage()
                                     : QStringLiteral("Node rejected transaction broadcast."));
                    entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                });
            }
            setLastError(result.hasError()
                             ? result.errorMessage()
                             : QStringLiteral("Node rejected transaction broadcast."));
            return;
        }

        if (!workflowId.isEmpty()) {
            updateTransactionEntry(workflowId, [](QJsonObject &entry) {
                entry.insert(QStringLiteral("status"), QStringLiteral("broadcasted"));
                entry.insert(QStringLiteral("broadcasted"), true);
                entry.insert(QStringLiteral("broadcast_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                entry.insert(QStringLiteral("last_node_check"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                entry.remove(QStringLiteral("broadcast_error"));
            });
            finalizeBroadcastedWorkflow(workflowId);
        }

        setLastError(QString());
        setLastInfo(QStringLiteral("Transaction broadcast submitted to node."));
        refreshBroadcastStatuses();
    });
}

void GrinWalletController::refreshStateFromStorage()
{
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QJsonObject balances = walletState.value(QStringLiteral("balances")).toObject();

    m_totalBalance = amountStringFromJson(balances, QStringLiteral("total"));
    m_spendableBalance = amountStringFromJson(balances, QStringLiteral("spendable"));
    m_lockedBalance = amountStringFromJson(balances, QStringLiteral("locked"));
    m_immatureBalance = amountStringFromJson(balances, QStringLiteral("immature"));
    m_scanHeight = static_cast<qulonglong>(walletState.value(QStringLiteral("scan_height")).toInt());
    emit statusChanged();
}

void GrinWalletController::startAutoRefresh()
{
    if (m_autoRefreshTimer) {
        return;
    }

    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(30000);
    m_autoRefreshTimer->setSingleShot(false);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &GrinWalletController::refreshNodeStatus);
    m_autoRefreshTimer->start();

    if (!m_sessionLockTimer) {
        m_sessionLockTimer = new QTimer(this);
        m_sessionLockTimer->setInterval(kSessionAutoLockIntervalMs);
        m_sessionLockTimer->setSingleShot(true);
        connect(m_sessionLockTimer, &QTimer::timeout, this, [this]() {
            if (!m_walletUnlocked) {
                return;
            }
            lockWallet();
            setLastInfo(QStringLiteral("Wallet locked after inactivity."));
        });
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (!m_walletUnlocked) {
            return;
        }
        if (state == Qt::ApplicationHidden || state == Qt::ApplicationInactive) {
            lockWallet();
            setLastInfo(QStringLiteral("Wallet locked because the browser tab or app became inactive."));
        }
    });
}

void GrinWalletController::storeOwnedOutput(const QString &source, const QString &amount, const SlateV4::Commit &commit)
{
    if (commit.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == commit.commitment) {
            return;
        }
    }

    WalletOutput output;
    output.commitment = commit.commitment;
    output.proof = commit.proof;
    output.amount = amount;
    output.source = source;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    storeOwnedOutput(output);
}

void GrinWalletController::storeOwnedOutput(const WalletOutput &output)
{
    if (output.commitment.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs.at(i).commitment == output.commitment) {
            outputs[i] = output;
            walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
            walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
            if (output.childIndex + 1 > nextChildIndexFromState(walletState)) {
                walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
            }
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            refreshStateFromStorage();
            return;
        }
    }

    outputs.append(output);
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    if (output.childIndex + 1 > nextChildIndexFromState(walletState)) {
        walletState.insert(QStringLiteral("next_child_index"), static_cast<int>(output.childIndex + 1));
    }
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

bool GrinWalletController::buildOwnedOutput(const QString &source,
                                            const QString &amount,
                                            WalletOutput *outputOut,
                                            SlateV4::Commit *commitOut,
                                            QString *errorOut) const
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Output target is missing.");
        }
        return false;
    }
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet must be unlocked to derive owned outputs.");
        }
        return false;
    }

    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const quint32 childIndex = nextChildIndexFromState(walletState);
    const WalletKeychain keychain(m_sessionMnemonic);
    if (!keychain.isValid()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet keychain could not be derived.");
        }
        return false;
    }

    const WalletCryptoBackend::OwnedCommitment owned =
        WalletCryptoBackend::createOwnedCommitment(keychain, childIndex, amount);
    if (!owned.success) {
        if (errorOut) {
            *errorOut = QStringLiteral("Wallet output derivation failed.");
        }
        return false;
    }

    WalletOutput output;
    output.commitment = owned.commit.commitment;
    output.proof = owned.commit.proof;
    output.amount = amount;
    output.source = source;
    output.keyPath = owned.keyPath;
    output.blindingFactor = owned.blindingFactor;
    output.childIndex = owned.childIndex;
    output.locked = false;
    output.spent = false;
    output.onChain = false;
    output.pending = false;

    *outputOut = output;
    *commitOut = owned.commit;
    return true;
}

bool GrinWalletController::ensureWorkflowSelectionContext(const QString &workflowId,
                                                          const QString &amount,
                                                          QString *feeOut,
                                                          QString *errorOut)
{
    if (workflowId.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow id is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    if (!localContext.value(QStringLiteral("selected_input_commits")).toArray().isEmpty()) {
        const quint64 persistedFee = localContext.value(QStringLiteral("fee_nano")).toVariant().toULongLong();
        if (feeOut) {
            *feeOut = persistedFee > 0
                ? formatNanogrin(persistedFee)
                : localContext.value(QStringLiteral("fee_amount_display")).toString();
        }
        return true;
    }

    const quint64 requestedAmount = amountToNanogrin(amount);
    if (requestedAmount == 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Workflow amount must be greater than zero.");
        }
        return false;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const WalletSelection::Result selection =
        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, m_chainHeight);
    if (!selection.success) {
        if (errorOut) {
            *errorOut = selection.error;
        }
        return false;
    }

    QJsonArray selectedCommitments;
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs.at(i).commitment != selection.selectedOutputs.at(j).commitment) {
                continue;
            }

            outputs[i].locked = true;
            outputs[i].pending = false;
            outputs[i].workflowId = workflowId;
            selectedCommitments.append(outputs.at(i).commitment);
            break;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    if (!saveDocument(document)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to persist sender selection context.");
        }
        return false;
    }

    localContext.insert(QStringLiteral("selected_inputs"), selection.selectedOutputs.size());
    localContext.insert(QStringLiteral("selected_total"), QString::number(selection.totalSelected));
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    localContext.insert(QStringLiteral("amount_nano"), QString::number(requestedAmount));
    localContext.insert(QStringLiteral("amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("fee_nano"), QString::number(selection.fee));
    localContext.insert(QStringLiteral("fee_amount_display"), formatNanogrin(selection.fee));

    if (selection.change > 0 && localContext.value(QStringLiteral("change_commit")).toString().isEmpty()) {
        const QString changeAmount = formatNanogrin(selection.change);
        WalletOutput changeOutput;
        SlateV4::Commit changeCommit;
        QString outputError;
        if (buildOwnedOutput(QStringLiteral("change"), changeAmount, &changeOutput, &changeCommit, &outputError)) {
            changeOutput.workflowId = workflowId;
            storeOwnedOutput(changeOutput);
            localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
            localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
            localContext.insert(QStringLiteral("change_amount_display"), changeAmount);
            localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
            localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
        } else if (errorOut) {
            *errorOut = outputError.isEmpty()
                ? QStringLiteral("Failed to derive change output.")
                : outputError;
            return false;
        }
    }

    storeWorkflowContext(workflowId, localContext);
    refreshStateFromStorage();
    if (feeOut) {
        *feeOut = formatNanogrin(selection.fee);
    }
    return true;
}

bool GrinWalletController::ensureReceiverOutputContext(const QString &workflowId,
                                                       const QString &amount,
                                                       const QString &source,
                                                       WalletOutput *outputOut,
                                                       SlateV4::Commit *commitOut,
                                                       QString *errorOut)
{
    if (!outputOut || !commitOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Receiver output target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    const QString existingCommitment = localContext.value(QStringLiteral("receiver_commit")).toString();
    if (!existingCommitment.isEmpty()) {
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(
            loadDocument().value(QStringLiteral("wallet_state")).toObject());
        WalletOutput existingOutput = findTrackedOutputByCommitment(outputs, existingCommitment);
        if (existingOutput.commitment.isEmpty()) {
            existingOutput.commitment = existingCommitment;
            existingOutput.proof = localContext.value(QStringLiteral("receiver_proof")).toString();
            existingOutput.amount = localContext.value(QStringLiteral("receiver_amount_display")).toString(amount.trimmed());
            existingOutput.source = source;
            existingOutput.keyPath = localContext.value(QStringLiteral("receiver_key_path")).toString();
            existingOutput.blindingFactor = localContext.value(QStringLiteral("receiver_blind")).toString();
            existingOutput.childIndex = static_cast<quint32>(
                localContext.value(QStringLiteral("receiver_child_index")).toInt());
            existingOutput.workflowId = workflowId;
            existingOutput.pending = true;
            existingOutput.locked = false;
            existingOutput.onChain = false;
            existingOutput.spent = false;
            storeOwnedOutput(existingOutput);
        }

        *outputOut = existingOutput;
        commitOut->commitment = existingOutput.commitment;
        commitOut->proof = existingOutput.proof;
        return !existingOutput.commitment.isEmpty();
    }

    WalletOutput output;
    SlateV4::Commit commit;
    if (!buildOwnedOutput(source, amount, &output, &commit, errorOut)) {
        return false;
    }

    output.workflowId = workflowId;
    output.pending = true;
    output.locked = false;
    output.onChain = false;
    output.spent = false;
    storeOwnedOutput(output);

    localContext.insert(QStringLiteral("receiver_commit"), commit.commitment);
    localContext.insert(QStringLiteral("receiver_proof"), commit.proof);
    localContext.insert(QStringLiteral("receiver_amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("receiver_child_index"), static_cast<int>(output.childIndex));
    localContext.insert(QStringLiteral("receiver_key_path"), output.keyPath);
    localContext.insert(QStringLiteral("receiver_blind"), output.blindingFactor);
    storeWorkflowContext(workflowId, localContext);

    *outputOut = output;
    *commitOut = commit;
    return true;
}

void GrinWalletController::persistWorkflowTransaction(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    QJsonObject existingEntry;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject candidate = transactions.at(i).toObject();
        if (candidate.value(QStringLiteral("workflow_id")).toString() == slate.workflowId()) {
            existingEntry = candidate;
            break;
        }
    }

    QJsonObject entry;
    entry.insert(QStringLiteral("workflow_id"), slate.workflowId());
    entry.insert(QStringLiteral("mode"), slate.modeCode());
    entry.insert(QStringLiteral("state"), slate.stateCode());
    entry.insert(QStringLiteral("amount"), slate.amount);
    entry.insert(QStringLiteral("fee"), slate.fee);
    entry.insert(QStringLiteral("slate_id"), slate.id);
    entry.insert(QStringLiteral("offset"), slate.offset);
    entry.insert(QStringLiteral("broadcasted"),
                 broadcasted || existingEntry.value(QStringLiteral("broadcasted")).toBool());
    entry.insert(QStringLiteral("timestamp"),
                 existingEntry.value(QStringLiteral("timestamp")).toString().isEmpty()
                    ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate)
                    : existingEntry.value(QStringLiteral("timestamp")).toString());
    entry.insert(QStringLiteral("tx_ready"), slate.metadata.value(QStringLiteral("tx_ready")).toBool());
    entry.insert(QStringLiteral("confirmations"), existingEntry.value(QStringLiteral("confirmations")).toInt());
    entry.insert(QStringLiteral("kernel_excess"),
                 !slate.metadata.value(QStringLiteral("pubkey_total")).toString().isEmpty()
                     ? slate.metadata.value(QStringLiteral("pubkey_total")).toString()
                     : existingEntry.value(QStringLiteral("kernel_excess")).toString());
    entry.insert(QStringLiteral("kernel_signature"),
                 !slate.metadata.value(QStringLiteral("final_sig")).toString().isEmpty()
                     ? slate.metadata.value(QStringLiteral("final_sig")).toString()
                     : existingEntry.value(QStringLiteral("kernel_signature")).toString());
    entry.insert(QStringLiteral("status"),
                 broadcasted ? QStringLiteral("broadcasted")
                             : (slate.metadata.value(QStringLiteral("tx_ready")).toBool()
                                    ? QStringLiteral("ready")
                                    : existingEntry.value(QStringLiteral("status")).toString().isEmpty()
                                        ? QStringLiteral("in_progress")
                                        : existingEntry.value(QStringLiteral("status")).toString()));
    if (existingEntry.contains(QStringLiteral("broadcast_at"))) {
        entry.insert(QStringLiteral("broadcast_at"), existingEntry.value(QStringLiteral("broadcast_at")).toString());
    }
    if (existingEntry.contains(QStringLiteral("broadcast_error"))) {
        entry.insert(QStringLiteral("broadcast_error"), existingEntry.value(QStringLiteral("broadcast_error")).toString());
    }
    if (existingEntry.contains(QStringLiteral("confirmed_height"))) {
        entry.insert(QStringLiteral("confirmed_height"), existingEntry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong());
    }
    if (existingEntry.contains(QStringLiteral("last_node_check"))) {
        entry.insert(QStringLiteral("last_node_check"), existingEntry.value(QStringLiteral("last_node_check")).toString());
    }
    if (existingEntry.contains(QStringLiteral("output_commitments"))) {
        entry.insert(QStringLiteral("output_commitments"), existingEntry.value(QStringLiteral("output_commitments")).toArray());
    }
    if (existingEntry.contains(QStringLiteral("rescan_rebuilt"))) {
        entry.insert(QStringLiteral("rescan_rebuilt"), existingEntry.value(QStringLiteral("rescan_rebuilt")).toBool());
    }
    if (slate.hasPaymentProof) {
        entry.insert(QStringLiteral("payment_proof"), slate.paymentProof.toJson());
        const QString metadataStatus = slate.metadata.value(QStringLiteral("payment_proof_status")).toString();
        entry.insert(QStringLiteral("payment_proof_status"),
                     !metadataStatus.isEmpty()
                         ? metadataStatus
                         : (slate.paymentProof.receiverSignature.isEmpty()
                                ? QStringLiteral("pending")
                                : QStringLiteral("receiver_signed")));
        if (slate.metadata.contains(QStringLiteral("payment_proof_valid"))) {
            entry.insert(QStringLiteral("payment_proof_valid"),
                         slate.metadata.value(QStringLiteral("payment_proof_valid")).toBool());
        }
        if (!slate.metadata.value(QStringLiteral("payment_proof_error")).toString().isEmpty()) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         slate.metadata.value(QStringLiteral("payment_proof_error")).toString());
        } else if (existingEntry.contains(QStringLiteral("payment_proof_error"))) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         existingEntry.value(QStringLiteral("payment_proof_error")).toString());
        }
    } else if (existingEntry.contains(QStringLiteral("payment_proof"))) {
        entry.insert(QStringLiteral("payment_proof"), existingEntry.value(QStringLiteral("payment_proof")).toObject());
        if (existingEntry.contains(QStringLiteral("payment_proof_status"))) {
            entry.insert(QStringLiteral("payment_proof_status"),
                         existingEntry.value(QStringLiteral("payment_proof_status")).toString());
        }
        if (existingEntry.contains(QStringLiteral("payment_proof_valid"))) {
            entry.insert(QStringLiteral("payment_proof_valid"),
                         existingEntry.value(QStringLiteral("payment_proof_valid")).toBool());
        }
        if (existingEntry.contains(QStringLiteral("payment_proof_error"))) {
            entry.insert(QStringLiteral("payment_proof_error"),
                         existingEntry.value(QStringLiteral("payment_proof_error")).toString());
        }
    }
    if (slate.metadata.value(QStringLiteral("tx_skeleton")).isObject()) {
        const QJsonObject txSkeleton = slate.metadata.value(QStringLiteral("tx_skeleton")).toObject();
        entry.insert(QStringLiteral("tx_skeleton"), txSkeleton);
        const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
        if (!kernels.isEmpty()) {
            const QJsonObject kernel = kernels.first().toObject();
            if (!kernel.value(QStringLiteral("excess")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_excess"), kernel.value(QStringLiteral("excess")).toString());
            }
            if (!kernel.value(QStringLiteral("excess_sig")).toString().isEmpty()) {
                entry.insert(QStringLiteral("kernel_signature"), kernel.value(QStringLiteral("excess_sig")).toString());
            }
        }
    }

    bool replaced = false;
    for (int i = 0; i < transactions.size(); ++i) {
        if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == slate.workflowId()) {
            transactions.replace(i, entry);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        transactions.append(entry);
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
}

void GrinWalletController::updateTransactionEntry(const QString &workflowId, const std::function<void (QJsonObject &)> &updater)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool updated = false;
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        if (entry.value(QStringLiteral("workflow_id")).toString() != workflowId) {
            continue;
        }

        updater(entry);
        transactions.replace(i, entry);
        updated = true;
        break;
    }

    if (!updated) {
        return;
    }

    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

QString GrinWalletController::kernelExcessFromEntry(const QJsonObject &entry) const
{
    const QString direct = entry.value(QStringLiteral("kernel_excess")).toString();
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonObject txSkeleton = entry.value(QStringLiteral("tx_skeleton")).toObject();
    const QJsonArray kernels = txSkeleton.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray();
    if (!kernels.isEmpty()) {
        return kernels.first().toObject().value(QStringLiteral("excess")).toString();
    }

    return QString();
}

void GrinWalletController::refreshTransactionConfirmations()
{
    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    bool changed = false;

    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
        const QString status = entry.value(QStringLiteral("status")).toString();

        int confirmations = 0;
        if (confirmedHeight > 0 && m_chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<int>(m_chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
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
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
    }
}

void GrinWalletController::refreshStoragePersistenceState()
{
#ifdef Q_OS_WASM
    if (char *state = browserStoragePersistenceState()) {
        m_storagePersistenceState = QString::fromUtf8(state);
        free(state);
    } else {
        m_storagePersistenceState = QStringLiteral("best-effort");
    }
#else
    m_storagePersistenceState = QStringLiteral("native");
#endif
    emit statusChanged();
}

void GrinWalletController::touchWalletSession()
{
    if (!m_walletUnlocked) {
        return;
    }

    if (!m_sessionLockTimer) {
        startAutoRefresh();
    }
    if (m_sessionLockTimer) {
        m_sessionLockTimer->start();
    }
}

void GrinWalletController::recoverPendingBroadcasts()
{
    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    for (int i = 0; i < transactions.size(); ++i) {
        const QString status = transactions.at(i).toObject().value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("broadcast_pending")
            || status == QStringLiteral("broadcasted")
            || status == QStringLiteral("in_mempool")) {
            refreshBroadcastStatuses();
            return;
        }
    }
}

QJsonArray GrinWalletController::rebuildTransactionHistoryFromOutputs(const QList<WalletOutput> &outputs,
                                                                     const QJsonArray &existingTransactions) const
{
    QHash<QString, QJsonObject> knownTransactionsByWorkflow;
    QHash<QString, QString> workflowByCommitment;
    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty()) {
            knownTransactionsByWorkflow.insert(workflowId, entry);
        }

        const QStringList commitments = transactionOutputCommitments(entry);
        for (int j = 0; j < commitments.size(); ++j) {
            workflowByCommitment.insert(commitments.at(j),
                                        workflowId.isEmpty()
                                            ? syntheticWorkflowIdForCommitment(commitments.at(j))
                                            : workflowId);
        }
    }

    QHash<QString, QList<WalletOutput> > groupedOutputs;
    QStringList orderedWorkflowIds;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.commitment.isEmpty()) {
            continue;
        }

        QString workflowId = output.workflowId.trimmed();
        if (workflowId.isEmpty()) {
            workflowId = workflowByCommitment.value(output.commitment);
        }
        if (workflowId.isEmpty()) {
            workflowId = syntheticWorkflowIdForCommitment(output.commitment);
        }

        if (!groupedOutputs.contains(workflowId)) {
            orderedWorkflowIds.append(workflowId);
        }
        groupedOutputs[workflowId].append(output);
    }

    QJsonArray transactions;
    QSet<QString> appendedExistingWorkflows;
    for (int i = 0; i < orderedWorkflowIds.size(); ++i) {
        const QString workflowId = orderedWorkflowIds.at(i);
        const QList<WalletOutput> grouped = groupedOutputs.value(workflowId);
        if (grouped.isEmpty()) {
            continue;
        }

        QJsonObject entry = knownTransactionsByWorkflow.value(workflowId);
        appendedExistingWorkflows.insert(workflowId);

        quint64 confirmedHeight = 0;
        bool anySpent = false;
        bool anyPending = false;
        bool anyLocked = false;
        quint64 displayAmount = 0;
        QString primaryCommitment;
        QJsonArray outputCommitments;

        for (int j = 0; j < grouped.size(); ++j) {
            const WalletOutput &output = grouped.at(j);
            outputCommitments.append(output.commitment);
            if (primaryCommitment.isEmpty()) {
                primaryCommitment = output.commitment;
            }
            if (confirmedHeight == 0 || (output.height > 0 && output.height < confirmedHeight)) {
                confirmedHeight = output.height;
            }
            anySpent = anySpent || output.spent;
            anyPending = anyPending || output.pending;
            anyLocked = anyLocked || output.locked;
            if (displayAmount == 0 && output.source != QStringLiteral("change")) {
                displayAmount = amountToNanogrin(output.amount);
            }
        }

        if (displayAmount == 0) {
            displayAmount = amountToNanogrin(grouped.first().amount);
        }

        const QString mode = modeFromOutputs(grouped, entry.value(QStringLiteral("mode")).toString());
        const QString existingStatus = entry.value(QStringLiteral("status")).toString();
        const bool existingBroadcasted = entry.value(QStringLiteral("broadcasted")).toBool();
        QString status = existingStatus;
        if (status.isEmpty() || status == QStringLiteral("cancelled")) {
            status = anySpent && mode == QStringLiteral("send")
                ? QStringLiteral("spent")
                : QStringLiteral("confirmed");
        }
        if (anyPending || anyLocked) {
            status = QStringLiteral("in_progress");
        }
        const bool broadcasted = existingBroadcasted
            || (mode == QStringLiteral("send") && status != QStringLiteral("cancelled"));

        entry.insert(QStringLiteral("workflow_id"), workflowId);
        entry.insert(QStringLiteral("mode"), mode);
        entry.insert(QStringLiteral("state"),
                     entry.value(QStringLiteral("state")).toString().isEmpty()
                         ? QStringLiteral("chain")
                         : entry.value(QStringLiteral("state")).toString());
        if (entry.value(QStringLiteral("amount")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("amount"), formatNanogrin(displayAmount));
        }
        if (entry.value(QStringLiteral("fee")).toString().trimmed().isEmpty()) {
            entry.insert(QStringLiteral("fee"), QStringLiteral("0.000000000"));
        }
        entry.insert(QStringLiteral("broadcasted"), broadcasted);
        entry.insert(QStringLiteral("tx_ready"), true);
        entry.insert(QStringLiteral("status"), status);
        entry.insert(QStringLiteral("commitment"), primaryCommitment);
        entry.insert(QStringLiteral("output_commitments"), outputCommitments);
        entry.insert(QStringLiteral("source"),
                     grouped.first().source.isEmpty() ? QStringLiteral("scan") : grouped.first().source);
        entry.insert(QStringLiteral("confirmed_height"), static_cast<qint64>(confirmedHeight));
        entry.insert(QStringLiteral("confirmations"),
                     confirmedHeight > 0 && m_chainHeight >= confirmedHeight
                         ? static_cast<qint64>(m_chainHeight - confirmedHeight + 1)
                         : 0);
        entry.insert(QStringLiteral("rescan_rebuilt"), true);
        if (entry.value(QStringLiteral("timestamp")).toString().isEmpty()) {
            entry.insert(QStringLiteral("timestamp"), QString());
        }
        transactions.append(entry);
    }

    for (int i = 0; i < existingTransactions.size(); ++i) {
        const QJsonObject entry = existingTransactions.at(i).toObject();
        const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
        if (!workflowId.isEmpty() && !appendedExistingWorkflows.contains(workflowId)) {
            transactions.append(entry);
        }
    }

    return transactions;
}

void GrinWalletController::refreshBroadcastStatuses()
{
    if (!m_nodeApi || m_broadcastStatusRefreshInFlight || m_kernelStatusCheckInFlight) {
        return;
    }

    const QJsonArray transactions = loadDocument()
                                        .value(QStringLiteral("wallet_state"))
                                        .toObject()
                                        .value(QStringLiteral("transactions"))
                                        .toArray();
    bool needsRefresh = false;
    for (int i = 0; i < transactions.size(); ++i) {
        const QJsonObject entry = transactions.at(i).toObject();
        const QString status = entry.value(QStringLiteral("status")).toString();
        if (!entry.value(QStringLiteral("broadcasted")).toBool()
            && status != QStringLiteral("broadcast_pending")) {
            continue;
        }
        if (status != QStringLiteral("confirmed") && status != QStringLiteral("cancelled")) {
            needsRefresh = true;
            break;
        }
    }

    if (!needsRefresh) {
        return;
    }

    m_broadcastStatusRefreshInFlight = true;
    m_nodeApi->getUnconfirmedTransactionsAsync();
}

void GrinWalletController::startNextKernelStatusCheck()
{
    if (!m_nodeApi || m_kernelStatusCheckInFlight || m_kernelStatusQueue.isEmpty()) {
        return;
    }

    const QPair<QString, QString> next = m_kernelStatusQueue.takeFirst();
    m_currentKernelWorkflowId = next.first;
    m_currentKernelExcess = next.second;
    if (m_currentKernelExcess.isEmpty()) {
        startNextKernelStatusCheck();
        return;
    }

    m_kernelStatusCheckInFlight = true;
    m_nodeApi->getKernelAsync(m_currentKernelExcess, 0, static_cast<int>(m_chainHeight > 0 ? m_chainHeight + 2 : 0));
}

void GrinWalletController::finalizeWorkflowOutputs(const SlateV4 &slate, bool broadcasted)
{
    if (slate.workflowId().isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonObject localContext = workflowContext(slate.workflowId());
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selectedCommitments.size(); ++j) {
            if (outputs[i].commitment == selectedCommitments.at(j).toString()) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = !broadcasted;
                outputs[i].locked = !broadcasted;
                outputs[i].spent = broadcasted;
                outputs[i].onChain = false;
            }
        }

        if (!changeCommit.isEmpty() && outputs[i].commitment == changeCommit) {
            outputs[i].workflowId = slate.workflowId();
            outputs[i].pending = true;
            outputs[i].locked = !broadcasted;
        }

        for (int j = 0; j < slate.commitments.size(); ++j) {
            if (outputs[i].commitment == slate.commitments.at(j).commitment) {
                outputs[i].workflowId = slate.workflowId();
                outputs[i].pending = true;
                outputs[i].locked = !broadcasted;
            }
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

void GrinWalletController::finalizeBroadcastedWorkflow(const QString &workflowId)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    const QJsonObject localContext = workflowContext(workflowId);
    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();

    for (int i = 0; i < outputs.size(); ++i) {
        bool matchesSelectedInput = false;
        for (int j = 0; j < selectedCommitments.size(); ++j) {
            if (outputs[i].commitment == selectedCommitments.at(j).toString()) {
                matchesSelectedInput = true;
                break;
            }
        }

        if (matchesSelectedInput) {
            outputs[i].workflowId = workflowId;
            outputs[i].pending = false;
            outputs[i].locked = false;
            outputs[i].spent = true;
            outputs[i].onChain = false;
            continue;
        }

        if (outputs[i].workflowId == workflowId) {
            outputs[i].pending = true;
            outputs[i].locked = false;
        }
    }

    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();
}

void GrinWalletController::storeWorkflowContext(const QString &workflowId, const QJsonObject &context)
{
    if (workflowId.isEmpty()) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject contexts = document.value(QStringLiteral("workflow_contexts")).toObject();
    contexts.insert(workflowId, context);
    document.insert(QStringLiteral("workflow_contexts"), contexts);
    saveDocument(document);
}

QJsonObject GrinWalletController::workflowContext(const QString &workflowId) const
{
    if (workflowId.isEmpty()) {
        return QJsonObject();
    }

    const QJsonObject document = loadDocument();
    return document.value(QStringLiteral("workflow_contexts")).toObject().value(workflowId).toObject();
}

void GrinWalletController::startSeedScan()
{
    m_seedScanActive = true;
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    m_seedScanNextIndex = qMax<qulonglong>(
        1,
        walletState.value(QStringLiteral("restore_leaf_index")).toVariant().toULongLong() + 1);
    m_seedScanDiscovered.clear();
    m_syncStatus = QStringLiteral("Seed scan started at leaf %1").arg(QString::number(m_seedScanNextIndex));
    emit statusChanged();
    m_nodeApi->getUnspentOutputsAsync(static_cast<int>(m_seedScanNextIndex), -1, 1000, true);
}

void GrinWalletController::finishSeedScan(const QString &message)
{
    m_seedScanActive = false;
    if (!message.isEmpty()) {
        setLastInfo(message);
    }
}

void GrinWalletController::requestWalletScan()
{
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before scanning outputs."));
        return;
    }

    if (m_walletScanInFlight || m_seedScanActive) {
        setLastInfo(QStringLiteral("Wallet scan is already running."));
        return;
    }

    if (!m_nodeApi) {
        connectNodeClient();
    }
    if (!m_nodeApi) {
        setLastError(QStringLiteral("Node client is not configured."));
        return;
    }

    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (outputs.isEmpty()) {
        QJsonObject document = loadDocument();
        QJsonObject state = document.value(QStringLiteral("wallet_state")).toObject();
        state.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        state.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
        document.insert(QStringLiteral("wallet_state"), state);
        saveDocument(document);
        refreshStateFromStorage();
        setLastInfo(QStringLiteral("Wallet has no tracked outputs yet. Starting seed scan."));
    }

    m_syncStatus = QStringLiteral("Scanning wallet outputs...");
    emit statusChanged();
    m_walletScanInFlight = true;
    startSeedScan();
}
