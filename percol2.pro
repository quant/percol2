######################################################################
# Automatically generated by qmake (2.01a) ?? 31. ??? 00:08:28 2009
######################################################################

CONFIG += debug
TEMPLATE = app
TARGET =
DEPENDPATH += .
INCLUDEPATH += .

# Input
HEADERS += mainwindow.h myparam.h percol2d.h plotter.h
SOURCES += main.cpp mainwindow.cpp myparam.cpp percol2d.cpp plotter.cpp
#The following line was inserted by qt3to4
QT +=  qt3support
win32 {
include(percol.pri)
}
unix {
LIBS += -lmkl_intel_lp64
LIBS += -lmkl_intel_thread
LIBS += -lmkl_core
LIBS += -liomp5
LIBS += -lpthread
}

# Local variables:
# mode: makefile
# End:
