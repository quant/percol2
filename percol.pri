win32 {
INCLUDEPATH += "$$(MKLROOT)/include"
QMAKE_LIBDIR += "$$(MKLROOT)/lib/intel64"
QMAKE_LIBDIR += "$$(MKLROOT)/../compiler/lib/intel64"
LIBS += mkl_intel_lp64_dll.lib
LIBS += mkl_intel_thread_dll.lib
LIBS += mkl_core_dll.lib
LIBS += libiomp5md.lib
}
unix {
LIBS += -lmkl_intel_lp64
LIBS += -lmkl_intel_thread
LIBS += -lmkl_core
LIBS += -liomp5
LIBS += -lpthread
}
