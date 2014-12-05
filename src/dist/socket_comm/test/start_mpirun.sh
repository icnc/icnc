#! /bin/sh
# ********************************************************************************
#  Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
#                                                                               **
#  Redistribution and use in source and binary forms, with or without           **
#  modification, are permitted provided that the following conditions are met:  **
#    * Redistributions of source code must retain the above copyright notice,   **
#      this list of conditions and the following disclaimer.                    **
#    * Redistributions in binary form must reproduce the above copyright        **
#      notice, this list of conditions and the following disclaimer in the      **
#      documentation and/or other materials provided with the distribution.     **
#    * Neither the name of Intel Corporation nor the names of its contributors  **
#      may be used to endorse or promote products derived from this software    **
#      without specific prior written permission.                               **
#                                                                               **
#  This software is provided by the copyright holders and contributors "as is"  **
#  and any express or implied warranties, including, but not limited to, the    **
#  implied warranties of merchantability and fitness for a particular purpose   **
#  are disclaimed. In no event shall the copyright owner or contributors be     **
#  liable for any direct, indirect, incidental, special, exemplary, or          **
#  consequential damages (including, but not limited to, procurement of         **
#  substitute goods or services; loss of use, data, or profits; or business     **
#  interruption) however caused and on any theory of liability, whether in      **
#  contract, strict liability, or tort (including negligence or otherwise)      **
#  arising in any way out of the use of this software, even if advised of       **
#  the possibility of such damage.                                              **
# ********************************************************************************
#
# start_batch.sh, this is the script file to perform startup activities
# when running with distCnC over sockets. General information can be
# found in the runtime_api documentation: CnC for distributed memory.

# This script starts all client processes in one go using mpirun.
# For an example of how to start processes individually see start.sh.

# You can use the following environment variables to configure the behavior of this script:
#   CNC_CLIENT_EXE  sets the client executable to start
#   CNC_CLIENT_ARGS sets the arguments passed to the client processes/executables
#   CNC_HOST_FILE   sets the file to read hostnames from on which client proceses will be started
#   CNC_NUM_CLIENTS sets the number of client processes to start
# other CnC env vars (like CNC_NUM_THREADS and CNC_SCHEDULER) can or course be used in the normal way.

 # !!! Adjust the number of expected clients here !!! (or set the env var accordingly)
: ${CNC_NUM_CLIENTS:=3}

#__my_xterm="env DISPLAY=$DISPLAY xterm -e"
#__my_debugger="gdb --args"

mode=$1

 # Special mode: query number of clients
if [ "$mode" = "-n" ]; then
    # we prepend '+' to indicate that we're going to start all clients at once
    echo "+$CNC_NUM_CLIENTS"
    exit 0
fi

# Normal mode: start client process 

contactString=$2

: ${CNC_NUM_THREADS:=-1}
: ${CNC_SCHEDULER:=TBB_TASK}

__my_exe="$CNC_HOST_EXECUTABLE"
__my_args="$CNC_HOST_ARGS"
__my_wdir=`pwd`

# determine hostname
if [ -r "$CNC_HOST_FILE" ]; then
    __my_hostarg="-f $CNC_HOST_FILE"
else
    __my_hostarg=""
fi

if [ -z "$CNC_CLIENT_EXE" ]; then
    CNC_CLIENT_EXE=$__my_exe
fi
if [ -z "$CNC_CLIENT_ARGS" ]; then
    CNC_CLIENT_ARGS=$__my_args
fi

_CLIENT_CMD1_="mpirun -genv DIST_CNC=SOCKETS -genv CNC_SOCKET_CLIENT=$contactString $__my_hostarg -n $CNC_NUM_CLIENTS $__my_xterm $__my_debugger $CNC_CLIENT_EXE $CNC_CLIENT_ARGS"

# start client process 
eval exec $_CLIENT_CMD1_
