#!/bin/bash
. ~/env.sh
mpirun -np 2 -machinefile ~/machines ~/aout_run.sh
