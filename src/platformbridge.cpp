#include "platformbridge.h"

#include "wallet/grinwalletplatformhelpers.h"

#ifdef Q_OS_WASM
#include <emscripten.h>
#endif

namespace {

PlatformBridge *g_platformBridge = nullptr;

}

#ifdef Q_OS_WASM
extern "C" {

EMSCRIPTEN_KEEPALIVE void grinffindorPlatformBridgeEditorChanged(const char *id, const char *text)
{
    if (!g_platformBridge) {
        return;
    }
    g_platformBridge->handleEditorTextChanged(QString::fromUtf8(id ? id : ""),
                                              QString::fromUtf8(text ? text : ""));
}

EMSCRIPTEN_KEEPALIVE void grinffindorPlatformBridgeEditorAccepted(const char *id, const char *text)
{
    if (!g_platformBridge) {
        return;
    }
    g_platformBridge->handleEditorAccepted(QString::fromUtf8(id ? id : ""),
                                           QString::fromUtf8(text ? text : ""));
}

EMSCRIPTEN_KEEPALIVE void grinffindorPlatformBridgeEditorClosed(const char *id,
                                                                const char *text,
                                                                int accepted)
{
    if (!g_platformBridge) {
        return;
    }
    g_platformBridge->handleEditorClosed(QString::fromUtf8(id ? id : ""),
                                         QString::fromUtf8(text ? text : ""),
                                         accepted == 1);
}

}
#endif

PlatformBridge::PlatformBridge(QObject *parent)
    : QObject(parent)
{
    g_platformBridge = this;
}

bool PlatformBridge::openTextEditor(const QString &title,
                                    const QString &text,
                                    const QVariantMap &options)
{
    const QString editorId = options.value(QStringLiteral("id")).toString().trimmed();
    if (editorId.isEmpty()) {
        return false;
    }

    if (!supportsNativeTouchSelection()) {
        emit focusRequested(editorId);
        return false;
    }

    if (!GrinWalletPlatformHelpers::openTextEditor(editorId, title, text, options)) {
        return false;
    }

    m_activeEditorId = editorId;
    m_activeEditorOptions = options;
    emit activeEditorChanged();
    emit textEditorOpened(editorId, title, text, options);
    return true;
}

bool PlatformBridge::copyToClipboard(const QString &text) const
{
    return writeClipboard(text);
}

QString PlatformBridge::readClipboard() const
{
    return GrinWalletPlatformHelpers::requestPasteText();
}

bool PlatformBridge::writeClipboard(const QString &text) const
{
    return GrinWalletPlatformHelpers::copyTextToClipboard(text);
}

void PlatformBridge::requestFocus(const QString &id)
{
    emit focusRequested(id);
}

bool PlatformBridge::isMobile() const
{
    return GrinWalletPlatformHelpers::isNativeMobilePlatform()
        || GrinWalletPlatformHelpers::isMobileBrowser();
}

bool PlatformBridge::isWasm() const
{
    return GrinWalletPlatformHelpers::isWasm();
}

bool PlatformBridge::supportsNativeTouchSelection() const
{
    return GrinWalletPlatformHelpers::supportsNativeTouchSelection();
}

void PlatformBridge::closeTextEditor(bool accept)
{
    GrinWalletPlatformHelpers::closeTextEditor(accept);
}

QString PlatformBridge::activeEditorId() const
{
    return m_activeEditorId;
}

void PlatformBridge::handleEditorTextChanged(const QString &id, const QString &text)
{
    emit textEditorTextChanged(id, text);
}

void PlatformBridge::handleEditorAccepted(const QString &id, const QString &text)
{
    emit textEditorAccepted(id, text);
}

void PlatformBridge::handleEditorClosed(const QString &id, const QString &text, bool accepted)
{
    if (m_activeEditorId == id) {
        m_activeEditorId.clear();
        m_activeEditorOptions.clear();
        emit activeEditorChanged();
    }
    emit textEditorClosed(id, text, accepted);
}
