#!/bin/sh

if [ -z "$PBS_NODEFILE" ]; then
    echo "\$PBS_NODEFILE is not defined."
    exit 11
fi
_host_=$(hostname)
_N_CLIENTS_=$(grep -v $_host_ $PBS_NODEFILE | sort -u | grep -c ^)

#==========================================================================
# Special mode: query number of clients

if [ "$1" = "-n" ]; then
    echo $_N_CLIENTS_
    exit 0
fi

#==========================================================================
# other mode: launch client

mode=$1
contactString=$2
clientId=$mode

_CLIENT_LIST_=$(cat $PBS_NODEFILE | grep -v $_host_)
__my_wdir=`pwd`
__my_exe="$CNC_SOCKET_HOST_EXECUTABLE"
_CLIENT_CMD_="'cd $__my_wdir && env CNC_SOCKET_CLIENT=$contactString env CNC_SOCKET_CLIENT_ID=$clientId env DIST_CNC=SOCKETS env LD_LIBRARY_PATH=$LD_LIBRARY_PATH $__my_exe'"

# client id starts with 1 (0 is master)
i=1
for client in $_CLIENT_LIST_
do
    if [ "$i" = "$clientId" ]; then
        eval exec "ssh $client "$_CLIENT_CMD_
        exit 0
    fi
    i=$((i+1))
done
