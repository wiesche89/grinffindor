QT += core testlib network

CONFIG += testcase console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = wallettests

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

INCLUDEPATH += $$PWD/.. \
               $$PWD/../3rdparty/secp256k1-zkp \
               $$PWD/../3rdparty/secp256k1-zkp/include \
               $$PWD/../3rdparty/secp256k1-zkp/src \
               $$PWD/../3rdparty/monocypher \
               $$PWD/../src \
               $$PWD/../src/wallet \
               $$PWD/../src/submodules/grin-common-api/src/attributes \
               $$PWD/../src/submodules/grin-common-api/src/util \
               $$PWD/../src/submodules/grin-node-api/src/attributes \
               $$PWD/../src/submodules/grin-node-api/src/foreign

SOURCES += $$PWD/wallettests_main.cpp \
           $$PWD/tst_walletcore.cpp \
           $$PWD/tst_walletintegration.cpp \
           $$PWD/../src/wallet/slatev4.cpp \
           $$PWD/../src/wallet/walletoutput.cpp \
           $$PWD/../src/wallet/walletscanner.cpp \
           $$PWD/../src/wallet/walletkeychain.cpp \
           $$PWD/../src/wallet/walletblake2b.cpp \
           $$PWD/../src/wallet/walletcryptobasehelpers.cpp \
           $$PWD/../src/wallet/walletcryptohelpers.cpp \
           $$PWD/../src/wallet/walletcryptoaggsighelpers.cpp \
           $$PWD/../src/wallet/walletcryptocommitmenthelpers.cpp \
           $$PWD/../src/wallet/walletcryptosecphelpers.cpp \
           $$PWD/../src/wallet/walletcryptokernelhelpers.cpp \
           $$PWD/../src/wallet/walletcryptosignaturehelpers.cpp \
           $$PWD/../src/wallet/walletcryptoslatepackhelpers.cpp \
           $$PWD/../src/wallet/walletcryptobackend.cpp \
           $$PWD/../src/wallet/grinwalletworkflow.cpp \
           $$PWD/../src/wallet/grinwalletnodesync.cpp \
           $$PWD/../src/wallet/grinwalletstorage.cpp \
           $$PWD/../src/wallet/grinwalletcontroller.cpp \
           $$PWD/../src/wallet/grinwalletcontrollerhelpers.cpp \
           $$PWD/../src/wallet/grinwalletcontrollerlifecycle.cpp \
           $$PWD/../src/wallet/grinwalletcontrollerstatehelpers.cpp \
           $$PWD/../src/wallet/grinwalletcontrollerworkflowhelpers.cpp \
           $$PWD/../src/wallet/grinwalletseedcrypto.cpp \
           $$PWD/../src/wallet/grinwallethistoryhelpers.cpp \
           $$PWD/../src/wallet/grinwalletplatformhelpers.cpp \
           $$PWD/../src/wallet/grinwalletshortcutbridge.cpp \
           $$PWD/../src/wallet/grinwalletnodesyncservice.cpp \
           $$PWD/../src/wallet/grinwalletnodesyncservicecontrol.cpp \
           $$PWD/../src/wallet/grinwalletworkflowservice.cpp \
           $$PWD/../src/wallet/grinwalletworkflowservicecreate.cpp \
           $$PWD/../src/wallet/grinwalletworkflowserviceprocess.cpp \
           $$PWD/../src/wallet/grinwalletworkflowserviceactions.cpp \
           $$PWD/../src/wallet/grinwalletworkflowtxhelpers.cpp \
           $$PWD/../src/wallet/grinwalletnodepushhelpers.cpp \
           $$PWD/../src/wallet/grinwallettransactionstore.cpp \
           $$PWD/../src/wallet/grinwalletworkflowhelpers.cpp \
           $$PWD/../src/wallet/walletselection.cpp \
           $$PWD/../src/wallet/wallettxbuilder.cpp \
           $$PWD/../src/wallet/binaryslatev4reader.cpp \
           $$PWD/../src/wallet/binaryslatev4writer.cpp \
           $$PWD/../src/submodules/grin-common-api/src/attributes/commitment.cpp \
           $$PWD/../src/submodules/grin-common-api/src/attributes/output.cpp \
           $$PWD/../src/submodules/grin-common-api/src/attributes/error.cpp \
           $$PWD/../src/submodules/grin-common-api/src/util/debugutils.cpp \
           $$PWD/../src/submodules/grin-common-api/src/util/jsonutil.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blindingfactor.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/input.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/merkleproof.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blockheaderprintable.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blockprintable.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blocklisting.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/locatedtxkernel.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/nodeversion.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/outputlisting.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/outputprintable.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/poolentry.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/txkernelprintable.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/txsource.cpp \
           $$PWD/../src/submodules/grin-node-api/src/attributes/tip.cpp \
            $$PWD/../src/submodules/grin-node-api/src/attributes/transaction.cpp \
            $$PWD/../src/submodules/grin-node-api/src/attributes/transactionbody.cpp \
            $$PWD/../src/submodules/grin-node-api/src/attributes/txkernel.cpp \
           $$PWD/../src/submodules/grin-node-api/src/foreign/nodeforeignapi.cpp \
           $$PWD/../3rdparty/monocypher/monocypher.c \
           $$PWD/../3rdparty/secp256k1-zkp/src/secp256k1.c

