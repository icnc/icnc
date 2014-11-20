# Makefile Copied from Cholesky example
# https://github.com/icnc/icnc/tree/master/samples/cholesky/cholesky

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

#CXX=g++
OPT := -pthread $(OPTS) -std=c++0x -O3 $(CPPFLAGS)

APPNAME = jacobi2D

TARGETS   := $(APPNAME) # dist$(APPNAME)
DEST_OBJS := $(TARGETS:=.o) $(TARGETS:=_cnc.o)
CNCFILE   := $(APPNAME).cnc
HEADERS  := $(APPNAME).h

all:  $(TARGETS)

$(TARGETS): %: %.o %_cnc.o 
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) -lcnc -lrt -ltbb -ltbbmalloc

%.o: %.cpp $(HEADERS)
	$(CXX) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

clean:
	rm -f $(TARGETS) $(DEST_OBJS)
