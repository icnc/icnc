import sys
import os
launcher={ 'shared' : '',
           'MPI'    : 'mpirun -genv DIST_CNC=MPI -n 4',
           'SOCKETS': 'env DIST_CNC=SOCKETS CNC_SOCKET_HOST=$CNCROOT/etc/dist/sockets/start.sh',
           'SHMEM'  : 'env DIST_CNC=SHMEM'
}
cmd=launcher[sys.argv[2]] + " " + sys.argv[1] + " " + " ".join(sys.argv[4:]) + " > " + sys.argv[3]
print cmd
os.system(cmd)