HEADERS += $$PWD/../src/wallet/slatev4.h \
           $$PWD/../src/wallet/walletoutput.h \
           $$PWD/../src/wallet/walletscanner.h \
           $$PWD/../src/wallet/walletkeychain.h \
           $$PWD/../src/wallet/walletblake2b.h \
           $$PWD/../src/wallet/walletcryptobasehelpers.h \
           $$PWD/../src/wallet/walletcryptohelpers.h \
           $$PWD/../src/wallet/walletcryptoaggsighelpers.h \
           $$PWD/../src/wallet/walletcryptocommitmenthelpers.h \
           $$PWD/../src/wallet/walletcryptosecphelpers.h \
           $$PWD/../src/wallet/walletcryptokernelhelpers.h \
           $$PWD/../src/wallet/walletcryptosignaturehelpers.h \
           $$PWD/../src/wallet/walletcryptoslatepackhelpers.h \
           $$PWD/../src/wallet/walletcryptobackend.h \
           $$PWD/../src/wallet/grinwalletworkflow.h \
           $$PWD/../src/wallet/grinwalletnodesync.h \
           $$PWD/../src/wallet/grinwalletstorage.h \
           $$PWD/../src/wallet/grinwalletcontroller.h \
           $$PWD/../src/wallet/grinwalletcontrollerhelpers.h \
           $$PWD/../src/wallet/grinwalletseedcrypto.h \
           $$PWD/../src/wallet/grinwallethistoryhelpers.h \
           $$PWD/../src/wallet/grinwalletplatformhelpers.h \
           $$PWD/../src/wallet/grinwalletshortcutbridge.h \
           $$PWD/../src/wallet/grinwalletnodesyncservice.h \
           $$PWD/../src/wallet/grinwalletworkflowservice.h \
           $$PWD/../src/wallet/grinwalletworkflowtxhelpers.h \
           $$PWD/../src/wallet/grinwalletnodepushhelpers.h \
           $$PWD/../src/wallet/grinwallettransactionstore.h \
           $$PWD/../src/wallet/grinwalletworkflowhelpers.h \
           $$PWD/../src/wallet/walletselection.h \
           $$PWD/../src/wallet/wallettxbuilder.h \
           $$PWD/../src/wallet/binaryslatev4reader.h \
           $$PWD/../src/wallet/binaryslatev4writer.h \
           $$PWD/../src/submodules/grin-common-api/src/attributes/commitment.h \
           $$PWD/../src/submodules/grin-common-api/src/attributes/output.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blindingfactor.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/input.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blockheaderprintable.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blockprintable.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/blocklisting.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/locatedtxkernel.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/merkleproof.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/nodeversion.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/outputfeatures.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/outputlisting.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/outputprintable.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/poolentry.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/txkernelprintable.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/txsource.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/tip.h \
            $$PWD/../src/submodules/grin-node-api/src/attributes/transaction.h \
            $$PWD/../src/submodules/grin-node-api/src/attributes/transactionbody.h \
           $$PWD/../src/submodules/grin-node-api/src/attributes/txkernel.h \
           $$PWD/../src/submodules/grin-node-api/src/foreign/nodeforeignapi.h

RESOURCES += $$PWD/wallettestresources.qrc
