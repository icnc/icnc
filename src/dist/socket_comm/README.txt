- Building + running:
      make

- Building with ITAC tracing:
      USE_ITAC=1 make

- Building in Debug mode:
      USE_DEBUG=1 make

- Subtargets:
      make lib  : builds only libCnCsocket.a
      make test : builds libCnCsocket.a + test executable

- make clean

%##########################################################################

Preparation:

  Adjust the hostname(s) in client starter script test/start.sh 
  (in _CLIENT_CMD1_ etc.)

What does the test example do?

- Host starts 3 clients (number of clients defined in test/start.sh|bat)
- host sends 123 to client 1 
- client 1 sends 246 to client 2
- client 2 sends 369 to client 3
- client 3 sends 492 to host
- client 3 bcasts 5555 to host and other clients
- each client sends a "termination message" containing -111111 to the host
- termination

%##########################################################################

My output of "make" (on Linux):
===============================

bash-3.1$ make
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  SocketCommunicator.cpp -o obj/SocketCommunicator.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  SocketHost.cpp -o obj/SocketHost.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  SocketClient.cpp -o obj/SocketClient.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  ThreadExecuter.cpp -o obj/ThreadExecuter.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  pal_socket.cpp -o obj/pal_socket.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  pal_util.cpp -o obj/pal_util.o
mkdir -p obj; icc -O2 -c -DEM64T_LIN -I../.. -IBB21_INSTALL_DIR/include  Settings.cpp -o obj/Settings.o
ar rv libCnCsocket.a obj/SocketCommunicator.o obj/SocketHost.o obj/SocketClient.o obj/ThreadExecuter.o obj/pal_socket.o obj/pal_util.o obj/Settings.o
ar: creating libCnCsocket.a
a - obj/SocketCommunicator.o
a - obj/SocketHost.o
a - obj/SocketClient.o
a - obj/ThreadExecuter.o
a - obj/pal_socket.o
a - obj/pal_util.o
a - obj/Settings.o
icc -O2 -DEM64T_LIN -I. -I../.. -IBB21_INSTALL_DIR/include  test/test_main.cpp test/test_distributor.cpp ../../src/Buffer.cpp libCnCsocket.a -ltbb
env CNC_SOCKET_HOST=test/start.sh ./a.out
starting clients via script:
test/start.sh <client_id> 0:1025_111@172.28.77.180
--> established socket connection 2, 2 still missing ...
--> established socket connection 3, 1 still missing ...
--> established all socket connections to the host.
--> establishing client connections to client 1 ... done
--> establishing client connections to client 2 ... done
PROC 0: received -111111 from 1
PROC 1: received 123 from 0
PROC 2: received 246 from 1
PROC 0: received -111111 from 2
PROC 3: received 369 from 2
PROC 1: received 5555 from 3
PROC 2: received 5555 from 3
PROC 0: received 492 from 3
PROC 0: received 5555 from 3
PROC 0: received -111111 from 3
