#! /bin/sh
# ********************************************************************************
#  Copyright (c) 2008-2013 Intel Corporation. All rights reserved.              **
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
#
# start.bat, this is the script file to perform startup activities
# when running with distCnC over sockets. More information can be
# found in the runtime_api documentation: CnC for distributed memory.

mode=$1
contactString=$2
clientId=$mode

#t3
__my_args="-t 0 -b 2000 -q 0.124875 -m 8 -r 42"
# t3l
__my_args="-t 0 -b 2000 -q 0.200014 -m 5 -r 7"
# t3xxl
#__my_args="-t 0 -b 2000 -q 0.499995 -m 2 -r 316"
# t1xl
#__my_args="-t 1 -a 3 -d 15 -b 4 -r 29"
 # !!! Adjust the number of expected clients here !!!
_N_CLIENTS_=3

__my_wdir=`pwd`
#__my_debugger=bin/insight
               # (you may have to add -X to the ssh commands below,
               # and specify CNC_SOCKETS_START_CLIENTS_IN_ORDER=1 to the host)
#__my_debugger="valgrind-3.3.1/bin/valgrind --leak-check=full"
__my_exe="$CNC_SOCKET_HOST_EXECUTABLE"

 # !!! Client startup commands !!!
 # !!! There must be one for each client 1,2,...,_N_CLIENTS_ !!!
 # The host's contact string must be transferred to the client processes
 # via the environment variable CNC_SOCKET_CLIENT.
_CLIENT_CMD1_="ssh localhost 'cd $__my_wdir &&  env TBB_VERSION=1 env CNC_NUM_THREADS=$CNC_NUM_THREADS env CNC_SOCKET_CLIENT=$contactString env CNC_SOCKET_CLIENT_ID=$clientId env DIST_CNC=SOCKETS env LD_LIBRARY_PATH=$LD_LIBRARY_PATH $__my_debugger $__my_exe $__my_args'"
_CLIENT_CMD2_=$_CLIENT_CMD1_
_CLIENT_CMD3_=$_CLIENT_CMD1_
_CLIENT_CMD4_=$_CLIENT_CMD1_
_CLIENT_CMD5_=$_CLIENT_CMD1_
_CLIENT_CMD6_=$_CLIENT_CMD1_
_CLIENT_CMD7_=$_CLIENT_CMD1_
#_CLIENT_CMD8_=........

#==========================================================================

 # Special mode: query number of clients
if [ "$mode" = "-n" ]; then
    echo $_N_CLIENTS_
    exit 0
fi

 # Normal mode: start client process with id $mode (1,2,...,_N_CLIENTS_).
 # Its startup command is assumed to be defined via the environment
 # variable _CLIENT_CMD<id>_ as specified above.
eval clientCmd=\$_CLIENT_CMD${clientId}_
if [ -z "$clientCmd" ]; then
    echo "*** ERROR from $0: no client command _CLIENT_CMD${mode}_ specified ..."
    exit 1
fi
eval exec $clientCmd
