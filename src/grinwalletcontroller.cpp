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
#include <QDebug>
#include <QUrl>
#include <QUuid>
#include <QVector>
#include <cstdlib>
#include <algorithm>

#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/emscripten.h>

class GrinWalletController;
static GrinWalletController *g_shortcutController = nullptr;

extern "C" {
EMSCRIPTEN_KEEPALIVE int grinffindorHandleShortcut(int key);
}

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

EM_JS(char *, browserReadClipboardText, (), {
    try {
        if (navigator.clipboard && navigator.clipboard.readText) {
            return Asyncify.handleAsync(async () => {
                try {
                    const value = await navigator.clipboard.readText();
                    const text = value || "";
                    const length = lengthBytesUTF8(text) + 1;
                    const buffer = _malloc(length);
                    stringToUTF8(text, buffer, length);
                    return buffer;
                } catch (e) {
                    const fallback = "";
                    const length = lengthBytesUTF8(fallback) + 1;
                    const buffer = _malloc(length);
                    stringToUTF8(fallback, buffer, length);
                    return buffer;
                }
            });
        }
    } catch (e) {}
    const fallback = "";
    const length = lengthBytesUTF8(fallback) + 1;
    const buffer = _malloc(length);
    stringToUTF8(fallback, buffer, length);
    return buffer;
});

EM_JS(char *, browserConsumeCapturedPasteText, (), {
    try {
        if (typeof window !== "undefined"
            && typeof window.__grinffindorCapturedPasteText === "string") {
            const value = window.__grinffindorCapturedPasteText;
            window.__grinffindorCapturedPasteText = "";
            const length = lengthBytesUTF8(value) + 1;
            const buffer = _malloc(length);
            stringToUTF8(value, buffer, length);
            return buffer;
        }
    } catch (e) {}
    const fallback = "";
    const length = lengthBytesUTF8(fallback) + 1;
    const buffer = _malloc(length);
    stringToUTF8(fallback, buffer, length);
    return buffer;
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

        const debug = function(label, payload) {
            try {
                console.log("[ShortcutBridge]", label, payload || "");
            } catch (e) {}
        };

        const qtCanvas = function() {
            if (typeof document === "undefined") {
                return null;
            }
            return document.querySelector("canvas");
        };

        const qtShortcutTarget = function() {
            if (typeof document === "undefined") {
                return null;
            }
            const active = document.activeElement;
            if (active) {
                const tag = (active.tagName || "").toUpperCase();
                const isEditable = active.isContentEditable
                    || tag === "INPUT"
                    || tag === "TEXTAREA"
                    || active.getAttribute("contenteditable") === "true"
                    || active.getAttribute("role") === "textbox";
                if (isEditable || tag === "DIV") {
                    return active;
                }
            }
            return qtCanvas();
        };

        const shortcutContextFocused = function() {
            try {
                return !!(window.__grinffindorShortcutContext && window.__grinffindorShortcutContext.focused);
            } catch (e) {}
            return false;
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
            return isQtCanvasFocused() || shortcutContextFocused();
        };

        const triggerQtShortcut = function(event) {
            const key = (event && event.key ? event.key : "").toLowerCase();
            let qtKey = 0;
            if (key === "a") {
                qtKey = 65;
            } else if (key === "c") {
                qtKey = 67;
            } else if (key === "v") {
                qtKey = 86;
            }
            if (!qtKey || typeof Module === "undefined" || typeof Module._grinffindorHandleShortcut !== "function") {
                debug("shortcut-dispatch-unavailable", {
                    key: key,
                    hasModule: typeof Module !== "undefined"
                });
                return false;
            }
            const result = Module._grinffindorHandleShortcut(qtKey);
            debug("shortcut-dispatch-result", {
                key: key,
                result: result
            });
            return !!result;
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

        const capturePasteText = function(value) {
            window.__grinffindorCapturedPasteText = value || "";
            debug("captured-paste", {
                length: (value || "").length
            });
        };

        window.addEventListener("paste", function(event) {
            try {
                if (!isQtCanvasFocused() && !shortcutContextFocused()) {
                    return;
                }
                debug("dom-paste", {
                    focused: shortcutContextFocused()
                });
                if (event && event.preventDefault) {
                    event.preventDefault();
                }
                if (event && event.stopPropagation) {
                    event.stopPropagation();
                }
                const clipboard = event.clipboardData || window.clipboardData;
                if (!clipboard || !clipboard.getData) {
                    return;
                }
                capturePasteText(clipboard.getData("text") || "");
                const handled = (typeof Module !== "undefined"
                    && typeof Module._grinffindorHandleShortcut === "function")
                    ? !!Module._grinffindorHandleShortcut(86)
                    : false;
                debug("paste-dispatch", {
                    handled: handled
                });
            } catch (e) {}
        }, true);

        window.addEventListener("beforeinput", function(event) {
            try {
                if (!event) {
                    return;
                }
                const inputType = String(event.inputType || "");
                debug("beforeinput", {
                    inputType: inputType,
                    focused: shortcutContextFocused()
                });
                if (inputType !== "insertFromPaste" && inputType !== "insertFromPasteAsQuotation") {
                    return;
                }
                if (!isQtCanvasFocused() && !shortcutContextFocused()) {
                    return;
                }
                if (event.preventDefault) {
                    event.preventDefault();
                }
                if (event.stopPropagation) {
                    event.stopPropagation();
                }
            } catch (e) {}
        }, true);

        if (typeof document !== "undefined" && document.addEventListener) {
            document.addEventListener("contextmenu", function(event) {
                try {
                    debug("contextmenu", {
                        focused: shortcutContextFocused(),
                        activeTag: document.activeElement ? (document.activeElement.tagName || "") : ""
                    });
                    if (!shortcutContextFocused()) {
                        return;
                    }
                    if (event && event.preventDefault) {
                        event.preventDefault();
                    }
                    if (event && event.stopPropagation) {
                        event.stopPropagation();
                    }
                } catch (e) {}
            }, true);
        }

        window.addEventListener("keydown", function(event) {
            if (shouldIntercept(event)) {
                debug("keydown-intercept", {
                    key: event.key || "",
                    focused: shortcutContextFocused()
                });
                event.preventDefault();
                const handled = triggerQtShortcut(event);
                if (!handled) {
                    browserFallbackShortcut(event);
                }
                debug("keydown-dispatch", {
                    key: event.key || "",
                    handled: handled
                });
            }
        }, true);

        window.addEventListener("keyup", function(event) {
            if (shouldIntercept(event)) {
                event.preventDefault();
                debug("keyup-intercept", event.key || "");
            }
        }, true);

        debug("installed");
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

const char *kAppSettingsKey = "app_settings";
const char *kAutoLockOnDeactivateKey = "auto_lock_on_app_deactivate";

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

QString displayAmountForTransactionEntry(const QJsonObject &entry, const QList<WalletOutput> &outputs)
{
    const QString storedAmount = entry.value(QStringLiteral("amount")).toString().trimmed();
    if (!storedAmount.isEmpty()) {
        return storedAmount;
    }

    const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
    quint64 receivedAmount = 0;
    quint64 changeAmount = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (output.workflowId != workflowId) {
            continue;
        }
        const quint64 amount = amountToNanogrin(output.amount);
        if (output.source == QStringLiteral("change")) {
            changeAmount += amount;
        } else if (output.source == QStringLiteral("receive") || output.source == QStringLiteral("invoice")) {
            receivedAmount += amount;
        }
    }

    if (receivedAmount > 0) {
        return formatNanogrin(receivedAmount);
    }

    quint64 inputAmount = 0;
    const QJsonArray inputs = entry.value(QStringLiteral("tx_skeleton"))
                                  .toObject()
                                  .value(QStringLiteral("body"))
                                  .toObject()
                                  .value(QStringLiteral("inputs"))
                                  .toArray();
    for (int i = 0; i < inputs.size(); ++i) {
        const QJsonObject input = inputs.at(i).toObject();
        QString commitment = input.value(QStringLiteral("commit")).toString();
        if (commitment.isEmpty()) {
            commitment = input.value(QStringLiteral("commit")).toObject().value(QStringLiteral("hex")).toString();
        }
        if (commitment.isEmpty()) {
            continue;
        }
        inputAmount += amountToNanogrin(findTrackedOutputByCommitment(outputs, commitment).amount);
    }

    const quint64 feeAmount = amountToNanogrin(entry.value(QStringLiteral("fee")).toString());
    if (inputAmount > 0 && inputAmount >= changeAmount + feeAmount) {
        return formatNanogrin(inputAmount - changeAmount - feeAmount);
    }

    return QString();
}

WalletOutput normalizedTrackedOutput(const WalletOutput &output, const WalletKeychain &keychain)
{
    if (!keychain.isValid()
        || output.source != QStringLiteral("scan")
        || output.keyPath.isEmpty()
        || !output.keyPath.startsWith(QStringLiteral("m/0/0/"))) {
        return output;
    }

    const WalletKeychain::OutputSecrets secrets =
        keychain.deriveOutputSecrets(output.childIndex, amountToNanogrin(output.amount));
    if (!secrets.success) {
        return output;
    }

    WalletOutput normalized = output;
    normalized.blindingFactor = QString::fromUtf8(secrets.blindingFactor.toHex());
    return normalized;
}

QString syntheticWorkflowIdForCommitment(const QString &commitment)
{
    return QStringLiteral("rescan-%1").arg(commitment.left(24));
}

QString invoiceContextKey(const QString &suffix)
{
    return QStringLiteral("invoice_context_%1").arg(suffix);
}

WalletCryptoBackend::ParticipantContext participantContextFromJson(const QJsonObject &json,
                                                                  const QString &role)
{
    WalletCryptoBackend::ParticipantContext context;
    context.role = role;
    context.blindSecret = json.value(QStringLiteral("sec_key")).toString();
    context.nonceSecret = json.value(QStringLiteral("sec_nonce")).toString();
    context.blindPublic = json.value(QStringLiteral("pub_key")).toString();
    context.noncePublic = json.value(QStringLiteral("pub_nonce")).toString();
    return context;
}

QJsonObject participantContextToJson(const WalletCryptoBackend::ParticipantContext &context)
{
    QJsonObject json;
    json.insert(QStringLiteral("sec_key"), context.blindSecret);
    json.insert(QStringLiteral("sec_nonce"), context.nonceSecret);
    json.insert(QStringLiteral("pub_key"), context.blindPublic);
    json.insert(QStringLiteral("pub_nonce"), context.noncePublic);
    return json;
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

qint64 inferredConfirmedHeightForTransactionEntry(const QJsonObject &entry, const QList<WalletOutput> &outputs)
{
    // Always try live output state first: it reflects the actual on-chain block and
    // overrides any stale stored height that might have been written from old input UTXOs.
    const QString workflowId = entry.value(QStringLiteral("workflow_id")).toString();
    // For receive/invoice transactions the owned UTXO may later be spent (used as input
    // in a subsequent send), but its creation height is still correct as the receive height.
    const QString mode = entry.value(QStringLiteral("mode")).toString();
    const bool isReceiveSide = (mode == QStringLiteral("receive") || mode == QStringLiteral("invoice"));
    quint64 inferredHeight = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (!output.onChain || output.height == 0) {
            continue;
        }
        // For send transactions skip spent inputs: their height predates the send.
        // For receive/invoice keep spent outputs: creation height = receive height.
        if (!isReceiveSide && output.spent) {
            continue;
        }
        if (!workflowId.isEmpty() && output.workflowId == workflowId) {
            if (inferredHeight == 0 || output.height < inferredHeight) {
                inferredHeight = output.height;
            }
        }
    }

    if (inferredHeight == 0) {
        const QStringList commitments = transactionOutputCommitments(entry);
        for (int i = 0; i < commitments.size(); ++i) {
            const WalletOutput output = findTrackedOutputByCommitment(outputs, commitments.at(i));
            if (!output.onChain || output.height == 0) {
                continue;
            }
            if (!isReceiveSide && output.spent) {
                continue;
            }
            if (inferredHeight == 0 || output.height < inferredHeight) {
                inferredHeight = output.height;
            }
        }
    }

    if (inferredHeight > 0) {
        return static_cast<qint64>(inferredHeight);
    }

    // Fall back to the stored height (e.g. set by kernel check or from a previous scan
    // before the output was discovered on-chain).
    return entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
}

qint64 transactionSortKey(const QJsonObject &entry)
{
    const QStringList timeFields = QStringList()
        << QStringLiteral("cancelled_at")
        << QStringLiteral("broadcast_at")
        << QStringLiteral("last_broadcast_attempt")
        << QStringLiteral("timestamp");

    for (const QString &field : timeFields) {
        const QString value = entry.value(field).toString().trimmed();
        if (value.isEmpty()) {
            continue;
        }
        const QDateTime parsed = QDateTime::fromString(value, Qt::ISODate);
        if (parsed.isValid()) {
            return parsed.toUTC().toMSecsSinceEpoch();
        }
    }

    const qint64 confirmedHeight = entry.value(QStringLiteral("confirmed_height")).toVariant().toLongLong();
    if (confirmedHeight > 0) {
        return confirmedHeight;
    }

    return 0;
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

bool compactCommitLessThan(const SlateV4::Commit &left, const SlateV4::Commit &right)
{
    const QByteArray leftBytes = QByteArray::fromHex(left.commitment.toUtf8());
    const QByteArray rightBytes = QByteArray::fromHex(right.commitment.toUtf8());
    if (leftBytes.size() == rightBytes.size() && !leftBytes.isEmpty()) {
        return leftBytes < rightBytes;
    }
    return left.commitment < right.commitment;
}

QList<SlateV4::Commit> sortedCompactCommitments(const QList<SlateV4::Commit> &commits)
{
    QList<SlateV4::Commit> inputs;
    QList<SlateV4::Commit> outputs;
    for (const SlateV4::Commit &commit : commits) {
        if (commit.proof.trimmed().isEmpty()) {
            inputs.append(commit);
        } else {
            outputs.append(commit);
        }
    }

    std::sort(inputs.begin(), inputs.end(), compactCommitLessThan);
    std::sort(outputs.begin(), outputs.end(), compactCommitLessThan);

    QList<SlateV4::Commit> ordered = inputs;
    ordered.append(outputs);
    return ordered;
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

QString shortHexPreview(const QByteArray &data, int bytes = 16)
{
    return QString::fromUtf8(data.left(bytes).toHex());
}

QString shortTextPreview(const QString &text, int maxChars = 48)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() <= maxChars) {
        return trimmed;
    }
    return trimmed.left(maxChars) + QStringLiteral("...");
}

QString summarizeSlatepackPayload(const QByteArray &payload)
{
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(payload);
    if (jsonDocument.isObject()) {
        const QJsonObject object = jsonDocument.object();
        const QString sender = object.value(QStringLiteral("sender")).toString().trimmed();
        const QString payloadText = object.value(QStringLiteral("payload")).toString();
        return QStringLiteral("json-envelope mode=%1 sender=%2 payloadChars=%3")
            .arg(QString::number(object.value(QStringLiteral("mode")).toInt(-1)),
                 sender.isEmpty() ? QStringLiteral("-") : sender,
                 QString::number(payloadText.size()));
    }

    if (payload.size() >= 17) {
        const quint8 major = static_cast<quint8>(payload.at(0));
        const quint8 minor = static_cast<quint8>(payload.at(1));
        const quint8 mode = static_cast<quint8>(payload.at(2));
        const quint16 optFlags =
            (static_cast<quint8>(payload.at(3)) << 8)
            | static_cast<quint8>(payload.at(4));
        quint32 optFieldsLen = 0;
        for (int i = 5; i < 9; ++i) {
            optFieldsLen = (optFieldsLen << 8) | static_cast<quint8>(payload.at(i));
        }
        quint64 innerPayloadLen = 0;
        const int payloadLenOffset = 9 + static_cast<int>(optFieldsLen);
        if (payloadLenOffset + 8 <= payload.size()) {
            for (int i = payloadLenOffset; i < payloadLenOffset + 8; ++i) {
                innerPayloadLen = (innerPayloadLen << 8) | static_cast<quint8>(payload.at(i));
            }
        } else {
            return QStringLiteral("binary-envelope major=%1 minor=%2 mode=%3 optFlags=%4 optFieldsLen=%5 innerPayloadLen=truncated")
                .arg(QString::number(major),
                     QString::number(minor),
                     QString::number(mode),
                     QString::number(optFlags),
                     QString::number(optFieldsLen));
        }
        return QStringLiteral("binary-envelope major=%1 minor=%2 mode=%3 optFlags=%4 optFieldsLen=%5 innerPayloadLen=%6")
            .arg(QString::number(major),
                 QString::number(minor),
                 QString::number(mode),
                 QString::number(optFlags),
                 QString::number(optFieldsLen),
                 QString::number(innerPayloadLen));
    }

    return QStringLiteral("unknown-payload size=%1 preview=%2")
        .arg(QString::number(payload.size()), shortHexPreview(payload));
}

QString summarizeArmoredSlatepack(const QString &slatepack)
{
    const QString trimmed = slatepack.trimmed();
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(trimmed.toUtf8());
    if (jsonDocument.isObject()) {
        return summarizeSlatepackPayload(trimmed.toUtf8());
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
            return QStringLiteral("armored %1").arg(summarizeSlatepackPayload(payload));
        }
        return QStringLiteral("armored invalid-base58 decodedSize=%1").arg(QString::number(decoded.size()));
    }

    return QStringLiteral("plain-text chars=%1 preview=%2")
        .arg(QString::number(trimmed.size()), shortTextPreview(trimmed));
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
        qDebug() << "[SlatepackDecode] empty input";
        return QString();
    }
    qDebug() << "[SlatepackDecode]"
             << "inputChars=" << trimmed.size()
             << "decryptionKeyBytes=" << decryptionKey.size()
             << "summary=" << summarizeArmoredSlatepack(trimmed);

    const QJsonDocument jsonDocument = QJsonDocument::fromJson(trimmed.toUtf8());
    if (jsonDocument.isObject()) {
        QString decodedPayload;
        QString parseError;
        if (BinarySlateV4Reader::decodeSlatepackPayload(trimmed.toUtf8(), decryptionKey, &decodedPayload, &parseError)) {
            qDebug() << "[SlatepackDecode] json payload decoded"
                     << "decodedChars=" << decodedPayload.size();
            return decodedPayload;
        }
        qDebug() << "[SlatepackDecode] json payload parse failed"
                 << "error=" << parseError;

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
                qDebug() << "[SlatepackDecode] armored payload decoded"
                         << "payloadBytes=" << payload.size()
                         << "decodedChars=" << decodedPayload.size();
                return decodedPayload;
            }
            qDebug() << "[SlatepackDecode] armored payload parse failed"
                     << "payloadBytes=" << payload.size()
                     << "error=" << parseError
                     << "payloadPreview=" << shortHexPreview(payload);
            return buildSlatepackDiagnostic(QStringLiteral("armored"), payload, parseError);
        }
    }

    qDebug() << "[SlatepackDecode] falling back to decodeSlatepackArmor";
    return decodeSlatepackArmor(trimmed);
}

