QT += core gui qml quick quickcontrols2 dbus sql

CONFIG += c++17 release
TARGET = corbel
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/itemmodel.h \
    src/pin.h \
    src/planimport.h \
    src/portalfilepicker.h \
    src/report.h \
    src/store.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/itemmodel.cpp \
    src/pin.cpp \
    src/planimport.cpp \
    src/portalfilepicker.cpp \
    src/report.cpp \
    src/store.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
