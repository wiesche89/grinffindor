#ifndef GRINWALLETPLATFORMHELPERS_H
#define GRINWALLETPLATFORMHELPERS_H

#include <QString>

class GrinWalletPlatformHelpers
{
public:
    static QString requestPasteText();
    static bool copyTextToClipboard(const QString &text);
    static bool downloadTextFile(const QString &suggestedName, const QString &text);
    static void requestPersistentBrowserStorage();
    static QString storagePersistenceState();
};

#endif
