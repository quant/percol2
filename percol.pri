win32 {
INCLUDEPATH += "$$(MKLROOT)/include"
QMAKE_LIBDIR += "$$(MKLROOT)/lib/ia32"
QMAKE_LIBDIR += "$$(MKLROOT)/../compiler/lib/ia32"
LIBS += mkl_intel_c_dll.lib
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
