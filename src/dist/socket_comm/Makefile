#
# With ITAC flags?
#
ifeq "$(USE_ITAC_INTERNAL)" "1"
USE_ITAC = 1
ITAC_CFLAGS = -DINTERNAL_ITAC
endif
ifeq "$(USE_ITAC)" "1"
ITAC_CFLAGS += -DCNC_WITH_ITAC -DINTERNAL_ITAC -I$(VT_ROOT)/include
ITAC_LFLAGS = -L$(VT_LIB_DIR) -lVTcs $(VT_ADD_LIBS)
endif

#
# With Debug flags?
#
ifneq "$(USE_DEBUG)" ""
CFLAGS = -O0 -g
endif
ifeq "$(USE_DEBUG)" ""
CFLAGS = -O2
endif

#CNC_DIR = ../..
CNC_DIR = ../cnc_dummy

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

CC     = icc $(CFLAGS)
DFLAGS = -DEM64T_LIN -DCNC_STATIC_LIBS # -DWITHOUT_SENDER_THREAD -DWITHOUT_SENDER_THREAD_TRACING
IFLAGS = -I$(CNC_DIR) -I../generic_comm -I$(TBB21_INSTALL_DIR)/include $(ITAC_CFLAGS)

SOCKETCOMM_SOURCES = GenericCommunicator.cpp \
                     ThreadExecuter.cpp \
                     ChannelInterface.cpp \
                     SocketCommunicator.cpp \
                     SocketChannelInterface.cpp \
                     SocketHostInitializer.cpp \
                     SocketClientInitializer.cpp \
                     pal_socket.cpp \
                     pal_util.cpp \
                     Settings.cpp

SOCKETCOMM_OBJS = $(SOCKETCOMM_SOURCES:%.cpp=obj/%.o)

vpath %.cpp .:../generic_comm

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

all: run

$(SOCKETCOMM_OBJS) : obj/%.o : %.cpp
	mkdir -p obj; $(CC) -c $(DFLAGS) $(IFLAGS) $< -o $@

lib: $(SOCKETCOMM_OBJS)
	ar rv libCnCsocket.a $(SOCKETCOMM_OBJS)

test: lib
	$(CC) $(DFLAGS) -I. $(IFLAGS) test/test_main.cpp test/test_distributor.cpp $(CNC_DIR)/src/Buffer.cpp libCnCsocket.a -ltbb $(ITAC_LFLAGS)

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

run: test
	env CNC_SOCKET_HOST=test/start.sh ./a.out

#%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

clean:
	rm -f obj/*.o a.out *.a a.out.*stf* a.out.prot
	rm -f *.ncb *.suo */*.vcproj.*.user
	rm -rf obj debug release vt_release Debug Release VT_Release x64
	rm -rf */debug */release */vt_release */Debug */Release */VT_Release */x64
	rm -f test/test*.stf*
