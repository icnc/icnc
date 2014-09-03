TOP = ..
include $(TOP)/Makefile.common


# RRN: If CXX doesn't exist, we are willing to just use g++ in the path.
# This is an obscure method for checking if a file exist in make:
ifeq ($(strip $(wildcard $(CXX))),)
  CXX = g++
endif



APPS =  ThreadRing ThreadRing_OneStepCollection Stream_wState

FLAGS = -O3
#FLAGS = -O0 -g -m32

build: $(APPS:=.run)
test: $(APPS:=.test)

# Uses the new API:

%.run: %.o
	$(PREFIX) $(CXX) -o $@ $< -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

%.o: %.cpp
	$(PREFIX) $(CXX) $(TBBDEBUGFLAG) -c $(FLAGS) -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<


ThreadRing.test: ThreadRing.run ThreadRing.tested ThreadRing.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./ThreadRing.run 1000000 > $(@:.test=.out)
	$(PYTHON) ../compare.py -p $(@:.test=.compare) -c $(@:.test=.out) -q
	rm -f $(@:.test=.failed)

ThreadRing_OneStepCollection.test: ThreadRing_OneStepCollection.run ThreadRing_OneStepCollection.tested ThreadRing_OneStepCollection.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./ThreadRing_OneStepCollection.run 1000000 > $(@:.test=.out)
	$(PYTHON) ../compare.py -p $(@:.test=.compare) -c $(@:.test=.out) -q
	rm -f $(@:.test=.failed)

Stream_wState.test: Stream_wState.run Stream_wState.tested Stream_wState.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./Stream_wState.run 1000000 > $(@:.test=.out)
	$(PYTHON) ../compare.py -p $(@:.test=.compare) -c $(@:.test=.out) -q
	rm -f $(@:.test=.failed)



# Utility rules
#
$(APPS:=.tested) $(APPS:=.failed):
	touch $@

clean:
	rm -f $(APPS:=.tested) $(APPS:=.failed) $(APPS:=.out) $(APPS:=.run) *.o

