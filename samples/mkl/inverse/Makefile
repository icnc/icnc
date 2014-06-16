OPT := -pthread $(OPTS) -fast

ICXX=icpc
MPIICXX=mpiicc

all: mkl scalapack

scalapack: matrix_inverse_scalapack.cpp
	$(MPIICXX) $(OPT) -o matrix_inverse_scalapack.o -c matrix_inverse_scalapack.cpp
	$(MPIICXX)$(OPT) -O3 -o matrix_inverse_scalapack matrix_inverse_scalapack.o -lmkl_scalapack_lp64 -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core  -lmkl_blacs_intelmpi_lp64 -openmp -tbb -lpthread

mkl:  matrix_inverse_mkl.cpp
	$(ICXX) $(OPT) -o matrix_inverse_mkl.o -c  matrix_inverse_mkl.cpp
	$(ICXX) $(OPT) -O3 -o matrix_inverse_mkl  matrix_inverse_mkl.o -lmkl_blas95_lp64 -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -fopenmp -tbb -lpthread

clean:
	rm -f *.o matrix_inverse_mkl matrix_inverse_scalapack
