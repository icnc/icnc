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

APPNAME = mapreduce

CXX = icpc
TARGETS   := $(APPNAME) dist$(APPNAME)
DEST_OBJS := $(TARGETS:=.o)
CNCFILE   := $(APPNAME).cnc
HINTSFILE := $(APPNAME)_codinghints.txt
#GEN_HEADER=$(CNCFILE:.cnc=.h)
HEADERS  := $(APPNAME).h

OPT_DEBUG = -DCNC_USE_ASSERT -DTBB_USE_ASSERT -g
OPT_OPT   = -O3
OPT := -pthread $(OPTS) -std=c++0x

DEBUG ?= 0
ifeq ("0", "$(DEBUG)")
OPT += $(OPT_OPT)
DBG_SFX =
else
OPT += $(OPT_DEBUG)
DBG_SFX = _debug
endif

all:  $(TARGETS)

$(TARGETS): %: %.o
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) -lcnc$(DBG_SFX) -lrt -ltbb$(DBG_SFX) -ltbbmalloc$(DBG_SFX)

$(APPNAME).o: $(APPNAME).cpp $(HEADERS)
	$(CXX) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

$(GEN_HEADER): $(CNCFILE)
	$(CNCROOT)/bin/$(ARCH)/cnc $(CNCFILE)

clean:
	rm -f $(TARGETS) $(DEST_OBJS) $(GEN_HEADER) $(HINTSFILE) *~
