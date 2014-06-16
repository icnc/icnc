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

CXX=icpc -I/usr/include/x86_64-linux-gnu/c++/4.7
OPT := -pthread $(OPTS) -std=c++0x
# -DUSE_BSIZEX=80 -DUSE_BSIZEY=80 -DUSE_BSIZEZ=80
#-DDUMP 

APPNAME = rtm_stencil_halo

TARGETS   := $(APPNAME) dist$(APPNAME)
FILES     := $(APPNAME).cpp stencil.h rtm_tags.h tile.h halo_tile.h
CNCFILE   := $(APPNAME).cnc
HINTSFILE := $(APPNAME)_codinghints.txt
#GEN_HEADER=$(CNCFILE:.cnc=.h)
HEADERS  := $(APPNAME).h
OF=y_points.txt

all:  $(TARGETS)

$(TARGETS): %: %.o
	$(CXX) $(OPT) -o $@ $< -L$(CNCROOT)/lib/$(ARCH) -lcnc -lrt -ltbb -ltbbmalloc -lrt

itac$(APPNAME): %: %.o
	mpiicpc -trace -mt_mpi -o $@ $< -L$(CNCROOT)/lib/$(ARCH) -lcnc -lrt -ltbb -ltbbmalloc -lrt
#-L$(VT_SLIB_DIR) -lVT $(VT_ADD_LIBS) -L$(I_MPI_ROOT)/lib64 -lmpi

$(APPNAME).o: $(FILES)
	$(CXX) -c -o $@ $(APPNAME).cpp $(OPT) -DOUTFILE=$(OF) -I$(CNCROOT)/include

dist$(APPNAME).o: $(FILES)
	$(CXX) -c -o $@ $(APPNAME).cpp $(OPT) -D_DIST_ -DOUTFILE=$(OF) -I$(CNCROOT)/include

itac$(APPNAME).o: $(FILES)
	$(CXX) -c -o $@ $(APPNAME).cpp $(OPT) -D_DIST_ -DCNC_WITH_ITAC -DOUTFILE=$(OF) -I$(CNCROOT)/include -I$(VT_ROOT)/include

$(GEN_HEADER): $(CNCFILE)
	$(CNCROOT)/bin/$(ARCH)/cnc $(CNCFILE)

clean:
	rm -f $(TARGETS) *.o $(GEN_HEADER) $(HINTSFILE) *~
