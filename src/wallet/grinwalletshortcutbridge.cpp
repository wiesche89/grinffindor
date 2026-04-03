#include "grinwalletshortcutbridge.h"

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>

#include "grinwalletplatformhelpers.h"

#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/emscripten.h>
#endif

namespace {

bool invokeNoArgMethod(QObject *object, const char *methodName)
{
    return object && QMetaObject::invokeMethod(object, methodName, Qt::DirectConnection);
}

bool replaceFocusedObjectSelection(QObject *object, const QString &text)
{
    if (!object) {
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
    object->setProperty("cursorPosition", text.length());
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

#ifdef Q_OS_WASM
static GrinWalletShortcutBridge *g_shortcutBridge = nullptr;

extern "C" {
EMSCRIPTEN_KEEPALIVE int grinffindorHandleShortcut(int key);
}

EM_JS(void, browserInstallWalletShortcutBridge, (), {
    try {
        if (typeof window === "undefined" || window.__grinffindorWalletShortcutBridgeInstalled) {
            return;
        }

        const debug = function(label, payload) {
            void label;
            void payload;
        };

        const qtCanvas = function() {
            if (typeof document === "undefined") {
                return null;
            }
            return document.querySelector("canvas");
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

extern "C" EMSCRIPTEN_KEEPALIVE int grinffindorHandleShortcut(int key)
{
    if (!g_shortcutBridge) {
        return 0;
    }
    return g_shortcutBridge->handleShortcutKey(key) ? 1 : 0;
}
#endif

} // namespace

GrinWalletShortcutBridge::GrinWalletShortcutBridge(QObject *parent)
    : QObject(parent)
{
}

void GrinWalletShortcutBridge::install()
{
#ifdef Q_OS_WASM
    g_shortcutBridge = this;
    browserInstallWalletShortcutBridge();
#endif
    qApp->installEventFilter(this);
}

bool GrinWalletShortcutBridge::eventFilter(QObject *watched, QEvent *event)
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

    if (handleShortcutKey(key)) {
        keyEvent->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool GrinWalletShortcutBridge::handleShortcutKey(int key)
{
    if (key != Qt::Key_A && key != Qt::Key_C && key != Qt::Key_V) {
        return false;
    }

    QObject *focusObject = qApp->focusObject();
    if (!focusObject) {
        return false;
    }

    if (key == Qt::Key_A) {
        return invokeNoArgMethod(focusObject, "selectAll");
    }

    if (key == Qt::Key_V) {
        const QString pastedText = GrinWalletPlatformHelpers::requestPasteText();
        return !pastedText.isEmpty() && replaceFocusedObjectSelection(focusObject, pastedText);
    }

    if (invokeNoArgMethod(focusObject, "copy")) {
        return true;
    }

    const QString copiedText = focusedObjectText(focusObject);
    return !copiedText.isEmpty() && GrinWalletPlatformHelpers::copyTextToClipboard(copiedText);
}

void GrinWalletShortcutBridge::updateBrowserShortcutContext(const QString &text,
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