bool invokeNoArgMethod(QObject *object, const char *methodName)
{
    return object && QMetaObject::invokeMethod(object, methodName, Qt::DirectConnection);
}

bool replaceFocusedObjectSelection(QObject *object, const QString &text)
{
    if (!object) {
        qDebug() << "[ShortcutFilter] replaceFocusedObjectSelection: no focus object";
        return false;
    }

    const QVariant selectionStartValue = object->property("selectionStart");
    const QVariant selectionEndValue = object->property("selectionEnd");
    if (selectionStartValue.isValid() && selectionEndValue.isValid()) {
        bool okStart = false;
        bool okEnd = false;
        const int selectionStart = selectionStartValue.toInt(&okStart);
        const int selectionEnd = selectionEndValue.toInt(&okEnd);
        if (okStart && okEnd) {
            const int start = std::min(selectionStart, selectionEnd);
            const int end = std::max(selectionStart, selectionEnd);
            if (end > start) {
                QMetaObject::invokeMethod(object,
                                          "remove",
                                          Qt::DirectConnection,
                                          Q_ARG(int, start),
                                          Q_ARG(int, end));
            }
            const bool inserted = QMetaObject::invokeMethod(object,
                                                            "insert",
                                                            Qt::DirectConnection,
                                                            Q_ARG(int, start),
                                                            Q_ARG(QString, text));
            qDebug() << "[ShortcutFilter] insert method"
                     << object->metaObject()->className()
                     << "start=" << start
                     << "end=" << end
                     << "ok=" << inserted;
            if (inserted) {
                object->setProperty("cursorPosition", start + text.length());
                return true;
            }
        }
    }

    const QVariant currentText = object->property("text");
    if (!currentText.isValid()) {
        return false;
    }

    const bool textSet = object->setProperty("text", text);
    const bool cursorSet = object->setProperty("cursorPosition", text.length());
    qDebug() << "[ShortcutFilter] text property fallback"
             << object->metaObject()->className()
             << "textSet=" << textSet
             << "cursorSet=" << cursorSet;
    return textSet;
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

#ifdef Q_OS_WASM
extern "C" EMSCRIPTEN_KEEPALIVE int grinffindorHandleShortcut(int key)
{
    if (!g_shortcutController) {
        qDebug() << "[ShortcutFilter] no shortcut controller for key" << key;
        return 0;
    }
    return g_shortcutController->processShortcutKey(key) ? 1 : 0;
}
#endif

GrinWalletController::GrinWalletController(QObject *parent) :
    QObject(parent),
    m_nodeApi(0),
    m_autoRefreshTimer(0),
    m_sessionLockTimer(0),
    m_walletExists(false),
    m_walletUnlocked(false),
    m_selectedNetwork(defaultNetworkName()),
    m_storagePersistenceState(QStringLiteral("unknown")),
    m_autoLockOnAppDeactivate(false),
    m_chainHeight(0),
    m_nodeBlockHeaderVersion(0),
    m_syncStatus(QStringLiteral("Idle")),
    m_totalBalance(QStringLiteral("0.000000000")),
    m_spendableBalance(QStringLiteral("0.000000000")),
    m_lockedBalance(QStringLiteral("0.000000000")),
    m_immatureBalance(QStringLiteral("0.000000000")),
    m_awaitingConfirmationBalance(QStringLiteral("0.000000000")),
    m_awaitingFinalizationBalance(QStringLiteral("0.000000000")),
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
QString GrinWalletController::awaitingConfirmationBalance() const { return m_awaitingConfirmationBalance; }
QString GrinWalletController::awaitingFinalizationBalance() const { return m_awaitingFinalizationBalance; }
qulonglong GrinWalletController::scanHeight() const { return m_scanHeight; }
QString GrinWalletController::lastError() const { return m_lastError; }
QString GrinWalletController::lastInfo() const { return m_lastInfo; }
QString GrinWalletController::workflowId() const { return m_workflowId; }
QString GrinWalletController::workflowState() const { return m_workflowState; }
QString GrinWalletController::workflowMode() const { return m_workflowMode; }
QString GrinWalletController::workflowSlatepack() const { return m_workflowSlatepack; }
QString GrinWalletController::workflowDecoded() const { return m_workflowDecoded; }
bool GrinWalletController::autoLockOnAppDeactivate() const { return m_autoLockOnAppDeactivate; }
QVariantList GrinWalletController::transactionHistory() const
{
    QVariantList history;
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    QList<QJsonObject> entries;
    entries.reserve(transactions.size());
    for (int i = 0; i < transactions.size(); ++i) {
        QJsonObject entry = transactions.at(i).toObject();
        const qint64 confirmedHeight = inferredConfirmedHeightForTransactionEntry(entry, outputs);
        const QString status = entry.value(QStringLiteral("status")).toString();
        qint64 confirmations = 0;
        if (confirmedHeight > 0 && m_chainHeight >= static_cast<qulonglong>(confirmedHeight)) {
            confirmations = static_cast<qint64>(m_chainHeight - static_cast<qulonglong>(confirmedHeight) + 1);
        }
        if (confirmedHeight > 0) {
            entry.insert(QStringLiteral("confirmed_height"), confirmedHeight);
        }
        entry.insert(QStringLiteral("confirmations"), confirmations);
        if (confirmedHeight > 0 && confirmations > 0 && status != QStringLiteral("cancelled")) {
            entry.insert(QStringLiteral("status"), QStringLiteral("confirmed"));
        }
        const QString displayAmount = displayAmountForTransactionEntry(entry, outputs);
        if (!displayAmount.isEmpty()) {
            entry.insert(QStringLiteral("amount"), displayAmount);
        }
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const QJsonObject &left, const QJsonObject &right) {
        const qint64 leftKey = transactionSortKey(left);
        const qint64 rightKey = transactionSortKey(right);
        if (leftKey != rightKey) {
            return leftKey > rightKey;
        }
        return left.value(QStringLiteral("workflow_id")).toString()
            > right.value(QStringLiteral("workflow_id")).toString();
    });

    history.reserve(entries.size());
    for (const QJsonObject &entry : entries) {
        history.append(entry.toVariantMap());
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
#ifdef Q_OS_WASM
    g_shortcutController = this;
#endif
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
    if (key != Qt::Key_A && key != Qt::Key_C && key != Qt::Key_V) {
        return QObject::eventFilter(watched, event);
    }

    if (processShortcutKey(key)) {
        keyEvent->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool GrinWalletController::processShortcutKey(int key)
{
    if (key != Qt::Key_A && key != Qt::Key_C && key != Qt::Key_V) {
        return false;
    }

    QObject *focusObject = qApp->focusObject();
    if (!focusObject) {
        qDebug() << "[ShortcutFilter] no focus object for key" << key;
        return false;
    }

    qDebug() << "[ShortcutFilter] key="
             << QKeySequence(key).toString()
             << "qtKey=" << key
             << "focusClass=" << focusObject->metaObject()->className()
             << "objectName=" << focusObject->objectName()
             << "selectedTextLength=" << focusObject->property("selectedText").toString().length()
             << "textLength=" << focusObject->property("text").toString().length();

    if (key == Qt::Key_A) {
        if (invokeNoArgMethod(focusObject, "selectAll")) {
            qDebug() << "[ShortcutFilter] selectAll accepted";
            return true;
        }
        qDebug() << "[ShortcutFilter] selectAll unavailable";
        return false;
    }

    if (key == Qt::Key_V) {
        const QString pastedText = requestPasteText();
        qDebug() << "[ShortcutFilter] paste text length=" << pastedText.length();
        if (!pastedText.isEmpty() && replaceFocusedObjectSelection(focusObject, pastedText)) {
            qDebug() << "[ShortcutFilter] paste accepted";
            return true;
        }
        qDebug() << "[ShortcutFilter] paste rejected";
        return false;
    }

    if (invokeNoArgMethod(focusObject, "copy")) {
        qDebug() << "[ShortcutFilter] copy accepted via method";
        return true;
    }

    const QString copiedText = focusedObjectText(focusObject);
    qDebug() << "[ShortcutFilter] copy fallback length=" << copiedText.length();
    if (!copiedText.isEmpty() && copyTextToClipboard(copiedText)) {
        qDebug() << "[ShortcutFilter] copy accepted via fallback";
        return true;
    }

    return false;
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
    m_nodeApi->getVersionAsync();
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
    const char *capturedValue = browserConsumeCapturedPasteText();
    const QString capturedText = QString::fromUtf8(capturedValue ? capturedValue : "");
    if (!capturedText.isEmpty()) {
        qDebug() << "[ShortcutFilter] requestPasteText using captured buffer length=" << capturedText.length();
        return capturedText;
    }
    const char *value = browserReadClipboardText();
    const QString clipboardText = QString::fromUtf8(value ? value : "");
    qDebug() << "[ShortcutFilter] requestPasteText navigator clipboard length=" << clipboardText.length();
    return clipboardText;
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
    const qulonglong effectiveHeight = m_chainHeight > 0 ? m_chainHeight : m_scanHeight;
    const WalletSelection::Result selection =
        WalletSelection::selectSpendableOutputs(outputs, requestedAmount, effectiveHeight);
    if (!selection.success) {
        setLastError(selection.error);
        return;
    }

    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs[i].commitment == selection.selectedOutputs.at(j).commitment) {
                // Only lock the input UTXO. Preserve its workflowId so the transaction
                // that created it (e.g. an invoice) retains its own identity.
                outputs[i].locked = true;
            }
        }
    }

    SlateV4 slate;
    alignSlateVersionWithNode(&slate);
    const QString workflowId = generateWorkflowId();
    // Do NOT set workflowId on input UTXOs here – they belong to the workflow that
    // received them. The send workflow tracks its inputs via selected_input_commits.
    walletState.insert(QStringLiteral("outputs"), WalletScanner::outputsToJson(outputs));
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    const WalletCryptoBackend::ParticipantContext senderContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    slate.state = SlateV4::Standard1;
    slate.amount = formatNanogrin(requestedAmount);
    slate.fee = QStringLiteral("%1.%2")
        .arg(QString::number(selection.fee / 1000000000ULL))
        .arg(QString::number(selection.fee % 1000000000ULL), 9, QLatin1Char('0'));
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
    QJsonObject selectedInputCoinbase;
    for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
        selectedCommitments.append(selection.selectedOutputs.at(i).commitment);
        selectedInputCoinbase.insert(selection.selectedOutputs.at(i).commitment,
                                     selection.selectedOutputs.at(i).coinbase);
    }
    localContext.insert(QStringLiteral("selected_input_commits"), selectedCommitments);
    localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);

    // Declare changeOutput outside the block so its blinding factor is available for offset computation.
    WalletOutput changeOutput;
    SlateV4::Commit changeCommit;
    if (selection.change > 0) {
        const QString changeAmount = QStringLiteral("%1.%2")
            .arg(QString::number(selection.change / 1000000000ULL))
            .arg(QString::number(selection.change % 1000000000ULL), 9, QLatin1Char('0'));
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

    // Compute S1 kernel offset per grin-wallet reference:
    //   offset = change_blind - xs_sender_aggsig_secret - sum(input_blinds)
    // This guarantees: sum(output_blinds) - sum(input_blinds) = kernel_excess + offset
    // where kernel_excess = xs_sender_pubkey + xs_receiver_pubkey (from aggsig).
    {
        // All negatives: xs_sender aggsig key + each input's blinding factor
        QStringList negatives;
        negatives << senderContext.blindSecret;

        if (!m_sessionMnemonic.trimmed().isEmpty()) {
            const WalletKeychain inputKeychain(m_sessionMnemonic);
            if (inputKeychain.isValid()) {
                for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
                    const WalletOutput normInput =
                        normalizedTrackedOutput(selection.selectedOutputs.at(i), inputKeychain);
                    if (!normInput.blindingFactor.isEmpty()) {
                        negatives << normInput.blindingFactor;
                        qDebug() << "[S1Offset] input blind"
                                 << "commitment=" << normInput.commitment.left(16)
                                 << "source="     << normInput.source;
                    } else {
                        qDebug() << "[S1Offset] input blind missing"
                                 << "commitment=" << selection.selectedOutputs.at(i).commitment.left(16)
                                 << "source="     << selection.selectedOutputs.at(i).source;
                    }
                }
            }
        }

        QString offsetError;
        QString computedOffset;
        if (!changeOutput.blindingFactor.isEmpty()) {
            // offset = change_blind - xs_sender - sum(inputs)
            computedOffset = WalletCryptoBackend::combineBlindingFactors(
                QStringList() << changeOutput.blindingFactor, negatives, &offsetError);
        } else {
            // No change output: offset = -(xs_sender + sum(inputs))
            const QString sumToNegate =
                WalletCryptoBackend::combineBlindingFactors(negatives, QStringList(), &offsetError);
            if (!sumToNegate.isEmpty()) {
                computedOffset = WalletCryptoBackend::negateScalar(sumToNegate, &offsetError);
            }
        }

        if (!computedOffset.isEmpty()) {
            slate.offset = computedOffset;
            qDebug() << "[S1Offset] offset computed correctly"
                     << "workflowId="  << workflowId
                     << "hasChange="   << !changeOutput.blindingFactor.isEmpty()
                     << "inputCount="  << (negatives.size() - 1)
                     << "offset="      << computedOffset.left(16);
        } else {
            // Fallback: deterministic offset — transaction will fail node validation.
            // Should not happen when wallet is unlocked and outputs are tracked.
            slate.offset = WalletCryptoBackend::createOffset(m_seedFingerprint, slate.id);
            qDebug() << "[S1Offset] fallback to derived offset"
                     << "workflowId=" << workflowId
                     << "error="      << offsetError;
        }
    }
    // Populate slate.commitments for S1 per reference:
    // sender inputs as plain commits (no proof), change output with proof.
    // This allows the receiver to verify the fee and build the full tx view.
    {
        QList<SlateV4::Commit> s1Commits;
        const QJsonObject s1CoinbaseMap =
            localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
        for (int i = 0; i < selection.selectedOutputs.size(); ++i) {
            const WalletOutput &inp = selection.selectedOutputs.at(i);
            SlateV4::Commit c;
            c.feature = s1CoinbaseMap.value(inp.commitment).toBool(inp.coinbase) ? 1 : 0;
            c.commitment = inp.commitment;
            // no proof — marks this as an input
            s1Commits.append(c);
        }
        if (!changeCommit.commitment.isEmpty()) {
            s1Commits.append(changeCommit);  // has proof — marks this as an output
        }
        slate.commitments = sortedCompactCommitments(s1Commits);
    }

    slate.metadata.insert(QStringLiteral("crypto_backend"), WalletCryptoBackend::describeBackend());
    slate.metadata.insert(QStringLiteral("crypto_real"), WalletCryptoBackend::supportsRealGrinTransactions());

    // Keep internal metadata locally, but emit a compact, reference-like external S1.
    SlateV4 outboundSlate = slate;
    outboundSlate.metadata = QJsonObject();
    outboundSlate.metadata.insert(QStringLiteral("external_binary"), true);
    outboundSlate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
    outboundSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    if (!localSlatepackAddress.trimmed().isEmpty()) {
        outboundSlate.metadata.insert(QStringLiteral("slatepack_sender"), localSlatepackAddress);
    }
    // Compact S1: no tx elements/proofs in the outbound packet.
    outboundSlate.commitments.clear();
    outboundSlate.hasPaymentProof = false;
    outboundSlate.paymentProof = SlateV4::PaymentProof();

    const QString decoded = QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(
            outboundSlate,
            &armoredSlatepack,
            &writerError,
            localSlatepackAddress,
            QStringList(),
            currentSlatepackSecret())) {
        armoredSlatepack = encodeSlatepackArmor(
            QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented)),
            localSlatepackAddress);
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(slate.workflowId(), slate.modeCode(), slate.stateCode(), armoredSlatepack, decoded);
    setLastInfo(QStringLiteral("SEND workflow started at S1. Share the generated Slatepack with the receiver."));
}

