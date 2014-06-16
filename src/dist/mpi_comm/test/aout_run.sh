#!/bin/bash
. ~/env.sh
outfilename=`hostname`_out
errfilename=`hostname`_err
~/a.out 100 1 1>$outfilename 2>$errfilename
