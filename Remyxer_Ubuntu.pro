QT += core gui multimedia widgets

LIBS += -lportaudio -L/lib
LIBS += -lsndfile -L/lib

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

RESOURCES += \
    icon.qrc
