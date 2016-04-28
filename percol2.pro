QT += widgets
CONFIG += debug
TEMPLATE = app
TARGET = percol2
DEPENDPATH += .
INCLUDEPATH += .

# Input
HEADERS += mainwindow.h myparam.h percol2d.h plotter.h
SOURCES += main.cpp mainwindow.cpp myparam.cpp percol2d.cpp plotter.cpp
include(percol.pri)
