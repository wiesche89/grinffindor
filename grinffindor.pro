QT += quick qml quickcontrols2 gui

CONFIG += c++11

SOURCES += src/main.cpp \
           src/nodefooterstatus.cpp

HEADERS += src/nodefooterstatus.h

RESOURCES += qml.qrc \
            res.qrc


#SUBMODULES
include(src/submodules/grin-common-api/grin-common-api.pri)
include(src/submodules/grin-node-api/grin-node-api.pri)
