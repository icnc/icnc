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

override CXX := mpicxx -mt_mpi

OPT := -pthread $(OPTS) -std=c++0x -g -DCNC_USE_ASSERT
# -O3 -DNDEBUG -ansi-alias
APPNAME = matrix_inverse

TARGETS   := $(APPNAME) dist$(APPNAME)
DEST_OBJS := $(TARGETS:=.o)
CNCFILE   := $(APPNAME).cnc
HINTSFILE := $(APPNAME)_codinghints.txt
#GEN_HEADER=$(CNCFILE:.cnc=.h)
#HEADERS  := $(APPNAME).h

all:  $(TARGETS)

$(TARGETS): %: %.o
	$(CXX) $(OPT) -o $@ $< -L$(CNCROOT)/lib/$(ARCH) -lcnc_debug -lrt -ltbb -ltbbmalloc

$(APPNAME).o: $(APPNAME).cpp $(HEADERS)
	$(CXX) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

$(GEN_HEADER): $(CNCFILE)
	$(CNCROOT)/bin/$(ARCH)/cnc $(CNCFILE)

clean:
	rm -f $(TARGETS) $(DEST_OBJS) $(GEN_HEADER) $(HINTSFILE) *~
