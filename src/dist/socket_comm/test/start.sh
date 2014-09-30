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
# start.sh, this is the script file to perform startup activities
# when running with distCnC over sockets. More information can be
# found in the runtime_api documentation: CnC for distributed memory.

 # !!! Adjust the number of expected clients here !!!
_N_CLIENTS_=3
#__my_xterm="env DISPLAY=$DISPLAY xterm -e"
#__my_debugger="gdb --args"

mode=$1
contactString=$2

 # Special mode: query number of clients
if [ "$mode" = "-n" ]; then
    echo $_N_CLIENTS_
    exit 0
fi

# Normal mode: start client process 

: ${CNC_NUM_THREADS:=-1}
: ${CNC_SCHEDULER:=TBB_TASK}

__my_exe="$CNC_SOCKET_HOST_EXECUTABLE"
__my_wdir=`pwd`

# determine hostname
if [ -r "$HOST_FILE" ]; then
    echo "Using hostfile $HOST_FILE..." 1>&2
    __thishost=`hostname`
    __nhosts=`grep -v -e '^$' $HOST_FILE | grep -v $__thishost | wc -l`
    __hostId=$(($mode % $__nhosts))
    if [ "$__hostId" = "0" ]; then
	__hostId=$__nhosts
    fi
    __my_host=`cat $HOST_FILE | grep -v -e '^$' | grep -v $__thishost | head -n $__hostId | tail -n 1`
else
    __my_host=localhost
fi

_CLIENT_CMD1_="ssh $__my_host 'cd $__my_wdir && $__my_xterm env CNC_NUM_THREADS=$CNC_NUM_THREADS env CNC_SOCKET_CLIENT=$contactString env DIST_CNC=SOCKETS env LD_LIBRARY_PATH=$LD_LIBRARY_PATH $__my_debugger $__my_exe $CLIENT_ARGS'"

# start client process 
#echo $_CLIENT_CMD1_
eval exec $_CLIENT_CMD1_
