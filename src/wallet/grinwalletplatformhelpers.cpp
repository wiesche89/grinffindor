#include "grinwalletplatformhelpers.h"

#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSaveFile>

#ifdef Q_OS_WASM
#include <cstdlib>
#include <emscripten.h>
#include <emscripten/emscripten.h>

/**
 * @brief EM_JS
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
 * @brief EM_JS
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
 * @brief EM_JS
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
 * @brief EM_JS
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
 * @brief EM_JS
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
 * @brief EM_JS
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
#endif

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