void GrinWalletController::startReceiveWorkflow(const QString &amount, const QString &note)
{
    touchWalletSession();
    SlateV4 slate;
    const QString workflowId = slate.id;
    slate.ver.slateVersion = 4;
    slate.ver.blockHeaderVersion = 3;
    const WalletCryptoBackend::ParticipantContext receiverContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    const quint64 requestedAmount = amountToNanogrin(amount);
    if (requestedAmount == 0) {
        setLastError(QStringLiteral("Enter a valid amount in GRIN, e.g. 1.000000000."));
        return;
    }
    slate.state = SlateV4::Invoice1;
    slate.amount = formatNanogrin(requestedAmount);
    slate.offset = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000");
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiverContext));
    WalletOutput invoiceOutput;
    SlateV4::Commit invoiceCommit;
    QString outputError;
    if (!ensureReceiverOutputContext(
            workflowId,
            slate.amount,
            QStringLiteral("invoice"),
            &invoiceOutput,
            &invoiceCommit,
            &outputError)) {
        setLastError(outputError.isEmpty()
            ? QStringLiteral("Failed to derive invoice output.")
            : outputError);
        return;
    }
    // Include receiver output commit in I1 per reference format.
    // This lets the payer verify the amount and include the output in the binary slate.
    slate.commitments.append(invoiceCommit);

    const QString localSlatepackAddress = currentSlatepackAddress();
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
    qDebug() << "[WorkflowReceiveStart]"
             << "workflowId=" << workflowId
             << "state=" << slate.stateCode()
             << "amount=" << slate.amount
             << "offset=" << slate.offset
             << "version=" << slate.versionCode()
             << "externalBinary=" << slate.metadata.value(QStringLiteral("external_binary")).toBool()
             << "receiverBlindPublic=" << receiverContext.blindPublic
             << "receiverNoncePublic=" << receiverContext.noncePublic
             << "hasPaymentProof=" << slate.hasPaymentProof
             << "commitment=" << invoiceCommit.commitment
             << "proofLen=" << invoiceCommit.proof.length()
             << "childIndex=" << invoiceOutput.childIndex
             << "keyPath=" << invoiceOutput.keyPath;
    // Keep full local invoice context internally, but emit compact external I1.
    SlateV4 outboundSlate = slate;
    outboundSlate.metadata = QJsonObject();
    outboundSlate.metadata.insert(QStringLiteral("external_binary"), true);
    outboundSlate.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
    outboundSlate.metadata.insert(QStringLiteral("workflow_id"), workflowId);
    if (!localSlatepackAddress.trimmed().isEmpty()) {
        outboundSlate.metadata.insert(QStringLiteral("slatepack_sender"), localSlatepackAddress);
    }
    // Compact I1: do not include tx elements/proofs in the outbound packet.
    outboundSlate.commitments.clear();
    outboundSlate.hasPaymentProof = false;
    outboundSlate.paymentProof = SlateV4::PaymentProof();

    const QString decoded = QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented));
    QString armoredSlatepack;
    QString writerError;
    if (!BinarySlateV4Writer::encodeSlatepack(
            outboundSlate,
            &armoredSlatepack,
            &writerError,
            localSlatepackAddress,
            QStringList(),
            currentSlatepackSecret())) {
        armoredSlatepack = encodeSlatepackArmor(
            QString::fromUtf8(QJsonDocument(outboundSlate.toJson()).toJson(QJsonDocument::Indented)),
            localSlatepackAddress);
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

    qDebug() << "[WorkflowProcess] incoming"
             << "inputChars=" << slatepack.trimmed().size()
             << "walletUnlocked=" << m_walletUnlocked
             << "selectedNetwork=" << resolvedNetworkName()
             << "inputSummary=" << summarizeArmoredSlatepack(slatepack);

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
    alignSlateVersionWithNode(&slate);
    const SlateV4 incomingSlate = slate;
    qDebug() << "[WorkflowProcess] decoded"
             << "id=" << slate.id
             << "state=" << slate.stateCode()
             << "mode=" << slate.modeCode()
             << "version=" << slate.versionCode()
             << "amount=" << slate.amount
             << "fee=" << slate.fee
             << "sigCount=" << slate.signatures.size()
             << "commitmentCount=" << slate.commitments.size()
             << "externalBinary=" << slate.metadata.value(QStringLiteral("external_binary")).toBool()
             << "senderMeta=" << slate.metadata.value(QStringLiteral("slatepack_sender")).toString()
             << "recipientMetaCount=" << slate.metadata.value(QStringLiteral("slatepack_recipients")).toArray().size()
             << "networkMeta=" << slate.metadata.value(QStringLiteral("network")).toString();
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
    qDebug() << "[WorkflowProcess] role"
             << "workflowId=" << workflowId
             << "state=" << state
             << "mode=" << mode
             << "localRole=" << localRoleTag
             << "localSlatepackAddress=" << localSlatepackAddress
             << "localPaymentProofAddress=" << localPaymentProofAddress;
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
        if (slate.metadata.value(QStringLiteral("external_binary")).toBool()
            && mode == QStringLiteral("invoice")
            && state == QStringLiteral("I1")) {
            persistWorkflowTransaction(slate, false);
            qDebug() << "[WorkflowSelectionContext] persisted transaction after sender lock"
                     << "workflowId=" << workflowId
                     << "state=" << state;
        }
    }

    WalletCryptoBackend::ParticipantContext signatureOverrideContext;
    WalletCryptoBackend::ParticipantContext *signatureOverride = 0;

    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I1")
        && localRoleTag == QStringLiteral("sender")) {
        if (!prepareInvoiceSenderContext(workflowId, &slate, &signatureOverrideContext, &cryptoError)) {
            setLastError(cryptoError.isEmpty()
                ? QStringLiteral("Failed to prepare invoice sender context.")
                : cryptoError);
            return;
        }
        signatureOverride = &signatureOverrideContext;
    }

    if (localRoleTag == QStringLiteral("receiver")
        && (slate.commitments.isEmpty()
            || state == QStringLiteral("S1")
            || (mode == QStringLiteral("invoice") && state == QStringLiteral("I2")))) {
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

        bool hasReceiverCommit = false;
        for (int i = 0; i < slate.commitments.size(); ++i) {
            if (slate.commitments.at(i).commitment == receiveCommit.commitment) {
                hasReceiverCommit = true;
                break;
            }
        }
        if (!hasReceiverCommit) {
            slate.commitments.append(receiveCommit);
            slate.commitments = sortedCompactCommitments(slate.commitments);
        }
        slate.metadata.insert(QStringLiteral("receiver_blind"), receiveOutput.blindingFactor);
        slate.metadata.insert(QStringLiteral("receiver_child_index"), static_cast<int>(receiveOutput.childIndex));
        slate.metadata.insert(QStringLiteral("receiver_key_path"), receiveOutput.keyPath);

        if (mode == QStringLiteral("invoice") && state == QStringLiteral("I2")) {
            const WalletCryptoBackend::ParticipantContext receiverContext =
                WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
            if (!receiveOutput.blindingFactor.isEmpty() && !receiverContext.blindSecret.isEmpty()) {
                QString offsetError;
                const QString adjustedOffset = WalletCryptoBackend::combineBlindingFactors(
                    QStringList() << slate.offset << receiveOutput.blindingFactor,
                    QStringList() << receiverContext.blindSecret,
                    &offsetError);
                if (!adjustedOffset.isEmpty()) {
                    slate.offset = adjustedOffset;
                    qDebug() << "[WorkflowSign] invoice I2 receiver offset adjusted"
                             << "workflowId=" << workflowId
                             << "offset=" << slate.offset
                             << "receiverBlind=" << receiveOutput.blindingFactor.left(16)
                             << "receiverPub=" << receiverContext.blindPublic;
                } else {
                    qDebug() << "[WorkflowSign] invoice I2 receiver offset adjust failed"
                             << "workflowId=" << workflowId
                             << "error=" << offsetError;
                }
            }
        }
    }

    if (!WalletCryptoBackend::applyRound2Signature(&slate, m_seedFingerprint, localRoleTag, signatureOverride, &cryptoError)) {
        setLastError(cryptoError.isEmpty() ? QStringLiteral("Failed to apply round 2 signature.") : cryptoError);
        return;
    }
    qDebug() << "[WorkflowSign] after round2"
             << "workflowId=" << workflowId
             << "state=" << state
             << "role=" << localRoleTag
             << "sigCount=" << slate.signatures.size()
             << "offset=" << slate.offset
             << "messageHash=" << slate.metadata.value(QStringLiteral("message_hash")).toString()
             << "pubkeyTotal=" << slate.metadata.value(QStringLiteral("pubkey_total")).toString()
             << "pubnonceTotal=" << slate.metadata.value(QStringLiteral("pubnonce_total")).toString();
    for (int i = 0; i < slate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate.signatures.at(i);
        qDebug() << "[WorkflowSign] participant"
                 << i
                 << "xs=" << sig.xs
                 << "nonce=" << sig.nonce
                 << "partLen=" << sig.part.length();
    }

    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I2")
        && localRoleTag == QStringLiteral("receiver")
        && slate.numParticipants < slate.signatures.size()) {
        slate.numParticipants = slate.signatures.size();
        qDebug() << "[WorkflowSign] normalized invoice I3 participants"
                 << "workflowId=" << workflowId
                 << "participantCount=" << slate.numParticipants
                 << "signatureCount=" << slate.signatures.size();
    }

    if (mode == QStringLiteral("invoice")
        && state == QStringLiteral("I1")
        && localRoleTag == QStringLiteral("sender")) {
        if (slate.metadata.value(QStringLiteral("external_binary")).toBool()) {
            slate.metadata.remove(QStringLiteral("tx_skeleton"));
            slate.metadata.remove(QStringLiteral("tx_build_error"));
            slate.metadata.insert(QStringLiteral("tx_ready"), false);
            qDebug() << "[WorkflowSign] invoice I2 tx skeleton skipped"
                     << "workflowId=" << workflowId
                     << "reason=" << "external-binary-invoice";
        } else {
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
                // Find the receiver output: first commit WITH a proof that is not the change output.
                for (int ci = 0; ci < slate.commitments.size(); ++ci) {
                    const SlateV4::Commit &c = slate.commitments.at(ci);
                    if (!c.proof.trimmed().isEmpty() && c.commitment != localContext.value(QStringLiteral("change_commit")).toString()) {
                        receiverOutput.commitment = c.commitment;
                        receiverOutput.proof = c.proof;
                        receiverOutput.amount = slate.amount;
                        break;
                    }
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
                    slate.metadata.insert(QStringLiteral("tx_ready"), false);
                    slate.metadata.remove(QStringLiteral("tx_build_error"));
                    qDebug() << "[WorkflowSign] invoice I2 tx skeleton built"
                             << "workflowId=" << workflowId
                             << "selectedInputCount=" << selectedInputs.size()
                             << "hasChange=" << !changeOutput.commitment.isEmpty();
                } else {
                    slate.metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
                    qDebug() << "[WorkflowSign] invoice I2 tx skeleton failed"
                             << "workflowId=" << workflowId
                             << "error=" << txBuild.error;
                }
            }
        }
        compactInvoiceSlateForReturn(workflowId, &slate);
    }

    if (mode == QStringLiteral("send")
        && state == QStringLiteral("S1")
        && localRoleTag == QStringLiteral("receiver")) {
        compactStandardSlateForReturn(workflowId, &slate);
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
    const bool compactExternalInvoiceI2 =
        externalBinary && nextState == QStringLiteral("I2") && mode == QStringLiteral("invoice");
    if (!compactExternalInvoiceI2) {
        slate.metadata.insert(QStringLiteral("processed_by"), m_walletName);
        slate.metadata.insert(QStringLiteral("processed_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    }

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
            // Find the receiver output: first commit WITH a proof that is not the change output.
            {
                const QString knownChangeCommit = localContext.value(QStringLiteral("change_commit")).toString();
                for (int ci = 0; ci < slate.commitments.size(); ++ci) {
                    const SlateV4::Commit &c = slate.commitments.at(ci);
                    if (!c.proof.trimmed().isEmpty() && c.commitment != knownChangeCommit) {
                        receiverOutput.commitment = c.commitment;
                        receiverOutput.proof = c.proof;
                        receiverOutput.amount = slate.amount;
                        break;
                    }
                }
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
        } else if (externalBinary && nextState == QStringLiteral("I3")) {
            WalletOutput receiverOutput;
            SlateV4::Commit receiverCommit;
            QString receiverError;
            const QJsonObject localContext = workflowContext(workflowId);
            const QString receiverAmount =
                localContext.value(QStringLiteral("receiver_amount_display")).toString();
            if (!receiverAmount.trimmed().isEmpty()) {
                if (!ensureReceiverOutputContext(workflowId,
                                                 receiverAmount,
                                                 QStringLiteral("invoice"),
                                                 &receiverOutput,
                                                 &receiverCommit,
                                                 &receiverError)) {
                    qDebug() << "[WorkflowSign] external invoice I3 receiver output restore failed"
                             << "workflowId=" << workflowId
                             << "error=" << receiverError;
                }
            }
            const WalletTxBuilder::BuildResult txBuild =
                WalletTxBuilder::buildTransactionSkeletonFromCommitments(
                    slate,
                    receiverOutput.commitment.isEmpty() ? 0 : &receiverOutput);
            if (txBuild.success) {
                slate.metadata.insert(QStringLiteral("tx_skeleton"), txBuild.transaction.toJson());
                slate.metadata.insert(QStringLiteral("tx_ready"), true);
                slate.metadata.remove(QStringLiteral("tx_build_error"));
                qDebug() << "[WorkflowSign] external invoice I3 tx skeleton built"
                         << "workflowId=" << workflowId
                         << "commitmentCount=" << slate.commitments.size()
                         << "hasReceiverOutput=" << !receiverOutput.commitment.isEmpty()
                         << "outputCount="
                         << txBuild.transaction.body().outputs().size();
            } else {
                slate.metadata.insert(QStringLiteral("tx_build_error"), txBuild.error);
                qDebug() << "[WorkflowSign] external invoice I3 tx skeleton failed"
                         << "workflowId=" << workflowId
                         << "error=" << txBuild.error;
            }
        }
    }

    QString updatedSlatepack;
    const QStringList outgoingRecipients = outgoingSlatepackRecipients(slate);
    const QString outgoingSender = localSlatepackAddress;
    if (!outgoingSender.trimmed().isEmpty()) {
        slate.metadata.insert(QStringLiteral("slatepack_sender"), outgoingSender);
    }
    const QString updatedDecoded = QString::fromUtf8(QJsonDocument(slate.toJson()).toJson(QJsonDocument::Indented));
    qDebug() << "[SlatepackEncode]"
             << "workflowId=" << workflowId
             << "state=" << nextState
             << "externalBinary=" << externalBinary
             << "senderMetadata=" << slate.metadata.value(QStringLiteral("slatepack_sender")).toString()
             << "localSender=" << localSlatepackAddress
             << "recipientCount=" << outgoingRecipients.size()
             << "recipientList=" << outgoingRecipients
             << "outgoingSender=" << outgoingSender;
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
    qDebug() << "[SlatepackEncode] result"
             << "workflowId=" << workflowId
             << "state=" << nextState
             << "outputChars=" << updatedSlatepack.size()
             << "outputSummary=" << summarizeArmoredSlatepack(updatedSlatepack);
    if (incomingSlate.metadata.value(QStringLiteral("external_binary")).toBool()
        && incomingSlate.modeCode() == QStringLiteral("invoice")
        && incomingSlate.stateCode() == QStringLiteral("I1")
        && nextState == QStringLiteral("I2")) {
        runExternalInvoicePreflight(incomingSlate, slate, updatedSlatepack);
    }
    persistWorkflowTransaction(slate, false);
    setWorkflow(workflowId, mode, nextState, updatedSlatepack, updatedDecoded);

    const bool autoBroadcastExternalInvoice =
        externalBinary
        && nextState == QStringLiteral("I3")
        && slate.metadata.value(QStringLiteral("tx_ready")).toBool()
        && slate.metadata.value(QStringLiteral("tx_skeleton")).isObject();
    if (autoBroadcastExternalInvoice) {
        qDebug() << "[WorkflowBroadcast] auto broadcast external invoice I3"
                 << "workflowId=" << workflowId
                 << "txReady=" << slate.metadata.value(QStringLiteral("tx_ready")).toBool()
                 << "skeletonOutputs="
                 << slate.metadata.value(QStringLiteral("tx_skeleton")).toObject()
                        .value(QStringLiteral("body")).toObject()
                        .value(QStringLiteral("outputs")).toArray().size();
        broadcastCurrentWorkflowTransaction();
        if (!m_pendingBroadcastWorkflowId.isEmpty()) {
            setLastInfo(QStringLiteral("Workflow %1 advanced to %2 and is being broadcast to the node.")
                            .arg(workflowId, nextState));
            return;
        }
    }

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

void GrinWalletController::cleanupLocalAndCancelledItems()
{
    touchWalletSession();
    
    if (!m_walletUnlocked || m_sessionMnemonic.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Unlock the wallet before cleaning up."));
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    
    // Entferne lokale Outputs (noch nicht on-chain)
    int localCount = 0;
    for (int i = outputs.size() - 1; i >= 0; --i) {
        if (!outputs.at(i).onChain) {
            outputs.removeAt(i);
            ++localCount;
        }
    }
    
    // Entferne abgebrochene Transaktionen
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
    walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
    walletState.insert(QStringLiteral("transactions"), transactions);
    document.insert(QStringLiteral("wallet_state"), walletState);
    saveDocument(document);
    refreshStateFromStorage();

    QString infoMsg = QStringLiteral("Cleanup completed: %1 local output(s) and %2 cancelled transaction(s) removed.")
                          .arg(QString::number(localCount))
                          .arg(QString::number(cancelledCount));
    setLastInfo(infoMsg);
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
    
    // Ermittle die ursprünglichen Inputs aus dem Workflow-Kontext
    const QJsonObject localContext = workflowContext(workflowId);
    const QJsonArray selectedInputCommits = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    
    // Konvertiere Selected Input Commits zu QStringList für schnelleren Lookup
    QStringList selectedInputCommitments;
    for (int j = 0; j < selectedInputCommits.size(); ++j) {
        selectedInputCommitments.append(selectedInputCommits.at(j).toString());
    }
    
    // Verarbeite alle Outputs mit diesem Workflow ID
    for (int i = 0; i < outputs.size(); ++i) {
        if (outputs[i].workflowId != workflowId) {
            continue;
        }

        if (outputs[i].spent) {
            continue;
        }

        // Überprüfe, ob dies ein Input (UTXO) ist, das ursprünglich von dieser Transaktion verwendet wurde
        if (selectedInputCommitments.contains(outputs[i].commitment)) {
            // Dies ist ein Input - gebe ihn frei.
            // workflowId NICHT löschen: sie zeigt auf die ursprüngliche Receive/Invoice-Tx,
            // die diesen UTXO erstellt hat, und wird für die Height-Inferenz benötigt.
            outputs[i].locked = false;
            outputs[i].pending = false;
        } else {
            // Dies ist ein neu erstellter Output (Change, Receive, Invoice, etc.)
            // der nicht auf die Blockchain ging - entferne ihn
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
    setLastInfo(QStringLiteral("Transaction %1 cancelled and UTXOs released.").arg(workflowId));
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
    return keychain.isValid() ? WalletCryptoBackend::slatepackAddress(keychain, m_selectedNetwork) : QString();
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

    qDebug() << "[SlatepackRecipients]"
             << "workflowId=" << slate.workflowId()
             << "state=" << slate.stateCode()
             << "localAddress=" << localAddress
             << "senderMeta=" << sender
             << "explicitRecipientCount=" << explicitRecipients.size()
             << "resolvedRecipients=" << recipients;
    return recipients;
}

void GrinWalletController::alignSlateVersionWithNode(SlateV4 *slate) const
{
    if (!slate) {
        return;
    }

    if (slate->ver.slateVersion <= 0) {
        slate->ver.slateVersion = 4;
    }

    const bool preserveIncomingInvoiceVersion =
        slate->metadata.value(QStringLiteral("external_binary")).toBool()
        && slate->modeCode() == QStringLiteral("invoice");
    if (preserveIncomingInvoiceVersion) {
        return;
    }

    if (m_nodeBlockHeaderVersion > 0 && slate->ver.blockHeaderVersion != m_nodeBlockHeaderVersion) {
        qDebug() << "[WorkflowVersion] alignSlate"
                 << "workflowId=" << slate->workflowId()
                 << "state=" << slate->stateCode()
                 << "from=" << slate->ver.blockHeaderVersion
                 << "to=" << m_nodeBlockHeaderVersion;
        slate->ver.blockHeaderVersion = m_nodeBlockHeaderVersion;
    }
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
    m_autoLockOnAppDeactivate = document
        .value(QLatin1String(kAppSettingsKey))
        .toObject()
        .value(QLatin1String(kAutoLockOnDeactivateKey))
        .toBool(false);

    emit walletChanged();
    emit nodeConfigChanged();
    refreshStateFromStorage();
}

void GrinWalletController::setAutoLockOnAppDeactivate(bool enabled)
{
    if (m_autoLockOnAppDeactivate == enabled) {
        return;
    }

    QJsonObject document = loadDocument();
    QJsonObject appSettings = document.value(QLatin1String(kAppSettingsKey)).toObject();
    appSettings.insert(QLatin1String(kAutoLockOnDeactivateKey), enabled);
    document.insert(QLatin1String(kAppSettingsKey), appSettings);
    if (!saveDocument(document)) {
        setLastError(QStringLiteral("Failed to persist app settings."));
        return;
    }

    m_autoLockOnAppDeactivate = enabled;
    emit statusChanged();
    setLastError(QString());
    setLastInfo(enabled
        ? QStringLiteral("Automatic wallet lock on app deactivation enabled.")
        : QStringLiteral("Automatic wallet lock on app deactivation disabled."));
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
    qDebug() << "[WorkflowState]"
             << "id=" << id
             << "mode=" << mode
             << "state=" << state
             << "slatepackChars=" << slatepack.size()
             << "decodedChars=" << decoded.size()
             << "slatepackSummary=" << summarizeArmoredSlatepack(slatepack);
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

    connect(m_nodeApi, &NodeForeignApi::getVersionFinished, this, [this](const Result<NodeVersion> &result) {
        if (result.hasError()) {
            return;
        }

        const quint64 blockHeaderVersion = result.value().blockHeaderVersion();
        if (blockHeaderVersion > 0 && blockHeaderVersion < 256) {
            m_nodeBlockHeaderVersion = static_cast<int>(blockHeaderVersion);
            qDebug() << "[WorkflowVersion] activeNodeVersion"
                     << "network=" << resolvedNetworkName()
                     << "blockHeaderVersion=" << m_nodeBlockHeaderVersion;
        }
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
        const QList<OutputPrintable> chainOutputs = result.value();

        QSet<QString> trackedCommitments;
        for (int i = 0; i < tracked.size(); ++i) {
            trackedCommitments.insert(tracked.at(i).commitment);
        }

        int matchedCommitments = 0;
        for (int i = 0; i < chainOutputs.size(); ++i) {
            const QString commitHex = chainOutputs.at(i).commit().hex();
            if (trackedCommitments.contains(commitHex)) {
                ++matchedCommitments;
            }
        }

        // If node returns no usable data for tracked commitments, fall back to seed scan from leaf 1.
        // This self-heals stale commitment sets without requiring manual full rescan.
        if (!tracked.isEmpty() && (chainOutputs.isEmpty() || matchedCommitments == 0)) {
            walletState.insert(QStringLiteral("restore_leaf_index"), 0);
            document.insert(QStringLiteral("wallet_state"), walletState);
            saveDocument(document);
            m_walletScanInFlight = false;
            setLastInfo(QStringLiteral("Node returned no tracked outputs. Falling back to seed scan from leaf 1."));
            startSeedScan();
            return;
        }

        tracked = WalletScanner::reconcileTrackedOutputs(tracked, chainOutputs);

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
    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    const QJsonObject storedBalances = walletState.value(QStringLiteral("balances")).toObject();
    m_scanHeight = static_cast<qulonglong>(walletState.value(QStringLiteral("scan_height")).toInt());

    // Always recompute balances from outputs so UI does not depend on stale persisted values.
    const qulonglong effectiveHeight = m_chainHeight > 0 ? m_chainHeight : m_scanHeight;
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);

    const QJsonObject recalculatedBalances = WalletScanner::balancesFromOutputs(outputs, effectiveHeight);
    const QJsonObject balances = recalculatedBalances.isEmpty() ? storedBalances : recalculatedBalances;

    if (balances != storedBalances) {
        walletState.insert(QStringLiteral("balances"), balances);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
    }

    m_totalBalance = amountStringFromJson(balances, QStringLiteral("total"));
    m_spendableBalance = amountStringFromJson(balances, QStringLiteral("spendable"));
    m_lockedBalance = amountStringFromJson(balances, QStringLiteral("locked"));
    m_immatureBalance = amountStringFromJson(balances, QStringLiteral("immature"));
    m_awaitingConfirmationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_confirmation"));
    m_awaitingFinalizationBalance = amountStringFromJson(balances, QStringLiteral("awaiting_finalization"));
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
        if (!m_autoLockOnAppDeactivate) {
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
        const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
        const QJsonArray transactions = walletState.value(QStringLiteral("transactions")).toArray();
        bool hasTransactionEntry = false;
        for (int i = 0; i < transactions.size(); ++i) {
            if (transactions.at(i).toObject().value(QStringLiteral("workflow_id")).toString() == workflowId) {
                hasTransactionEntry = true;
                break;
            }
        }
        const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
        const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
        bool hasMatchingLocks = false;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const WalletOutput output =
                findTrackedOutputByCommitment(outputs, selectedCommitments.at(i).toString());
            if (!output.commitment.isEmpty() && output.locked && output.workflowId == workflowId) {
                hasMatchingLocks = true;
                break;
            }
        }

        if (!hasMatchingLocks) {
            qDebug() << "[WorkflowSelectionContext] stale selection discarded"
                     << "workflowId=" << workflowId
                     << "hasTransactionEntry=" << hasTransactionEntry
                     << "hasMatchingLocks=" << hasMatchingLocks
                     << "selectedInputs=" << selectedCommitments.size();
            localContext.remove(QStringLiteral("selected_inputs"));
            localContext.remove(QStringLiteral("selected_total"));
            localContext.remove(QStringLiteral("selected_input_commits"));
            localContext.remove(QStringLiteral("selected_input_coinbase"));
            localContext.remove(QStringLiteral("change_amount"));
            localContext.remove(QStringLiteral("amount_nano"));
            localContext.remove(QStringLiteral("amount_display"));
            localContext.remove(QStringLiteral("fee_nano"));
            localContext.remove(QStringLiteral("fee_amount_display"));
            localContext.remove(QStringLiteral("change_commit"));
            localContext.remove(QStringLiteral("change_proof"));
            localContext.remove(QStringLiteral("change_amount_display"));
            localContext.remove(QStringLiteral("change_child_index"));
            localContext.remove(QStringLiteral("change_key_path"));
            storeWorkflowContext(workflowId, localContext);
        } else {
        if (!hasTransactionEntry) {
            qDebug() << "[WorkflowSelectionContext] existing lock without transaction entry"
                     << "workflowId=" << workflowId
                     << "selectedInputs=" << selectedCommitments.size();
            SlateV4 placeholder;
            placeholder.id = workflowId;
            placeholder.metadata.insert(QStringLiteral("workflow_id"), workflowId);
            placeholder.metadata.insert(QStringLiteral("workflow"), QStringLiteral("external-grin-slatepack"));
            placeholder.metadata.insert(QStringLiteral("network"), resolvedNetworkName());
            placeholder.setStateFromCode(QStringLiteral("I1"));
            placeholder.amount = localContext.value(QStringLiteral("amount_display")).toString();
            placeholder.fee = localContext.value(QStringLiteral("fee_amount_display")).toString();
            placeholder.offset = localContext.value(QStringLiteral("offset")).toString();
            persistWorkflowTransaction(placeholder, false);
        }
        QJsonObject selectedInputCoinbase;
        quint64 selectedTotal = 0;
        for (int i = 0; i < selectedCommitments.size(); ++i) {
            const QString commitment = selectedCommitments.at(i).toString();
            const WalletOutput output = findTrackedOutputByCommitment(outputs, commitment);
            if (!output.commitment.isEmpty()) {
                selectedInputCoinbase.insert(output.commitment, output.coinbase);
                selectedTotal += amountToNanogrin(output.amount);
                qDebug() << "[WorkflowSelectionContext] existing input"
                         << i
                         << "commitment=" << commitment.left(16)
                         << "walletCoinbase=" << output.coinbase;
            } else {
                const QJsonValue persistedValue =
                    localContext.value(QStringLiteral("selected_input_coinbase")).toObject().value(commitment);
                if (!persistedValue.isUndefined()) {
                    selectedInputCoinbase.insert(commitment, persistedValue.toBool());
                    qDebug() << "[WorkflowSelectionContext] existing input fallback"
                             << i
                             << "commitment=" << commitment.left(16)
                             << "persistedCoinbase=" << persistedValue.toBool();
                } else {
                    qDebug() << "[WorkflowSelectionContext] existing input missing"
                             << i
                             << "commitment=" << commitment.left(16);
                }
            }
        }
        if (!selectedInputCoinbase.isEmpty()
            && selectedInputCoinbase != localContext.value(QStringLiteral("selected_input_coinbase")).toObject()) {
            localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
            storeWorkflowContext(workflowId, localContext);
        }
        if (!localContext.value(invoiceContextKey(QStringLiteral("participant"))).isObject()) {
            const QJsonObject legacyParticipant = [&localContext]() {
                QJsonObject json;
                const QString secKey = localContext.value(QStringLiteral("invoice_sender_blind_secret")).toString();
                const QString secNonce = localContext.value(QStringLiteral("invoice_sender_nonce_secret")).toString();
                const QString pubKey = localContext.value(QStringLiteral("invoice_sender_blind_public")).toString();
                const QString pubNonce = localContext.value(QStringLiteral("invoice_sender_nonce_public")).toString();
                if (secKey.isEmpty() || secNonce.isEmpty() || pubKey.isEmpty() || pubNonce.isEmpty()) {
                    return QJsonObject();
                }
                json.insert(QStringLiteral("sec_key"), secKey);
                json.insert(QStringLiteral("sec_nonce"), secNonce);
                json.insert(QStringLiteral("pub_key"), pubKey);
                json.insert(QStringLiteral("pub_nonce"), pubNonce);
                return json;
            }();
            if (!legacyParticipant.isEmpty()) {
                localContext.insert(invoiceContextKey(QStringLiteral("participant")), legacyParticipant);
                localContext.remove(QStringLiteral("invoice_sender_blind_secret"));
                localContext.remove(QStringLiteral("invoice_sender_nonce_secret"));
                localContext.remove(QStringLiteral("invoice_sender_blind_public"));
                localContext.remove(QStringLiteral("invoice_sender_nonce_public"));
                storeWorkflowContext(workflowId, localContext);
            }
        }
        qDebug() << "[WorkflowSelectionContext] existing selection"
                 << "workflowId=" << workflowId
                 << "selectedInputs=" << selectedCommitments.size()
                 << "coinbaseMapSize=" << selectedInputCoinbase.size();
        const quint64 amountNano = localContext.value(QStringLiteral("amount_nano")).toVariant().toULongLong();
        const quint64 feeWithoutChange =
            WalletSelection::estimateFee(selectedCommitments.size(), 1, 1);
        const quint64 feeWithChange =
            WalletSelection::estimateFee(selectedCommitments.size(), 2, 1);
        const bool exactNoChange = (selectedTotal == amountNano + feeWithoutChange);
        const quint64 recalculatedFee = exactNoChange ? feeWithoutChange : feeWithChange;
        const quint64 recalculatedChange =
            (selectedTotal >= amountNano + recalculatedFee)
                ? (selectedTotal - amountNano - recalculatedFee)
                : 0;
        const quint64 persistedFee = localContext.value(QStringLiteral("fee_nano")).toVariant().toULongLong();
        const QString existingChangeAmountDisplay = localContext.value(QStringLiteral("change_amount_display")).toString();
        const QString recalculatedChangeDisplay =
            recalculatedChange > 0 ? formatNanogrin(recalculatedChange) : QString();
        if (persistedFee != recalculatedFee
            || localContext.value(QStringLiteral("selected_total")).toString() != QString::number(selectedTotal)
            || localContext.value(QStringLiteral("change_amount")).toString() != QString::number(recalculatedChange)
            || existingChangeAmountDisplay != recalculatedChangeDisplay) {
            localContext.insert(QStringLiteral("selected_total"), QString::number(selectedTotal));
            localContext.insert(QStringLiteral("fee_nano"), QString::number(recalculatedFee));
            localContext.insert(QStringLiteral("fee_amount_display"), formatNanogrin(recalculatedFee));
            localContext.insert(QStringLiteral("change_amount"), QString::number(recalculatedChange));
            if (recalculatedChange > 0) {
                WalletOutput changeOutput;
                SlateV4::Commit changeCommit;
                QString outputError;
                if (buildOwnedOutput(QStringLiteral("change"),
                                     recalculatedChangeDisplay,
                                     &changeOutput,
                                     &changeCommit,
                                     &outputError)) {
                    changeOutput.workflowId = workflowId;
                    storeOwnedOutput(changeOutput);
                    localContext.insert(QStringLiteral("change_commit"), changeCommit.commitment);
                    localContext.insert(QStringLiteral("change_proof"), changeCommit.proof);
                    localContext.insert(QStringLiteral("change_amount_display"), recalculatedChangeDisplay);
                    localContext.insert(QStringLiteral("change_child_index"), static_cast<int>(changeOutput.childIndex));
                    localContext.insert(QStringLiteral("change_key_path"), changeOutput.keyPath);
                } else if (errorOut) {
                    *errorOut = outputError.isEmpty()
                        ? QStringLiteral("Failed to rebuild change output.")
                        : outputError;
                    return false;
                }
            } else {
                localContext.remove(QStringLiteral("change_commit"));
                localContext.remove(QStringLiteral("change_proof"));
                localContext.remove(QStringLiteral("change_amount_display"));
                localContext.remove(QStringLiteral("change_child_index"));
                localContext.remove(QStringLiteral("change_key_path"));
            }
            storeWorkflowContext(workflowId, localContext);
            qDebug() << "[WorkflowSelectionContext] existing fee recalculated"
                     << "workflowId=" << workflowId
                     << "selectedTotal=" << selectedTotal
                     << "fee=" << recalculatedFee
                     << "change=" << recalculatedChange;
        }
        if (feeOut) {
            *feeOut = recalculatedFee > 0
                ? formatNanogrin(recalculatedFee)
                : localContext.value(QStringLiteral("fee_amount_display")).toString();
        }
        return true;
        }
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
    QJsonObject selectedInputCoinbase;
    for (int i = 0; i < outputs.size(); ++i) {
        for (int j = 0; j < selection.selectedOutputs.size(); ++j) {
            if (outputs.at(i).commitment != selection.selectedOutputs.at(j).commitment) {
                continue;
            }

            outputs[i].locked = true;
            outputs[i].pending = false;
            // Preserve the UTXO's original workflowId (e.g. from the receive/invoice
            // that created it). The send workflow tracks inputs via selected_input_commits.
            selectedCommitments.append(outputs.at(i).commitment);
            selectedInputCoinbase.insert(outputs.at(i).commitment, outputs.at(i).coinbase);
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
    localContext.insert(QStringLiteral("selected_input_coinbase"), selectedInputCoinbase);
    localContext.insert(QStringLiteral("change_amount"), QString::number(selection.change));
    localContext.insert(QStringLiteral("amount_nano"), QString::number(requestedAmount));
    localContext.insert(QStringLiteral("amount_display"), amount.trimmed());
    localContext.insert(QStringLiteral("fee_nano"), QString::number(selection.fee));
    localContext.insert(QStringLiteral("fee_amount_display"), formatNanogrin(selection.fee));
    if (!localContext.value(invoiceContextKey(QStringLiteral("participant"))).isObject()) {
        localContext.insert(invoiceContextKey(QStringLiteral("participant")),
                            participantContextToJson(
                                WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"))));
    }

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

bool GrinWalletController::prepareInvoiceSenderContext(
    const QString &workflowId,
    SlateV4 *slate,
    WalletCryptoBackend::ParticipantContext *signatureOverrideOut,
    QString *errorOut)
{
    if (!slate || !signatureOverrideOut) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invoice sender context target is missing.");
        }
        return false;
    }

    QJsonObject localContext = workflowContext(workflowId);
    WalletCryptoBackend::ParticipantContext senderAggsig =
        participantContextFromJson(localContext.value(invoiceContextKey(QStringLiteral("participant"))).toObject(),
                                   QStringLiteral("sender"));
    if (senderAggsig.blindSecret.isEmpty()
        || senderAggsig.nonceSecret.isEmpty()
        || senderAggsig.blindPublic.isEmpty()
        || senderAggsig.noncePublic.isEmpty()) {
        senderAggsig = WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
        localContext.insert(invoiceContextKey(QStringLiteral("participant")),
                            participantContextToJson(senderAggsig));
        storeWorkflowContext(workflowId, localContext);
    }

    const QJsonArray selectedCommitments = localContext.value(QStringLiteral("selected_input_commits")).toArray();
    const QJsonObject selectedInputCoinbase =
        localContext.value(QStringLiteral("selected_input_coinbase")).toObject();
    const QJsonObject walletState = loadDocument().value(QStringLiteral("wallet_state")).toObject();
    QList<WalletOutput> trackedOutputs = WalletScanner::outputsFromState(walletState);
    if (!m_sessionMnemonic.trimmed().isEmpty()) {
        const WalletKeychain keychain(m_sessionMnemonic);
        if (keychain.isValid()) {
            for (int i = 0; i < trackedOutputs.size(); ++i) {
                trackedOutputs[i] = normalizedTrackedOutput(trackedOutputs.at(i), keychain);
            }
        }
    }

    QStringList positiveBlinds;
    const QString priorOffset = slate->offset.trimmed();
    if (!priorOffset.isEmpty() && priorOffset != QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000")) {
        positiveBlinds.append(priorOffset);
    }
    const QString changeCommit = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommit.isEmpty()) {
        const WalletOutput changeOutput = findTrackedOutputByCommitment(trackedOutputs, changeCommit);
        if (!changeOutput.blindingFactor.isEmpty()) {
            positiveBlinds.append(changeOutput.blindingFactor);
        }
    }

    QStringList negativeBlinds;
    negativeBlinds.append(senderAggsig.blindSecret);
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (!input.blindingFactor.isEmpty()) {
            negativeBlinds.append(input.blindingFactor);
        }
    }

    QString cryptoError;
    const QString adjustedOffset = WalletCryptoBackend::combineBlindingFactors(
        positiveBlinds,
        negativeBlinds,
        &cryptoError);
    if (adjustedOffset.isEmpty()) {
        if (errorOut) {
            *errorOut = cryptoError.isEmpty()
                ? QStringLiteral("Failed to derive invoice sender offset.")
                : cryptoError;
        }
        return false;
    }

    slate->offset = adjustedOffset;
    *signatureOverrideOut = senderAggsig;
    qDebug() << "[InvoiceSenderOffset]"
             << "workflowId=" << workflowId
             << "selectedInputs=" << selectedCommitments.size()
             << "positiveCount=" << positiveBlinds.size()
             << "negativeCount=" << negativeBlinds.size()
             << "adjustedOffset=" << adjustedOffset
             << "blindPublic=" << senderAggsig.blindPublic;

    QList<SlateV4::Commit> rebuiltCommitments = slate->commitments;
    for (int i = 0; i < selectedCommitments.size(); ++i) {
        const WalletOutput input = findTrackedOutputByCommitment(trackedOutputs, selectedCommitments.at(i).toString());
        if (input.commitment.isEmpty()) {
            qDebug() << "[InvoiceSenderComs] input missing from tracked outputs"
                     << "commitment=" << selectedCommitments.at(i).toString().left(16);
            continue;
        }
        qDebug() << "[InvoiceSenderInput]"
                 << "commitment=" << input.commitment.left(16)
                 << "amount=" << input.amount
                 << "source=" << input.source
                 << "coinbase=" << input.coinbase
                 << "onChain=" << input.onChain
                 << "spent=" << input.spent
                 << "locked=" << input.locked
                 << "pending=" << input.pending
                 << "height=" << input.height
                 << "childIndex=" << input.childIndex
                 << "keyPath=" << input.keyPath
                 << "workflowId=" << input.workflowId;

        bool exists = false;
        for (int j = 0; j < rebuiltCommitments.size(); ++j) {
            if (rebuiltCommitments.at(j).commitment == input.commitment) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        SlateV4::Commit commit;
        const bool inputCoinbase =
            selectedInputCoinbase.value(input.commitment).toBool(input.coinbase);
        qDebug() << "[InvoiceSenderComs] source"
                 << "commitment=" << input.commitment.left(16)
                 << "walletCoinbase=" << input.coinbase
                 << "mappedCoinbase=" << inputCoinbase
                 << "mapDefined=" << selectedInputCoinbase.contains(input.commitment);
        commit.feature = inputCoinbase ? 1 : 0;
        commit.commitment = input.commitment;
        rebuiltCommitments.append(commit);
    }

    const QString changeCommitment = localContext.value(QStringLiteral("change_commit")).toString();
    if (!changeCommitment.isEmpty()) {
        const WalletOutput changeOutput = findTrackedOutputByCommitment(trackedOutputs, changeCommitment);
        if (!changeOutput.commitment.isEmpty()) {
            qDebug() << "[InvoiceSenderChange]"
                     << "commitment=" << changeOutput.commitment.left(16)
                     << "amount=" << changeOutput.amount
                     << "source=" << changeOutput.source
                     << "coinbase=" << changeOutput.coinbase
                     << "onChain=" << changeOutput.onChain
                     << "spent=" << changeOutput.spent
                     << "locked=" << changeOutput.locked
                     << "pending=" << changeOutput.pending
                     << "height=" << changeOutput.height
                     << "childIndex=" << changeOutput.childIndex
                     << "keyPath=" << changeOutput.keyPath
                     << "workflowId=" << changeOutput.workflowId;
            bool exists = false;
            for (int i = 0; i < rebuiltCommitments.size(); ++i) {
                if (rebuiltCommitments.at(i).commitment == changeOutput.commitment) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                SlateV4::Commit commit;
                commit.feature = 0;
                commit.commitment = changeOutput.commitment;
                commit.proof = changeOutput.proof;
                rebuiltCommitments.append(commit);
            }
        }
    }

    slate->commitments = sortedCompactCommitments(rebuiltCommitments);
    qDebug() << "[InvoiceSenderComs]"
             << "workflowId=" << workflowId
             << "commitmentCount=" << slate->commitments.size();
    for (int i = 0; i < slate->commitments.size(); ++i) {
        const SlateV4::Commit &commit = slate->commitments.at(i);
        qDebug() << "[InvoiceSenderComs] commit"
                 << i
                 << "feature=" << commit.feature
                 << "hasProof=" << !commit.proof.isEmpty()
                 << "commitment=" << commit.commitment.left(16);
    }

    return true;
}

void GrinWalletController::compactInvoiceSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    WalletCryptoBackend::ParticipantContext senderContext = participantContextFromJson(
        workflowContext(workflowId).value(invoiceContextKey(QStringLiteral("participant"))).toObject(),
        QStringLiteral("sender"));
    if (senderContext.blindPublic.isEmpty() || senderContext.noncePublic.isEmpty()) {
        senderContext = WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("sender"));
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == senderContext.blindPublic && sig.nonce == senderContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }
    slate->numParticipants = 2;
    slate->amount.clear();
    slate->metadata.remove(QStringLiteral("message_hash"));
    slate->metadata.remove(QStringLiteral("pubkey_total"));
    slate->metadata.remove(QStringLiteral("pubnonce_total"));
    slate->metadata.remove(QStringLiteral("signature_status"));
    slate->metadata.remove(QStringLiteral("processed_by"));
    slate->metadata.remove(QStringLiteral("processed_at"));
    slate->metadata.remove(QStringLiteral("tx_ready"));
    slate->metadata.remove(QStringLiteral("network"));
    qDebug() << "[WorkflowSign] compacted invoice I2"
             << "workflowId=" << workflowId
             << "remainingSigCount=" << slate->signatures.size()
             << "participantCount=" << slate->numParticipants
             << "amountLength=" << slate->amount.length();
}

void GrinWalletController::runExternalInvoicePreflight(const SlateV4 &incomingSlate,
                                                       const SlateV4 &emittedSlate,
                                                       const QString &armoredSlatepack) const
{
    const QString decodedRoundtrip = decodeIncomingSlatepack(armoredSlatepack, QByteArray());
    if (decodedRoundtrip.isEmpty()) {
        qDebug() << "[InvoicePreflight] roundtrip decode failed"
                 << "workflowId=" << emittedSlate.workflowId();
        return;
    }

    const QJsonDocument roundtripDocument = QJsonDocument::fromJson(decodedRoundtrip.toUtf8());
    if (!roundtripDocument.isObject()) {
        qDebug() << "[InvoicePreflight] roundtrip json invalid"
                 << "workflowId=" << emittedSlate.workflowId();
        return;
    }

    const SlateV4 roundtripSlate = SlateV4::fromJson(roundtripDocument.object());
    qDebug() << "[InvoicePreflight] roundtrip"
             << "workflowId=" << emittedSlate.workflowId()
             << "state=" << roundtripSlate.stateCode()
             << "version=" << roundtripSlate.versionCode()
             << "sender=" << roundtripSlate.metadata.value(QStringLiteral("slatepack_sender")).toString()
             << "sigCount=" << roundtripSlate.signatures.size()
             << "participantCount=" << roundtripSlate.numParticipants
             << "commitmentCount=" << roundtripSlate.commitments.size()
             << "amount=" << roundtripSlate.amount
             << "fee=" << roundtripSlate.fee
             << "offset=" << roundtripSlate.offset
             << "messageHash=" << roundtripSlate.metadata.value(QStringLiteral("message_hash")).toString();

    for (int i = 0; i < incomingSlate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = incomingSlate.signatures.at(i);
        qDebug() << "[InvoicePreflight] incoming-participant"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "xs=" << sig.xs
                 << "nonce=" << sig.nonce
                 << "partLen=" << sig.part.length();
    }
    for (int i = 0; i < emittedSlate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = emittedSlate.signatures.at(i);
        qDebug() << "[InvoicePreflight] emitted-participant"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "xs=" << sig.xs
                 << "nonce=" << sig.nonce
                 << "partLen=" << sig.part.length();
    }
    for (int i = 0; i < roundtripSlate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = roundtripSlate.signatures.at(i);
        qDebug() << "[InvoicePreflight] roundtrip-participant"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "xs=" << sig.xs
                 << "nonce=" << sig.nonce
                 << "partLen=" << sig.part.length();
    }

    for (int i = 0; i < incomingSlate.commitments.size(); ++i) {
        const SlateV4::Commit &commit = incomingSlate.commitments.at(i);
        qDebug() << "[InvoicePreflight] incoming-commit"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "feature=" << commit.feature
                 << "hasProof=" << !commit.proof.isEmpty()
                 << "commitment=" << commit.commitment;
    }
    for (int i = 0; i < emittedSlate.commitments.size(); ++i) {
        const SlateV4::Commit &commit = emittedSlate.commitments.at(i);
        qDebug() << "[InvoicePreflight] emitted-commit"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "feature=" << commit.feature
                 << "hasProof=" << !commit.proof.isEmpty()
                 << "commitment=" << commit.commitment;
    }
    for (int i = 0; i < roundtripSlate.commitments.size(); ++i) {
        const SlateV4::Commit &commit = roundtripSlate.commitments.at(i);
        qDebug() << "[InvoicePreflight] roundtrip-commit"
                 << "workflowId=" << emittedSlate.workflowId()
                 << "index=" << i
                 << "feature=" << commit.feature
                 << "hasProof=" << !commit.proof.isEmpty()
                 << "commitment=" << commit.commitment;
    }

    qDebug() << "[InvoicePreflight] compare"
             << "workflowId=" << emittedSlate.workflowId()
             << "plannedSender=" << emittedSlate.metadata.value(QStringLiteral("slatepack_sender")).toString()
             << "roundtripSender=" << roundtripSlate.metadata.value(QStringLiteral("slatepack_sender")).toString()
             << "plannedFee=" << emittedSlate.fee
             << "roundtripFee=" << roundtripSlate.fee
             << "plannedOffset=" << emittedSlate.offset
             << "roundtripOffset=" << roundtripSlate.offset
             << "plannedSigCount=" << emittedSlate.signatures.size()
             << "roundtripSigCount=" << roundtripSlate.signatures.size()
             << "plannedCommitmentCount=" << emittedSlate.commitments.size()
             << "roundtripCommitmentCount=" << roundtripSlate.commitments.size();

    SlateV4 combinedSlate = roundtripSlate;
    combinedSlate.signatures = incomingSlate.signatures;
    for (int i = 0; i < roundtripSlate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &roundtripParticipant = roundtripSlate.signatures.at(i);
        bool updated = false;
        for (int j = 0; j < combinedSlate.signatures.size(); ++j) {
            if (combinedSlate.signatures[j].xs == roundtripParticipant.xs) {
                combinedSlate.signatures[j] = roundtripParticipant;
                updated = true;
                break;
            }
        }
        if (!updated) {
            combinedSlate.signatures.append(roundtripParticipant);
        }
    }
    combinedSlate.numParticipants = combinedSlate.signatures.size();

    QString emittedBlindError;
    QString emittedNonceError;
    qDebug() << "[InvoicePreflight] emitted-aggsig"
             << "workflowId=" << emittedSlate.workflowId()
             << "messageHash=" << WalletCryptoBackend::kernelSignatureMessageHex(emittedSlate)
             << "pubkeyTotal=" << WalletCryptoBackend::combinedBlindPublicKeyHex(emittedSlate, &emittedBlindError)
             << "pubnonceTotal=" << WalletCryptoBackend::combinedNoncePublicKeyHex(emittedSlate, &emittedNonceError)
             << "pubkeyError=" << emittedBlindError
             << "pubnonceError=" << emittedNonceError;

    QString roundtripBlindError;
    QString roundtripNonceError;
    qDebug() << "[InvoicePreflight] roundtrip-aggsig"
             << "workflowId=" << emittedSlate.workflowId()
             << "messageHash=" << WalletCryptoBackend::kernelSignatureMessageHex(roundtripSlate)
             << "pubkeyTotal=" << WalletCryptoBackend::combinedBlindPublicKeyHex(roundtripSlate, &roundtripBlindError)
             << "pubnonceTotal=" << WalletCryptoBackend::combinedNoncePublicKeyHex(roundtripSlate, &roundtripNonceError)
             << "pubkeyError=" << roundtripBlindError
             << "pubnonceError=" << roundtripNonceError;

    QString combinedBlindError;
    QString combinedNonceError;
    qDebug() << "[InvoicePreflight] combined-aggsig"
             << "workflowId=" << emittedSlate.workflowId()
             << "messageHash=" << WalletCryptoBackend::kernelSignatureMessageHex(combinedSlate)
             << "pubkeyTotal=" << WalletCryptoBackend::combinedBlindPublicKeyHex(combinedSlate, &combinedBlindError)
             << "pubnonceTotal=" << WalletCryptoBackend::combinedNoncePublicKeyHex(combinedSlate, &combinedNonceError)
             << "pubkeyError=" << combinedBlindError
             << "pubnonceError=" << combinedNonceError
             << "combinedSigCount=" << combinedSlate.signatures.size()
             << "combinedParticipantCount=" << combinedSlate.numParticipants;

    QString verifyError;
    const bool verifyOk = WalletCryptoBackend::verifyPartialSignatures(combinedSlate, &verifyError);
    qDebug() << "[InvoicePreflight] partial-verify"
             << "workflowId=" << emittedSlate.workflowId()
             << "ok=" << verifyOk
             << "combinedSigCount=" << combinedSlate.signatures.size()
             << "combinedParticipantCount=" << combinedSlate.numParticipants
             << "error=" << verifyError;

    QString excessError;
    const QString excessCommitment = WalletCryptoBackend::calculateExcessCommitment(combinedSlate, &excessError);
    qDebug() << "[InvoicePreflight] excess"
             << "workflowId=" << emittedSlate.workflowId()
             << "commitment=" << excessCommitment
             << "error=" << excessError;

    QString finalSigError;
    QString finalSig;
    const bool finalSigOk = WalletCryptoBackend::buildFinalSignature(combinedSlate, &finalSig, &finalSigError);
    qDebug() << "[InvoicePreflight] final-sig"
             << "workflowId=" << emittedSlate.workflowId()
             << "ok=" << finalSigOk
             << "signature=" << finalSig
             << "error=" << finalSigError;

    int partialCount = 0;
    int missingPartialCount = 0;
    for (int i = 0; i < combinedSlate.signatures.size(); ++i) {
        const SlateV4::ParticipantData &participant = combinedSlate.signatures.at(i);
        if (participant.part.isEmpty()) {
            ++missingPartialCount;
            qDebug() << "[InvoicePreflight] missing-participant"
                     << "workflowId=" << emittedSlate.workflowId()
                     << "index=" << i
                     << "xs=" << participant.xs
                     << "nonce=" << participant.nonce
                     << "matchesIncoming=" << (i < incomingSlate.signatures.size()
                                               && incomingSlate.signatures.at(i).xs == participant.xs
                                               && incomingSlate.signatures.at(i).nonce == participant.nonce);
        } else {
            ++partialCount;
        }
    }

    qDebug() << "[InvoicePreflight] receiver-finalize"
             << "workflowId=" << emittedSlate.workflowId()
             << "partialCount=" << partialCount
             << "missingPartialCount=" << missingPartialCount
             << "numParticipants=" << combinedSlate.numParticipants
             << "needsStoredContext=" << (missingPartialCount > 0)
             << "expectedKernelExcess=" << excessCommitment
             << "finalSigPreflightOk=" << finalSigOk;

    qDebug() << "[InvoicePreflight] receiver-readiness"
             << "workflowId=" << emittedSlate.workflowId()
             << "hasSingleReplySignature=" << (roundtripSlate.signatures.size() == 1)
             << "hasTwoCommitments=" << (roundtripSlate.commitments.size() == 2)
             << "hasFee=" << !roundtripSlate.fee.trimmed().isEmpty()
             << "amountCleared=" << roundtripSlate.amount.trimmed().isEmpty()
             << "senderHeaderPresent=" << !roundtripSlate.metadata.value(QStringLiteral("slatepack_sender")).toString().trimmed().isEmpty()
             << "note=" << "final-sig at I2 may be incomplete without receiver context";
}

void GrinWalletController::compactStandardSlateForReturn(const QString &workflowId, SlateV4 *slate)
{
    if (!slate) {
        return;
    }

    const WalletCryptoBackend::ParticipantContext receiverContext =
        WalletCryptoBackend::createParticipant(m_seedFingerprint, workflowId, QStringLiteral("receiver"));
    const QString receiverBlind = slate->metadata.value(QStringLiteral("receiver_blind")).toString();

    if (!receiverBlind.isEmpty() && !receiverContext.blindSecret.isEmpty()) {
        QStringList positiveBlinds;
        positiveBlinds.append(slate->offset);
        positiveBlinds.append(receiverBlind);

        QStringList negativeBlinds;
        negativeBlinds.append(receiverContext.blindSecret);

        QString offsetError;
        const QString adjustedOffset =
            WalletCryptoBackend::combineBlindingFactors(positiveBlinds, negativeBlinds, &offsetError);
        if (!adjustedOffset.isEmpty()) {
            slate->offset = adjustedOffset;
        } else {
            qDebug() << "[WorkflowSign] standard S2 offset adjust failed"
                     << "workflowId=" << workflowId
                     << "error=" << offsetError;
        }
    }

    QList<SlateV4::ParticipantData> compactedSignatures;
    for (int i = 0; i < slate->signatures.size(); ++i) {
        const SlateV4::ParticipantData &sig = slate->signatures.at(i);
        if (sig.xs == receiverContext.blindPublic && sig.nonce == receiverContext.noncePublic) {
            compactedSignatures.append(sig);
        }
    }
    if (!compactedSignatures.isEmpty()) {
        slate->signatures = compactedSignatures;
    }

    slate->amount.clear();
    slate->fee.clear();
    qDebug() << "[WorkflowSign] compacted standard S2"
             << "workflowId=" << workflowId
             << "remainingSigCount=" << slate->signatures.size()
             << "offset=" << slate->offset
             << "amountLength=" << slate->amount.length()
             << "feeLength=" << slate->fee.length();
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
    entry.insert(QStringLiteral("amount"),
                 !slate.amount.trimmed().isEmpty()
                     ? slate.amount
                     : existingEntry.value(QStringLiteral("amount")).toString());
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
            // Only use unspent outputs for height inference: spent = old input UTXOs
            if (!output.spent && output.height > 0) {
                if (confirmedHeight == 0 || output.height < confirmedHeight) {
                    confirmedHeight = output.height;
                }
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
        if (entry.value(QStringLiteral("rescan_rebuilt")).toBool()
            && !workflowId.isEmpty()
            && !appendedExistingWorkflows.contains(workflowId)) {
            continue;
        }
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
            // Preserve original workflowId on input UTXOs so the receive/invoice
            // transaction that created them keeps its own identity and confirmed height.
            // Do NOT overwrite workflowId here.
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

    QJsonObject document = loadDocument();
    QJsonObject walletState = document.value(QStringLiteral("wallet_state")).toObject();
    const QList<WalletOutput> outputs = WalletScanner::outputsFromState(walletState);
    if (outputs.isEmpty()) {
        walletState.insert(QStringLiteral("scan_height"), static_cast<int>(m_chainHeight));
        walletState.insert(QStringLiteral("balances"), WalletScanner::balancesFromOutputs(outputs, m_chainHeight));
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
        refreshStateFromStorage();
        setLastInfo(QStringLiteral("Wallet has no tracked outputs yet. Starting seed scan."));
        m_syncStatus = QStringLiteral("Scanning wallet outputs...");
        emit statusChanged();
        m_walletScanInFlight = true;
        startSeedScan();
        return;
    }

    int unspentOnChainCount = 0;
    for (int i = 0; i < outputs.size(); ++i) {
        if (!outputs.at(i).spent && outputs.at(i).onChain) {
            ++unspentOnChainCount;
        }
    }

    // Node 5.4.0 can crash on get_outputs with large commitment lists.
    // Use seed scan as the primary sync mechanism and auto-heal stale index when all tracked outputs are off-chain.
    if (unspentOnChainCount == 0) {
        walletState.insert(QStringLiteral("restore_leaf_index"), 0);
        document.insert(QStringLiteral("wallet_state"), walletState);
        saveDocument(document);
        setLastInfo(QStringLiteral("All tracked outputs are currently off-chain. Restarting seed scan from leaf 1."));
    } else {
        setLastInfo(QStringLiteral("Refreshing tracked outputs via seed scan."));
    }

    m_syncStatus = QStringLiteral("Scanning wallet outputs...");
    emit statusChanged();
    m_walletScanInFlight = true;
    startSeedScan();
}
