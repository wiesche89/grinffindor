#ifndef GRINWALLETPLATFORMHELPERS_H
#define GRINWALLETPLATFORMHELPERS_H

#include <QString>

class GrinWalletPlatformHelpers
{
public:
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
 * @brief Requests persistent browser storage.
 */
    static void requestPersistentBrowserStorage();
/**
 * @brief Processes storage persistence state.
 */
    static QString storagePersistenceState();
};

#endif
