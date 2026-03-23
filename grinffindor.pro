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
}
