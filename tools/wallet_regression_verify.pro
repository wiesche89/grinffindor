QT += core gui network qml quick quickcontrols2

CONFIG += console c++17
CONFIG -= app_bundle

SOURCES += \
    wallet_regression_verify.cpp \
    ../src/grinwalletcontroller.cpp \
    ../src/wallet/slatev4.cpp \
    ../src/wallet/walletoutput.cpp \
    ../src/wallet/walletscanner.cpp \
    ../src/wallet/walletselection.cpp \
    ../src/wallet/walletkeychain.cpp \
    ../src/wallet/walletblake2b.cpp \
    ../src/wallet/wallettxbuilder.cpp \
    ../src/wallet/binaryslatev4reader.cpp \
    ../src/wallet/binaryslatev4writer.cpp \
    ../src/wallet/walletcryptobackend.cpp \
    ../3rdparty/monocypher/monocypher.c \
    ../3rdparty/secp256k1-zkp/src/secp256k1.c

HEADERS += \
    ../src/grinwalletcontroller.h \
    ../src/wallet/slatev4.h \
    ../src/wallet/walletoutput.h \
    ../src/wallet/walletscanner.h \
    ../src/wallet/walletselection.h \
    ../src/wallet/walletkeychain.h \
    ../src/wallet/walletblake2b.h \
    ../src/wallet/wallettxbuilder.h \
    ../src/wallet/binaryslatev4reader.h \
    ../src/wallet/binaryslatev4writer.h \
    ../src/wallet/walletcryptobackend.h

INCLUDEPATH += $$PWD/../3rdparty/secp256k1-zkp \
               $$PWD/../3rdparty/secp256k1-zkp/include \
               $$PWD/../3rdparty/secp256k1-zkp/src \
               $$PWD/../3rdparty/monocypher

DEFINES += GRIN_HAS_SLATEPACK_CRYPTO
DEFINES += HAVE_CONFIG_H

RESOURCES += ../qml.qrc

include(../src/submodules/grin-common-api/grin-common-api.pri)
include(../src/submodules/grin-node-api/grin-node-api.pri)
