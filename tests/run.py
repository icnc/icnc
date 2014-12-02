import sys
import os
launcher={ 'shared' : 'env ' + sys.argv[4],
           'MPI'    : 'mpirun -genv ' + sys.argv[4] + ' -genv DIST_CNC=MPI -n 4',
           'SOCKETS': 'env ' + sys.argv[4] + ' DIST_CNC=SOCKETS CNC_SOCKET_HOST=$CNCROOT/misc/distributed/socket/start.sh',
           'SHMEM'  : 'env ' + sys.argv[4] + ' DIST_CNC=SHMEM'
}
cmd=launcher[sys.argv[2]] + " " + sys.argv[1] + " " + " ".join(sys.argv[5:]) + " > " + sys.argv[3]
print "run.py: " + cmd + "(" + sys.argv[4] + ")"
os.system(cmd)
