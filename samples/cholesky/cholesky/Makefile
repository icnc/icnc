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

ifeq ($(USE_MKL),1)
MKL_LD_FLAGS = -L$(MKLROOT)/lib/intel64 -Wl,-rpath -Wl,$(MKLROOT)/lib/intel64 -lmkl_intel_lp64 -lmkl_gnu_thread -lmkl_core -ldl -lpthread -lm
#-lmkl_intel_lp64 -lmkl_sequential -lmkl_core -lpthread -lm
#-lmkl_intel_lp64 -lmkl_sequential -lmkl_blas95_lp64 -lmkl_core -liomp5
MKL_OPTS = -DUSE_MKL -fopenmp 
endif


#CXX=g++
OPT := -pthread $(OPTS) -std=c++0x -O3 $(MKL_OPTS)

APPNAME = cholesky

TARGETS   := $(APPNAME) dist$(APPNAME)
DEST_OBJS := $(TARGETS:=.o) $(TARGETS:=_cnc.o) init.o
CNCFILE   := $(APPNAME).cnc
HINTSFILE := $(APPNAME)_codinghints.txt
#GEN_HEADER=$(CNCFILE:.cnc=.h)
HEADERS  := $(APPNAME).h

all:  $(TARGETS)

$(TARGETS): %: %.o %_cnc.o init.o
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) $(MKL_LD_FLAGS) -lcnc -lrt -ltbb -ltbbmalloc

%.o: %.cpp $(HEADERS)
	$(CXX) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

$(GEN_HEADER): $(CNCFILE)
	$(CNCROOT)/bin/$(ARCH)/cnc $(CNCFILE)

clean:
	rm -f $(TARGETS) $(DEST_OBJS) $(GEN_HEADER) $(HINTSFILE) *~
