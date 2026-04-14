#include "grinwalletplatformhelpers.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QSaveFile>

#ifdef Q_OS_WASM
#include <cstdlib>
#include <emscripten.h>
#include <emscripten/emscripten.h>

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param int
 * @param browserCopyToClipboard
 * @param value
 */
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

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param char
 * @param browserReadClipboardText
 * @param (
 */
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

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param char
 * @param browserConsumeCapturedPasteText
 * @param (
 */
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

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param int
 * @param browserDownloadTextFile
 * @param value
 */
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

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param int
 * @param browserRequestPersistentStorage
 * @param (
 */
EM_JS(int, browserRequestPersistentStorage, (), {
    try {
        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist();
            return 1;
        }
    } catch (e) {}
    return 0;
});

/**
 * @brief Declares a JavaScript bridge function for WASM integration.
 * @param char
 * @param browserStoragePersistenceState
 * @param (
 */
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

EM_JS(int, browserIsMobileBrowser, (), {
    try {
        if (typeof window === "undefined" || typeof navigator === "undefined") {
            return 0;
        }
        const userAgent = navigator.userAgent || "";
        const coarsePointer = typeof window.matchMedia === "function"
            && window.matchMedia("(pointer: coarse)").matches;
        const touchPoints = navigator.maxTouchPoints || 0;
        const mobileAgent = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini|Mobile/i.test(userAgent);
        return (coarsePointer || touchPoints > 1 || mobileAgent) ? 1 : 0;
    } catch (e) {}
    return 0;
});

EM_JS(int, browserSupportsNativeTouchSelection, (), {
    try {
        if (typeof window === "undefined" || typeof navigator === "undefined") {
            return 0;
        }
        const userAgent = navigator.userAgent || "";
        const coarsePointer = typeof window.matchMedia === "function"
            && window.matchMedia("(pointer: coarse)").matches;
        const touchPoints = navigator.maxTouchPoints || 0;
        const mobileAgent = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini|Mobile/i.test(userAgent);
        return (coarsePointer || touchPoints > 1 || mobileAgent) ? 1 : 0;
    } catch (e) {}
    return 0;
});

EM_JS(int, browserOpenTextEditor, (const char *editorId,
                                   const char *title,
                                   const char *text,
                                   const char *optionsJson), {
    try {
        if (typeof window === "undefined" || typeof document === "undefined" || !document.body) {
            return 0;
        }

        const ensureManager = function() {
            if (window.__grinffindorPlatformEditorManager) {
                return window.__grinffindorPlatformEditorManager;
            }

            const state = {
                active: false,
                currentId: "",
                activeField: null,
                restoreOverflow: "",
                elements: {}
            };

            const sendEditorEvent = function(method, idValue, textValue, acceptedValue) {
                try {
                    if (typeof Module === "undefined") {
                        return;
                    }

                    const exported = Module["_" + method];
                    if (typeof exported !== "function") {
                        return;
                    }

                    const safeId = idValue || "";
                    const safeText = textValue || "";
                    const idLength = lengthBytesUTF8(safeId) + 1;
                    const textLength = lengthBytesUTF8(safeText) + 1;
                    const idPointer = _malloc(idLength);
                    const textPointer = _malloc(textLength);

                    stringToUTF8(safeId, idPointer, idLength);
                    stringToUTF8(safeText, textPointer, textLength);

                    try {
                        if (acceptedValue === undefined) {
                            exported(idPointer, textPointer);
                        } else {
                            exported(idPointer, textPointer, acceptedValue ? 1 : 0);
                        }
                    } finally {
                        _free(idPointer);
                        _free(textPointer);
                    }
                } catch (e) {}
            };

            const syncGlobalState = function(active, id) {
                window.__grinffindorPlatformEditor = {
                    active: !!active,
                    id: id || ""
                };
            };

            const createButton = function(label) {
                const button = document.createElement("button");
                button.type = "button";
                button.textContent = label;
                button.style.border = "1px solid #2a4f64";
                button.style.background = "#10273a";
                button.style.color = "#f3fbff";
                button.style.borderRadius = "999px";
                button.style.padding = "12px 18px";
                button.style.fontSize = "14px";
                button.style.fontWeight = "600";
                button.style.cursor = "pointer";
                return button;
            };

            const copyFieldValue = function(field) {
                try {
                    const value = field ? (field.value || "") : "";
                    if (!value.length) {
                        return false;
                    }
                    if (navigator.clipboard && navigator.clipboard.writeText) {
                        navigator.clipboard.writeText(value);
                        return true;
                    }
                } catch (e) {}

                try {
                    if (!field) {
                        return false;
                    }
                    field.focus();
                    if (typeof field.select === "function") {
                        field.select();
                    }
                    if (typeof field.setSelectionRange === "function") {
                        field.setSelectionRange(0, field.value.length);
                    }
                    return !!document.execCommand("copy");
                } catch (e) {}
                return false;
            };

            const ensureElements = function() {
                if (state.elements.overlay) {
                    return;
                }

                const overlay = document.createElement("div");
                overlay.style.position = "fixed";
                overlay.style.inset = "0";
                overlay.style.zIndex = "2147483647";
                overlay.style.display = "none";
                overlay.style.alignItems = "stretch";
                overlay.style.justifyContent = "center";
                overlay.style.background = "rgba(3, 8, 13, 0.78)";
                overlay.style.padding = "max(16px, env(safe-area-inset-top)) 16px max(16px, env(safe-area-inset-bottom))";
                overlay.style.boxSizing = "border-box";

                const panel = document.createElement("div");
                panel.style.width = "min(100%, 820px)";
                panel.style.maxHeight = "100%";
                panel.style.display = "flex";
                panel.style.flexDirection = "column";
                panel.style.background = "#0f1b26";
                panel.style.border = "1px solid #2a4f64";
                panel.style.borderRadius = "24px";
                panel.style.boxShadow = "0 32px 90px rgba(0, 0, 0, 0.45)";
                panel.style.overflow = "hidden";

                const header = document.createElement("div");
                header.style.padding = "18px 20px 10px";
                header.style.display = "flex";
                header.style.flexDirection = "column";
                header.style.gap = "8px";

                const titleNode = document.createElement("div");
                titleNode.style.color = "#ffffff";
                titleNode.style.fontSize = "22px";
                titleNode.style.fontWeight = "700";
                titleNode.style.lineHeight = "1.2";

                const body = document.createElement("div");
                body.style.padding = "10px 20px 20px";
                body.style.display = "flex";
                body.style.flex = "1 1 auto";
                body.style.minHeight = "180px";

                const footer = document.createElement("div");
                footer.style.display = "flex";
                footer.style.justifyContent = "space-between";
                footer.style.alignItems = "center";
                footer.style.gap = "10px";
                footer.style.padding = "0 20px 20px";

                const leftActions = document.createElement("div");
                leftActions.style.display = "flex";
                leftActions.style.gap = "10px";

                const rightActions = document.createElement("div");
                rightActions.style.display = "flex";
                rightActions.style.gap = "10px";

                const copyButton = createButton("Copy");
                const cancelButton = createButton("Cancel");
                const acceptButton = createButton("Done");
                acceptButton.style.background = "#194866";
                acceptButton.style.borderColor = "#2c6585";

                leftActions.appendChild(copyButton);
                rightActions.appendChild(cancelButton);
                rightActions.appendChild(acceptButton);
                header.appendChild(titleNode);
                panel.appendChild(header);
                panel.appendChild(body);
                footer.appendChild(leftActions);
                footer.appendChild(rightActions);
                panel.appendChild(footer);
                overlay.appendChild(panel);
                document.body.appendChild(overlay);

                overlay.addEventListener("click", function(event) {
                    if (event.target === overlay) {
                        state.close(false);
                    }
                });

                cancelButton.addEventListener("click", function() {
                    state.close(false);
                });

                copyButton.addEventListener("click", function() {
                    copyFieldValue(state.activeField);
                });

                acceptButton.addEventListener("click", function() {
                    state.close(true);
                });

                state.elements.overlay = overlay;
                state.elements.titleNode = titleNode;
                state.elements.body = body;
                state.elements.copyButton = copyButton;
                state.elements.cancelButton = cancelButton;
                state.elements.acceptButton = acceptButton;
            };

            const buildField = function(multiline) {
                const field = document.createElement(multiline ? "textarea" : "input");
                if (!multiline) {
                    field.type = "text";
                }
                field.style.width = "100%";
                field.style.minHeight = multiline ? "220px" : "52px";
                field.style.maxHeight = multiline ? "min(52vh, 520px)" : "52px";
                field.style.padding = multiline ? "16px" : "14px 16px";
                field.style.borderRadius = "18px";
                field.style.border = "1px solid #2a4f64";
                field.style.background = "#08131c";
                field.style.color = "#e2f4ff";
                field.style.fontSize = multiline ? "15px" : "16px";
                field.style.lineHeight = multiline ? "1.45" : "1.2";
                field.style.outline = "none";
                field.style.resize = multiline ? "vertical" : "none";
                field.style.boxSizing = "border-box";
                field.style.fontFamily = "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif";
                field.spellcheck = true;
                field.autocomplete = "off";
                return field;
            };

            state.handleInput = function() {
                if (!state.active || !state.activeField) {
                    return;
                }
                sendEditorEvent("grinffindorPlatformBridgeEditorChanged",
                                state.currentId || "",
                                state.activeField.value || "");
            };

            state.hideWithoutSignal = function() {
                if (state.elements.overlay) {
                    state.elements.overlay.style.display = "none";
                }
                if (state.elements.body) {
                    state.elements.body.innerHTML = "";
                }
                if (document.body) {
                    document.body.style.overflow = state.restoreOverflow || "";
                }
                state.active = false;
                state.currentId = "";
                state.activeField = null;
                syncGlobalState(false, "");
            };

            state.close = function(accepted) {
                if (!state.active) {
                    return;
                }
                const textValue = state.activeField ? (state.activeField.value || "") : "";
                const currentId = state.currentId || "";
                if (accepted) {
                    sendEditorEvent("grinffindorPlatformBridgeEditorAccepted",
                                 currentId,
                                 textValue);
                }
                 sendEditorEvent("grinffindorPlatformBridgeEditorClosed",
                              currentId,
                              textValue,
                              accepted);
                state.hideWithoutSignal();
            };

            state.open = function(payload) {
                ensureElements();

                const options = payload.options || {};
                const multiline = !!options.multiline;
                const isPasswordField = options.password === true;
                const field = buildField(multiline);

                state.restoreOverflow = document.body ? document.body.style.overflow : "";
                if (document.body) {
                    document.body.style.overflow = "hidden";
                }

                if (!multiline) {
                    if (isPasswordField) {
                        field.type = "password";
                    } else if (typeof options.inputType === "string" && options.inputType.length > 0) {
                        field.type = options.inputType;
                    }
                    field.enterKeyHint = typeof options.enterKeyHint === "string" && options.enterKeyHint.length > 0
                        ? options.enterKeyHint
                        : "done";
                }

                if (typeof options.inputMode === "string" && options.inputMode.length > 0) {
                    field.inputMode = options.inputMode;
                }
                if (typeof options.placeholder === "string") {
                    field.placeholder = options.placeholder;
                }
                if (typeof options.autoCapitalize === "string" && options.autoCapitalize.length > 0) {
                    field.autocapitalize = options.autoCapitalize;
                }
                if (options.autoCorrect === false) {
                    field.autocorrect = "off";
                }
                if (options.spellcheck === false) {
                    field.spellcheck = false;
                }
                field.readOnly = !!options.readOnly;
                field.value = payload.text || "";

                field.addEventListener("input", state.handleInput);
                field.addEventListener("keydown", function(event) {
                    if (!event) {
                        return;
                    }
                    if (event.key === "Escape") {
                        event.preventDefault();
                        state.close(false);
                        return;
                    }
                    if (!multiline && event.key === "Enter") {
                        event.preventDefault();
                        state.close(true);
                    }
                });

                state.elements.titleNode.textContent = payload.title || options.placeholder || "Edit text";
                state.elements.copyButton.style.display = isPasswordField ? "none" : "inline-flex";
                state.elements.acceptButton.textContent = options.readOnly ? (options.acceptText || "Close") : (options.acceptText || "Done");
                state.elements.cancelButton.style.display = options.readOnly ? "none" : "inline-flex";
                state.elements.body.innerHTML = "";
                state.elements.body.appendChild(field);
                state.elements.overlay.style.display = "flex";

                state.active = true;
                state.currentId = payload.id || "";
                state.activeField = field;
                syncGlobalState(true, state.currentId);

                window.setTimeout(function() {
                    try {
                        field.focus({ preventScroll: true });
                    } catch (e) {
                        field.focus();
                    }
                    if (typeof field.setSelectionRange === "function") {
                        if (options.selectAll !== false) {
                            field.setSelectionRange(0, field.value.length);
                        } else {
                            const position = field.value.length;
                            field.setSelectionRange(position, position);
                        }
                    } else if (typeof field.select === "function" && options.selectAll !== false) {
                        field.select();
                    }
                }, 0);

                state.handleInput();
                return true;
            };

            syncGlobalState(false, "");
            window.__grinffindorPlatformEditorManager = state;
            return state;
        };

        const manager = ensureManager();
        const parsedOptions = JSON.parse(UTF8ToString(optionsJson) || "{}");
        return manager.open({
            id: UTF8ToString(editorId),
            title: UTF8ToString(title),
            text: UTF8ToString(text),
            options: parsedOptions
        }) ? 1 : 0;
    } catch (e) {
        try {
            console.error("grinffindor platform editor open failed", e);
        } catch (ignored) {}
    }
    return 0;
});

EM_JS(void, browserCloseTextEditor, (int accept), {
    try {
        if (typeof window === "undefined" || !window.__grinffindorPlatformEditorManager) {
            return;
        }
        window.__grinffindorPlatformEditorManager.close(!!accept);
    } catch (e) {}
});
#endif

bool GrinWalletPlatformHelpers::isWasm()
{
#ifdef Q_OS_WASM
    return true;
#else
    return false;
#endif
}

bool GrinWalletPlatformHelpers::isNativeMobilePlatform()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

bool GrinWalletPlatformHelpers::isMobileBrowser()
{
#ifdef Q_OS_WASM
    return browserIsMobileBrowser() == 1;
#else
    return false;
#endif
}

bool GrinWalletPlatformHelpers::supportsNativeTouchSelection()
{
#ifdef Q_OS_WASM
    return browserSupportsNativeTouchSelection() == 1;
#else
    // Native iOS/Android use Qt's built-in keyboard and focus handling.
    // The browser-overlay editor is a WASM-only feature.
    return false;
#endif
}

/**
 * @brief GrinWalletPlatformHelpers::requestPasteText
 * @return
 */
QString GrinWalletPlatformHelpers::requestPasteText()
{
#ifdef Q_OS_WASM
    const char *capturedValue = browserConsumeCapturedPasteText();

    const QString capturedText = QString::fromUtf8(capturedValue ? capturedValue : "");
    if (capturedValue) {
        free(const_cast<char *>(capturedValue));
    }
    if (!capturedText.isEmpty()) {
        return capturedText;
    }

    const char *value = browserReadClipboardText();

    const QString clipboardText = QString::fromUtf8(value ? value : "");
    if (value) {
        free(const_cast<char *>(value));
    }
    return clipboardText;
#else
    const QClipboard *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString();
#endif
}

/**
 * @brief GrinWalletPlatformHelpers::copyTextToClipboard
 * @param text
 * @return
 */
bool GrinWalletPlatformHelpers::copyTextToClipboard(const QString &text)
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

/**
 * @brief GrinWalletPlatformHelpers::downloadTextFile
 * @param suggestedName
 * @param text
 * @return
 */
bool GrinWalletPlatformHelpers::downloadTextFile(const QString &suggestedName, const QString &text)
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
    const QString fileName = suggestedName.trimmed().isEmpty()
        ? QStringLiteral("grinffindor-wallet-backup.json")
        : suggestedName.trimmed();
    QSaveFile outputFile(QFileInfo(fileName).isAbsolute()
                             ? fileName

                             : QFileInfo(QDir::current(), fileName).absoluteFilePath());
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    if (outputFile.write(text.toUtf8()) < 0) {
        outputFile.cancelWriting();
        return false;
    }
    return outputFile.commit();
#endif
}

bool GrinWalletPlatformHelpers::openTextEditor(const QString &editorId,
                                              const QString &title,
                                              const QString &text,
                                              const QVariantMap &options)
{
#ifdef Q_OS_WASM
    if (!supportsNativeTouchSelection() || editorId.trimmed().isEmpty()) {
        return false;
    }

    const QByteArray editorIdUtf8 = editorId.toUtf8();
    const QByteArray titleUtf8 = title.toUtf8();
    const QByteArray textUtf8 = text.toUtf8();
    const QByteArray optionsUtf8 = QJsonDocument::fromVariant(options).toJson(QJsonDocument::Compact);

    return browserOpenTextEditor(editorIdUtf8.constData(),
                                 titleUtf8.constData(),
                                 textUtf8.constData(),
                                 optionsUtf8.constData()) == 1;
#else
    Q_UNUSED(editorId)
    Q_UNUSED(title)
    Q_UNUSED(text)
    Q_UNUSED(options)
    return false;
#endif
}

void GrinWalletPlatformHelpers::closeTextEditor(bool accept)
{
#ifdef Q_OS_WASM
    browserCloseTextEditor(accept ? 1 : 0);
#else
    Q_UNUSED(accept)
#endif
}

/**
 * @brief GrinWalletPlatformHelpers::requestPersistentBrowserStorage
 */
void GrinWalletPlatformHelpers::requestPersistentBrowserStorage()
{
#ifdef Q_OS_WASM
    browserRequestPersistentStorage();
#endif
}

/**
 * @brief GrinWalletPlatformHelpers::storagePersistenceState
 * @return
 */
QString GrinWalletPlatformHelpers::storagePersistenceState()
{
#ifdef Q_OS_WASM
    if (char *state = browserStoragePersistenceState()) {
        const QString value = QString::fromUtf8(state);
        free(state);
        return value;
    }
    return QStringLiteral("best-effort");
#else
    return QStringLiteral("native");
#endif
}
