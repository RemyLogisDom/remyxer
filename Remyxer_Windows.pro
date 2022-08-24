QT += core gui multimedia widgets

INCLUDEPATH += $$PWD/include
INCLUDEPATH += "C:\Program Files\Mega-Nerd\libsndfile"
LIBS += -lportaudio -L$$PWD/lib
LIBS += -lsndfile -L$$PWD/lib

TARGET = Remyxer

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    include/qvumeterh.cpp \
    include/passwordlineedit.cpp
HEADERS += \
    mainwindow.h \
    include/qvumeterh.h \
    include/passwordlineedit.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    Remyxer_fr_FR.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    icon.qrc

DISTFILES += \
    images/music_off.png \
    images/music_on.png \
    images/stop.png
