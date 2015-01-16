#!/bin/bash

########################################################################
# posumm_cannon/build-and-run.sh: This file is part of the POSUMM project.
#
# POSUMM: Portable OSU Matrix-Multiply, CnC implementations.
#
# Copyright (C) 2014,2015 Ohio State University, Columbus, Ohio
#
# This program can be redistributed and/or modified under the terms
# of the license specified in the LICENSE.txt file at the root of the
# project.
#
# Contact: Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>
#
#
# @file: posumm_cannon/build-and-run.sh
# @author: Martin Kong <kongm@cse.ohio-state.edu>
#########################################################################

source ${MPIDIR}/impi_latest/bin64/mpivars.sh
source ${CNCDIR}/bin/cncvars.sh
module load intel/latest

export DIST_CNC=MPI
export CNC_NUM_THREADS=1
export OMP_NUM_THREADS=1
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/lib:/usr/lib64
export CXX=icpc

## Intel MPI options
export I_MPI_DEBUG=5
## The PMI library should be here
export I_MPI_PMI_LIBRARY=/usr/lib64/libpmi.so
## To avoid some correctness issues with DAPL
export I_MPI_DAPL_TRANSLATION_CACHE=0
## Verbose debugging with IMPI
export I_MPI_STATS=5
## To guarantee that even "big" messages are sent out immediately
export I_MPI_EAGER_THRESHOLD=20000kb
## To use RDMA
export I_MPI_SHM_BYPASS=1
## To guarantee that even "big" messages are sent out immediately
export I_MPI_INTRANODE_EAGER_THRESHOLD=20000kb


if [ $# -ne 7 ]; then
  echo "Need: <#machines> <#total-tasks> <#tasks-per-socket> <problem size> <block_size> <check:0/1> <compile:1/0>"
  exit
fi

## Number of machines
M=$1
## Total number of tasks
T=$2
## Number of tasks per socket
S=$3
## Number of tasks per node
TPC=$(($T/$M))

## Matrix size
N=$4
## Block size
B=$5
## Check flag: 1 for check, 0 for don't check
check=$6

slurmcom="srun --distribution=block --contiguous --time=12:00  --nodes=$M --ntasks-per-socket=$S --ntasks=$T --ntasks-per-node=$TPC --threads-per-core=1  --ntasks-per-core=1  "
fcheck=""

if [ "$check" -eq 1 ]; then
  fcheck="-check"
fi

if [ "x$7" == "x1" ]; then
make clean
make
fi

$slurmcom ./dist_mm_cannon_cnc.exe  -b $B -m $M -n $N -p $T -cnc $fcheck 
echo "Used: nodes=$M tasks=$T tasks-per-node=$TPC pbs=$N block=$B"
