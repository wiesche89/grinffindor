#include "walletsecurerandom.h"

#include <QDebug>
#include <QRandomGenerator>

#ifdef Q_OS_WASM
#include <emscripten.h>

EM_JS(int, wasmFillSecureRandom, (char *data, int size), {
    try {
        if (size <= 0) {
            return 1;
        }
        if (typeof crypto === "undefined" || typeof crypto.getRandomValues !== "function") {
            return 0;
        }

        const view = HEAPU8.subarray(data, data + size);
        crypto.getRandomValues(view);
        return 1;
    } catch (e) {
        try {
            console.error("secure random generation failed", e);
        } catch (ignored) {}
        return 0;
    }
});
#endif

bool WalletSecureRandom::fill(QByteArray *buffer)
{
    if (!buffer) {
        return false;
    }
    if (buffer->isEmpty()) {
        return true;
    }

#ifdef Q_OS_WASM
    return wasmFillSecureRandom(buffer->data(), buffer->size()) == 1;
#else
    quint32 *wordPtr = reinterpret_cast<quint32 *>(buffer->data());
    const int wordCount = buffer->size() / static_cast<int>(sizeof(quint32));
    if (wordCount > 0) {
        QRandomGenerator::system()->fillRange(wordPtr, wordCount);
    }

    const int remainderOffset = wordCount * static_cast<int>(sizeof(quint32));
    if (remainderOffset < buffer->size()) {
        quint32 tail = QRandomGenerator::system()->generate();
        for (int i = remainderOffset; i < buffer->size(); ++i) {
            (*buffer)[i] = static_cast<char>(tail & 0xff);
            tail >>= 8;
        }
    }
    return true;
#endif
}

QByteArray WalletSecureRandom::bytes(int size)
{
    if (size <= 0) {
        return QByteArray();
    }

    QByteArray buffer(size, Qt::Uninitialized);
    if (!fill(&buffer)) {
        buffer.clear();
    }
    return buffer;
}

bool WalletSecureRandom::selfTest()
{
    const QByteArray first = bytes(32);
    const QByteArray second = bytes(32);
    if (first.size() != 32 || second.size() != 32) {
        qCritical() << "Secure random self-test failed: insufficient random data.";
        return false;
    }
    if (first == second) {
        qCritical() << "Secure random self-test failed: repeated random output.";
        return false;
    }
    return true;
}
