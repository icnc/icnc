ARCH=intel64
OPT := $(OPTS) -O3 -DUSE_MKL

CXX=g++
ICXX=icpc
MPICXX=mpiicc

all: cnc distcnc mkl scalapack

cnc: cholesky.cpp cholesky_cnc.cpp cholesky.h cholesky_types.h init.o
	$(CXX) -std=c++0x $(OPT) -pthread -I$(CNCROOT)/include -o cholesky.o -c cholesky.cpp
	$(CXX) -std=c++0x $(OPT) -pthread -I$(CNCROOT)/include -o cholesky_cnc.o -c cholesky_cnc.cpp
	$(CXX) -std=c++0x -O3 -pthread -o cholesky_cnc  cholesky.o  cholesky_cnc.o init.o -L$(CNCROOT)/lib/$(ARCH)  -L$(MKLROOT)/lib/em64t -Wl,-rpath -Wl,$(MKLROOT)/lib/intel64 -lmkl_intel_lp64 -lmkl_sequential -lmkl_blas95_lp64 -lmkl_core -ltbb -lpthread -lcnc$(DBG) -lrt

distcnc: cholesky.cpp cholesky_cnc.cpp cholesky.h cholesky_types.h
	$(CXX) -std=c++0x -D_DIST_ $(OPT) -pthread -I$(CNCROOT)/include -o dcholesky.o -c cholesky.cpp
	$(CXX) -std=c++0x -D_DIST_ $(OPT) -pthread -I$(CNCROOT)/include -o dcholesky_cnc.o -c cholesky_cnc.cpp
	$(CXX) -std=c++0x -pthread -O3 -o cholesky_distcnc dcholesky.o  dcholesky_cnc.o init.o -L$(CNCROOT)/lib/$(ARCH)  -L$(MKLROOT)/lib/em64t -Wl,-rpath -Wl,$(MKLROOT)/lib/intel64 -lmkl_intel_lp64 -lmkl_sequential -lmkl_blas95_lp64 -lmkl_core -liomp5 -lpthread -lcnc$(DBG) -lrt -ltbb$(DBG) -ltbbmalloc$(DBG)

scalapack: cholesky_scalapack.cpp init.o
	$(MPICXX) $(OPT) -pthread -o cholesky_scalapack.o -c cholesky_scalapack.cpp
	$(MPICXX) $(OPT) -pthread -O3 -o cholesky_scalapack init.o cholesky_scalapack.o -lmkl_scalapack_lp64 -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core  -lmkl_blacs_intelmpi_lp64 -openmp -tbb -lpthread

mkl: cholesky_scalapack.cpp init.o
	$(CXX) $(OPT) -pthread -o cholesky_mkl.o -c cholesky_mkl.cpp
	$(ICXX) $(OPT) -pthread -O3 -o cholesky_mkl init.o cholesky_mkl.o -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_blas95_lp64 -lmkl_core -liomp5 -ltbb -lpthread -lrt
#-lmkl_lapack_lp64 -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core  -lmkl_blacs_intelmpi_lp64 -openmp -tbb -lpthread

mkl_seq: cholesky_scalapack.cpp init_seq.o
	$(CXX) $(OPT) -o cholesky_mkl.o -c cholesky_mkl.cpp
	$(ICXX) $(OPT) -O3 -o cholesky_mkl_seq init_seq.o cholesky_mkl.o -lmkl_intel_lp64 -lmkl_core -lmkl_sequential -lpthread -lm -lrt

init.o: init.cpp
	$(CXX) $(OPT) -pthread -o $@ -c $<
init_seq.o: init.cpp
	$(CXX) $(OPT) -DSEQ -o $@ -c $<

clean:
	rm -f *.o cholesky_mkl cholesky_scalapack cholesky_distcnc cholesky_cnc
