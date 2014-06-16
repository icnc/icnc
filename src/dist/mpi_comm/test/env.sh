#!/bin/bash
export TBB22_INSTALL_DIR=/export/users/nkurtov/tbb22_20090809oss/
export TBB_LIB=$TBB22_INSTALL_DIR/intel64/cc4.1.0_libc2.4_kernel2.6.16.21/lib
export TBB_INCLUDE=$TBB22_INSTALL_DIR/include
export CNC_DISTRO_ROOT=/export/users/nkurtov/cnc/t2/trunk/distro
export CNC_LIB=$CNC_DISTRO_ROOT/lib/intel64/
export CNC_INCLUDE=$CNC_DISTRO_ROOT/include/
export PYTHON_PATH=/export/users/nkurtov/python/usr/local/bin
export MPI_DIR=/usr/mpi/gcc/mvapich-1.0.1/
. $MPI_DIR/bin/mpivars.sh
export PATH=$PYTHON_PATH:$MPI_DIR/bin:$PATH
export LD_LIBRARY_PATH=/nfs/ins/home/nkurtov:$TBB_LIB:$CNC_LIB:$MPI_DIR/lib/shared/
export COMPILE_CNC="mpicc -O2 -I$TBB_INCLUDE -I$CNC_INCLUDE -L$TBB_LIB -L$CNC_LIB -ltbb -ltbbmalloc -lcnc -lcnc_mpi -DDIST -DCNC_MPI"
