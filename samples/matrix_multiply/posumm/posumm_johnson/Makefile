ARCH := intel64
M_UNAME := $(shell uname -m)
ifeq ($(M_UNAME), i686)
ARCH := ia32
endif

ifeq (,$(CNCROOT))
$(info Please estblish CnC environment variables before using this Makefile.)
$(info E.g. by running cncvars.sh or cncvars.csh)
$(info More information is available in 'Getting Started > Running the samples')
$(error CNCROOT is not set)
endif

ifeq ($(CXX), icpc)
OPENMPLIB = -openmp
FP_OPTS = -fp-model precise
OPTS := $(OPTS) -parallel -openmp 
KOPTS := -c -O3 -xhost -fno-alias -std=c99
else
OPENMPLIB = -lgomp
KOPTS := -c -O3 -fargument-noalias -std=c99
endif

ifeq ($(CXX), g++)
OPENMPLIB = -lgomp
endif


OPT_LEVEL                 := -O3 -I$(TBB)/include/ $(TIMESTEPS)
CNC_OPT                   := -static-libstdc++ -std=c++0x $(OPTS) $(OPT_LEVEL) -pthread $(FP_OPTS)
COMMON_OPT                := $(OPTS) $(OPT_LEVEL) -fopenmp $(FP_OPTS)

CNC_LIBS                  := -lcnc -ltbb -ltbbmalloc $(OPENMPLIB) -lrt -pthread

APPNAME                   = mm_johnson
CNC_APPNAME               = $(APPNAME)_cnc

CNC_DIST_APPNAME          = dist_$(CNC_APPNAME)

UTILS_SOURCES             := ../common/utils.cpp
UTILS_HEADERS             := ../common/utils.h
UTILS_OBJS                := utils.o

CNC_DEST_OBJS             := $(CNC_APPNAME).o
CNC_DIST_DEST_OBJS        := $(CNC_DIST_APPNAME).o 
CNC_HEADERS               := ../common/cnc_common.h $(UTILS_HEADERS) 
CNC_SOURCES               := $(CNC_APPNAME).cpp

CNC_APP_OBJS              := kernel.o $(CNC_DEST_OBJS) $(UTILS_OBJS) 
CNC_DIST_APP_OBJS         := kernel.o $(CNC_DIST_DEST_OBJS) $(UTILS_OBJS) 

all: $(CNC_APPNAME) $(CNC_DIST_APPNAME)
shared: $(CNC_APPNAME) 
dist: $(CNC_DIST_APPNAME)

$(CNC_APPNAME): $(CNC_APP_OBJS)
	$(CXX) $^  $(CNC_OPT) $(OPT_LEVEL) -L$(CNCROOT)/lib/$(ARCH) $(CNC_LIBS) -o $@.exe

$(CNC_DIST_APPNAME): $(CNC_DIST_APP_OBJS)
	$(CXX) $^  $(CNC_OPT) $(OPT_LEVEL) -L$(CNCROOT)/lib/$(ARCH) $(CNC_LIBS) -o $@.exe

$(CNC_DEST_OBJS): $(CNC_SOURCES) $(CNC_HEADERS) 
	$(CXX) -c -I$(CNCROOT)/include $(CNC_OPT) $(CNC_SOURCES) -o $@ 

$(CNC_DIST_DEST_OBJS): $(CNC_SOURCES) $(CNC_HEADERS) 
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(CNC_OPT) $(CNC_SOURCES) -o $@ 

utils.o: $(UTILS_SOURCES) $(UTILS_HEADERS) 
	$(CXX) -c $(COMMON_OPT) $(UTILS_SOURCES) -o $@

kernel.o: 
	$(CXX) $(KOPTS) kernel.cpp

clean:
	rm -f $(CNC_APPNAME).exe $(CNC_APP_OBJS) $(CNC_DIST_APPNAME).exe $(CNC_DIST_APP_OBJS) stats.txt conf.txt *~
