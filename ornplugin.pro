TEMPLATE = lib
TARGET = ornplugin
QT += qml quick

CONFIG += qt plugin static c++11 link_pkgconfig
CONFIG(release, debug|release): DEFINES += QT_NO_DEBUG_OUTPUT
VERSION = 0.1
TARGET = $$qtLibraryTarget($$TARGET)
PKGCONFIG+= packagekitqt5
INCLUDEPATH+= /usr/include/packagekitqt5/

uri = harbour.orn

DEFINES += \
    ORN_LIB_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/orn_plugin.cpp \
    src/orn.cpp \
    src/ornapirequest.cpp \
    src/ornclient.cpp \
    src/ornversion.cpp \
    src/ornabstractlistmodel.cpp \
    src/ornabstractappsmodel.cpp \
    src/ornrecentappsmodel.cpp \
    src/ornuserappsmodel.cpp \
    src/orncommentsmodel.cpp \
    src/ornrepomodel.cpp \
    src/ornproxymodel.cpp \
    src/ornapplication.cpp \
    src/ornapplistitem.cpp \
    src/orncommentlistitem.cpp \
    src/ornsearchappsmodel.cpp \
    src/orncategoriesmodel.cpp \
    src/orncategorylistitem.cpp \
    src/orncategoryappsmodel.cpp \
    src/orninstalledappsmodel.cpp \
    src/ornzypp.cpp \
    src/ornbookmarksmodel.cpp \
    src/ornbackup.cpp

HEADERS += \
    src/orn_plugin.h \
    src/orn.h \
    src/ornapirequest.h \
    src/ornclient.h \
    src/ornversion.h \
    src/ornabstractlistmodel.h \
    src/ornabstractappsmodel.h \
    src/ornrecentappsmodel.h \
    src/ornuserappsmodel.h \
    src/orncommentsmodel.h \
    src/ornrepomodel.h \
    src/ornproxymodel.h \
    src/ornapplication.h \
    src/ornapplistitem.h \
    src/orncommentlistitem.h \
    src/ornsearchappsmodel.h \
    src/orncategoriesmodel.h \
    src/orncategorylistitem.h \
    src/orncategoryappsmodel.h \
    src/orninstalledappsmodel.h \
    src/ornzypp.h \
    src/ornbookmarksmodel.h \
    src/ornbackup.h

OTHER_FILES += \
    scripts/update_categories.py
