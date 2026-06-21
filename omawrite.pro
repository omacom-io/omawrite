QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = omawrite
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/filepicker.h \
    src/markdownhighlighter.h \
    src/portalfilepicker.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/markdownhighlighter.cpp \
    src/portalfilepicker.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
