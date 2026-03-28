QT += core

CONFIG += console c++17
CONFIG -= app_bundle

SOURCES += \
    slatepack_reader_verify.cpp \
    ../src/wallet/binaryslatev4reader.cpp \
    ../src/wallet/binaryslatev4writer.cpp \
    ../src/wallet/slatev4.cpp \
    ../src/wallet/walletcryptobackend.cpp \
    ../src/wallet/walletkeychain.cpp \
    ../src/wallet/walletblake2b.cpp \
    ../3rdparty/secp256k1-zkp/src/secp256k1.c \
    ../3rdparty/monocypher/monocypher.c

HEADERS += \
    ../src/wallet/binaryslatev4reader.h \
    ../src/wallet/binaryslatev4writer.h \
    ../src/wallet/slatev4.h \
    ../src/wallet/walletcryptobackend.h \
    ../src/wallet/walletkeychain.h \
    ../src/wallet/walletblake2b.h

INCLUDEPATH += \
    $$PWD/../src/wallet \
    $$PWD/../3rdparty/monocypher \
    $$PWD/../3rdparty/secp256k1-zkp \
    $$PWD/../3rdparty/secp256k1-zkp/include \
    $$PWD/../3rdparty/secp256k1-zkp/src

DEFINES += GRIN_HAS_SLATEPACK_CRYPTO
DEFINES += HAVE_CONFIG_H
