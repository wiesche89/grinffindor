QT += quick qml quickcontrols2 gui network

CONFIG += c++17

QT_WASM_INITIAL_MEMORY = 128MB

SOURCES += src/main.cpp \
           src/nodefooterstatus.cpp \
           src/wallet/grinwalletstorage.cpp \
           src/wallet/grinwalletplatformhelpers.cpp \
           src/wallet/grinwalletcontrollerhelpers.cpp \
           src/wallet/grinwallethistoryhelpers.cpp \
           src/wallet/grinwalletseedcrypto.cpp \
           src/wallet/grinwalletshortcutbridge.cpp \
           src/wallet/grinwallettransactionstore.cpp \
           src/wallet/grinwalletworkflow.cpp \
           src/wallet/grinwalletworkflowhelpers.cpp \
           src/wallet/grinwalletworkflowtxhelpers.cpp \
           src/wallet/grinwalletnodesync.cpp \
           src/wallet/grinwalletnodesyncservice.cpp \
           src/wallet/grinwalletworkflowservice.cpp \
           src/wallet/grinwalletcontroller.cpp \
           src/wallet/grinwalletnodepushhelpers.cpp \
           src/wallet/slatev4.cpp \
           src/wallet/walletoutput.cpp \
           src/wallet/walletscanner.cpp \
           src/wallet/walletselection.cpp \
           src/wallet/walletkeychain.cpp \
           src/wallet/walletblake2b.cpp \
           src/wallet/walletcryptohelpers.cpp \
           src/wallet/walletcryptokernelhelpers.cpp \
           src/wallet/walletcryptosignaturehelpers.cpp \
           src/wallet/walletcryptoslatepackhelpers.cpp \
           src/wallet/wallettxbuilder.cpp \
           src/wallet/binaryslatev4reader.cpp \
           src/wallet/binaryslatev4writer.cpp \
           src/wallet/walletcryptobackend.cpp \
           3rdparty/monocypher/monocypher.c \
           3rdparty/secp256k1-zkp/src/secp256k1.c

HEADERS += src/nodefooterstatus.h \
           src/wallet/grinwalletstorage.h \
           src/wallet/grinwalletplatformhelpers.h \
           src/wallet/grinwalletcontrollerhelpers.h \
           src/wallet/grinwallethistoryhelpers.h \
           src/wallet/grinwalletseedcrypto.h \
           src/wallet/grinwalletshortcutbridge.h \
           src/wallet/grinwallettransactionstore.h \
           src/wallet/grinwalletworkflow.h \
           src/wallet/grinwalletworkflowhelpers.h \
           src/wallet/grinwalletworkflowtxhelpers.h \
           src/wallet/grinwalletnodesync.h \
           src/wallet/grinwalletnodesyncservice.h \
           src/wallet/grinwalletworkflowservice.h \
           src/wallet/grinwalletcontroller.h \
           src/wallet/grinwalletnodepushhelpers.h \
           src/wallet/slatev4.h \
           src/wallet/walletoutput.h \
           src/wallet/walletscanner.h \
           src/wallet/walletselection.h \
           src/wallet/walletkeychain.h \
           src/wallet/walletblake2b.h \
           src/wallet/walletcryptohelpers.h \
           src/wallet/walletcryptokernelhelpers.h \
           src/wallet/walletcryptosignaturehelpers.h \
           src/wallet/walletcryptoslatepackhelpers.h \
           src/wallet/wallettxbuilder.h \
           src/wallet/binaryslatev4reader.h \
           src/wallet/binaryslatev4writer.h \
           src/wallet/walletcryptobackend.h

INCLUDEPATH += $$PWD/3rdparty/secp256k1-zkp \
               $$PWD/3rdparty/secp256k1-zkp/include \
               $$PWD/3rdparty/secp256k1-zkp/src \
               $$PWD/3rdparty/monocypher \
               $$PWD/src

DEFINES += GRIN_HAS_SLATEPACK_CRYPTO

DEFINES += USE_NUM_NONE \
           USE_FIELD_INV_BUILTIN \
           USE_SCALAR_INV_BUILTIN \
           USE_FIELD_10X26 \
           USE_SCALAR_8X32 \
           ENABLE_MODULE_GENERATOR \
           ENABLE_MODULE_COMMITMENT \
           ENABLE_MODULE_BULLETPROOF \
           ENABLE_MODULE_AGGSIG

RESOURCES += qml.qrc \
            res.qrc


#SUBMODULES
include(src/submodules/grin-common-api/grin-common-api.pri)
include(src/submodules/grin-node-api/grin-node-api.pri)

wasm {
    # The default 50 MB initial memory is too small for the current
    # resource-heavy QML/UI bundle during wasm link time.
    RESOURCES -= res.qrc
    RESOURCES += res-wasm.qrc
    QMAKE_LFLAGS += -Wl,--initial-memory=134217728

    win32 {
        WASM_MEDIA_SOURCE = $$replace($$shell_path($$PWD/media), /, \\)
        WASM_MEDIA_TARGET = $$replace($$shell_path($$OUT_PWD/media), /, \\)
        QMAKE_POST_LINK += powershell -NoProfile -ExecutionPolicy Bypass -Command \"New-Item -ItemType Directory -Force -Path '$$WASM_MEDIA_TARGET' | Out-Null; Copy-Item -Path '$$WASM_MEDIA_SOURCE\\*' -Destination '$$WASM_MEDIA_TARGET' -Recurse -Force\" $$escape_expand(\\n\\t)
    } else {
        WASM_MEDIA_SOURCE = $$shell_path($$PWD/media)
        WASM_MEDIA_TARGET = $$shell_path($$OUT_PWD/media)
        QMAKE_POST_LINK += mkdir -p $$WASM_MEDIA_TARGET && cp -R $$WASM_MEDIA_SOURCE/* $$WASM_MEDIA_TARGET $$escape_expand(\\n\\t)
    }
}
