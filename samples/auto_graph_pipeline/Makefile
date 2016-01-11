rows=100                     # Number of rows of any matrix involved, which is assumed to be square
block_rows=10                # The number of rows in a block.
use_composed=-DUSE_COMPOSED  # Enable composed functions (i.e., composed graphs). Comment out right hand side to disable
validate=-DVALIDATE          # Enable printing checksum of the results. Comment out right hand side to disable

# Imagine the following has happened:
# (1) There was a user program containing a sequence of calls to functions
# in a library.
# (2) A compiler (not necessarily the CXX here) analyzed the user program,  
# automatically composed the library functions into a single new function in a
# C++ file (auto_composed_graph.cpp), and exposed the new function in C interface.
# (3) The above compiler replaced the calls in the user program with a call to
# the new function. 
#
# The CXX compiler here compiles the user program and the C++ file into an
# executable.
# Note: in general, if the user program is written in a language different from
# C/C++, one can compile the new C++ files into a binary library first, and
# invoke the library from the user program, instead.

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
BLAS_LD_FLAGS = -L$(MKLROOT)/lib/intel64 -Wl,-rpath -Wl,$(MKLROOT)/lib/intel64 -lmkl_intel_lp64 -lmkl_gnu_thread -lmkl_core -ldl -lpthread -lm
BLAS_OPTS = -I$(MKLROOT)/include -DUSE_MKL -fopenmp 
else
BLAS_LD_FLAGS = -lblas
endif

#CXX=icpc
OPT := -pthread $(OPTS) -O3 $(BLAS_OPTS) -std=c++0x
CPPFLAGS += -DSZ=$(rows) -DBR=$(block_rows) $(use_composed) $(validate) \
            -I./src/expert_written_wrappers -I./src/auto_composed_graph

APPNAME = auto_graph_pipeline

TARGETS   := $(APPNAME) dist$(APPNAME)
OBJS      := hello_world.o auto_composed_graph.o
DIST_OBJS := $(addprefix dist,$(OBJS))
DEST_OBJS := $(OBJS) $(DIST_OBJS)
HEADERS   := src/expert_written_wrappers/*.h src/auto_composed_graph/*.cpp src/auto_composed_graph/*.h

VPATH = src/user_program src/auto_composed_graph

all: $(TARGETS)

$(APPNAME): $(OBJS)
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) $(BLAS_LD_FLAGS) -lcnc -lrt -ltbb -ltbbmalloc

dist$(APPNAME): $(DIST_OBJS)
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) $(BLAS_LD_FLAGS) -lcnc -lrt -ltbb -ltbbmalloc

%.o: %.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

%.o: %.c $(HEADERS)
	$(CXX) $(CPPFLAGS) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.c $(HEADERS)
	$(CXX) $(CPPFLAGS) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

clean:
	rm -f $(TARGETS) $(DEST_OBJS) *~ */*~ */*/*~
