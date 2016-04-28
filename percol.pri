INCLUDEPATH += "$$(MKLROOT)/include"
QMAKE_LIBDIR += "$$(MKLROOT)/lib/ia32"
QMAKE_LIBDIR += "$$(MKLROOT)/../compiler/lib/ia32"
LIBS += mkl_intel_c_dll.lib
LIBS += mkl_intel_thread_dll.lib
LIBS += mkl_core_dll.lib
LIBS += libiomp5md.lib
