INCLUDEPATH += "$$(MKLROOT)/include"
win32: LIBS += -L"$$(MKLROOT)\ia32\lib"
win32: LIBS += -L"$$(MKLROOT)\..\lib\ia32"
LIBS += mkl_intel_c_dll.lib
LIBS += mkl_intel_thread_dll.lib
LIBS += mkl_core_dll.lib
LIBS += libiomp5md.lib
