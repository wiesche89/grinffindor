#ifndef GRINWALLETPLATFORMHELPERS_H
#define GRINWALLETPLATFORMHELPERS_H

#include <QString>
#include <QVariantMap>

class GrinWalletPlatformHelpers
{
public:
/**
 * @brief Returns whether the runtime target is WASM.
 */
    static bool isWasm();
/**
 * @brief Returns whether the browser runtime should be treated as mobile.
 */
    static bool isMobileBrowser();
/**
 * @brief Returns whether browser-native touch selection should be preferred.
 */
    static bool supportsNativeTouchSelection();
/**
 * @brief Requests paste text.
 */
    static QString requestPasteText();
/**
 * @brief Copies text to clipboard.
 */
    static bool copyTextToClipboard(const QString &text);
/**
 * @brief Downloads text file.
 */
    static bool downloadTextFile(const QString &suggestedName, const QString &text);
/**
 * @brief Opens the platform-native text editor bridge.
 */
     static bool openTextEditor(const QString &editorId,
                                         const QString &title,
                                         const QString &text,
                                         const QVariantMap &options);
/**
 * @brief Closes the platform-native text editor bridge.
 */
     static void closeTextEditor(bool accept);
/**
 * @brief Requests persistent browser storage.
 */
    static void requestPersistentBrowserStorage();
/**
 * @brief Processes storage persistence state.
 */
    static QString storagePersistenceState();
};

#endif
