QT += core network testlib
QT -= gui
CONFIG += testcase console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = nodeblocktests
SOURCES += $$PWD/tst_nodeblock.cpp
HEADERS += $$PWD/tst_nodeblock.h
include(../src/submodules/grin-common-api/grin-common-api.pri)
include(../src/submodules/grin-node-api/grin-node-api.pri)
