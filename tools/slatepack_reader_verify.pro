QT += core

CONFIG += console c++17
CONFIG -= app_bundle

SOURCES += \
    slatepack_reader_verify.cpp \
    ../src/wallet/binaryslatev4reader.cpp \
    ../src/wallet/slatev4.cpp \
    ../3rdparty/monocypher/monocypher.c

HEADERS += \
    ../src/wallet/binaryslatev4reader.h \
    ../src/wallet/slatev4.h

INCLUDEPATH += \
    $$PWD/../src/wallet \
    $$PWD/../3rdparty/monocypher

DEFINES += GRIN_HAS_SLATEPACK_CRYPTO

