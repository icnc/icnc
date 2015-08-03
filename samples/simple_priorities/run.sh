#!/bin/bash

make || exit 1

export CNC_SCHEDULER=FIFO_SINGLE
#export CNC_SCHEDULER=FIFO_STEAL
#export CNC_SCHEDULER=FIFO_AFFINITY

export CNC_PIN_THREADS=1

export CNC_NUM_THREADS=1

[ "$1" = 1 ] && export CNC_USE_PRIORITY=1

./simple_priorities
