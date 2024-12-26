INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt5
INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt5/QtWidgets
INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt5/QtGui
INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt5/QtCore
INCLUDEPATH += /usr/include/x86_64-linux-gnu/qt5/QtNetwork
QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG += sdk_no_version_check

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += /opt/homebrew/opt/jsoncpp/include
LIBS += -L/opt/homebrew/opt/jsoncpp/lib -ljsoncpp

INCLUDEPATH += /opt/homebrew/opt/curl/include
LIBS += -L/opt/homebrew/opt/curl/lib -lcurl

LIBS += -lcurl
INCLUDEPATH += /usr/include/curl

LIBS += -ljsoncpp
INCLUDEPATH += /usr/include/jsoncpp

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
