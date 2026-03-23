QT += quick qml quickcontrols2 gui

CONFIG += c++11

QT_WASM_INITIAL_MEMORY = 128MB

SOURCES += src/main.cpp \
           src/nodefooterstatus.cpp

HEADERS += src/nodefooterstatus.h

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
