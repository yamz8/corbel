QT += core gui qml quick quickcontrols2 testlib dbus sql
CONFIG += c++17 testcase
TARGET = corbel_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/itemmodel.h ../src/pin.h ../src/theme.h ../src/translator.h
SOURCES += corbel_tests.cpp ../src/backend.cpp ../src/itemmodel.cpp ../src/pin.cpp ../src/planimport.cpp \
    ../src/report.cpp ../src/store.cpp ../src/theme.cpp ../src/translator.cpp
