QMAKE_CC = mpicc -cc=icc
QMAKE_CXX = mpicxx -cxx=icpc
QMAKE_LINK = mpicxx -cxx=icpc

INCLUDEPATH += "$$(MKLROOT)/include"
win32 {
 LIBS += -L"$$(MKLROOT)\ia32\lib"
 LIBS += -L"$$(MKLROOT)\..\lib\ia32"
 LIBS += mkl_intel_c_dll.lib
 LIBS += mkl_intel_thread_dll.lib
 LIBS += mkl_core_dll.lib
 LIBS += libiomp5md.lib
 INCLUDEPATH += "C:\Program Files\MPICH2\include"
 LIBS += "C:\Program Files\MPICH2\lib\mpi.lib"
}
unix {
LIBS += -lmkl_intel_lp64
#LIBS += -lmkl_intel_thread
LIBS += -lmkl_sequential
LIBS += -lmkl_core
LIBS += -liomp5
LIBS += -lpthread
QMAKE_CXXFLAGS = -xSSSE3 -wd981,1572,383,193,593
}

# Local variables:
# mode: makefile
# End:

