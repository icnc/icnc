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

#CXX=icpc
OPT := -pthread $(OPTS) -O3

APPNAME = dedup

TARGETS   := $(APPNAME)
DEST_OBJS := hashtable.o queue.o util.o binheap.o tree.o dedup.o rabin.o encoder_util.o encoder_cnc.o 
CNCFILE   := $(APPNAME).cnc
HINTSFILE := $(APPNAME)_codinghints.txt
#GEN_HEADER=$(CNCFILE:.cnc=.h)
HEADERS  := binheap.h dedup_debug.h dedupdef.h hashtable.h queue.h tree.h config.h decoder.h encoder.h hashtable_private.h rabin.h util.h encoder_util.h

all:  $(TARGETS)

$(TARGETS): $(DEST_OBJS)
	$(CXX) $(OPT) -o $@ $^ -L$(CNCROOT)/lib/$(ARCH) -lcnc -ltbb -ltbbmalloc -lz -lm -lssl -lrt -lcrypto

$(DEST_OBJS): %.o: %.cpp $(HEADERS)
	$(CXX) -c -I$(CNCROOT)/include $(OPT) -o $@ $<

dist%.o: %.cpp $(HEADERS)
	$(CXX) -c -D_DIST_ -I$(CNCROOT)/include $(OPT) -o $@ $<

$(GEN_HEADER): $(CNCFILE)
	$(CNCROOT)/bin/$(ARCH)/cnc $(CNCFILE)

clean:
	rm -f $(TARGETS) $(DEST_OBJS) $(GEN_HEADER) $(HINTSFILE) *~

