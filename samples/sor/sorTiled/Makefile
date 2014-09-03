TOP = ..
include $(TOP)/Makefile.common

APPS = sorTiled_NewApi
TESTS = sorTiled_NewApi_w03_100x100_x50_10x10

build: $(APPS)
test: $(TESTS:=.test)


# Rules to build applications
#
sorTiled_NewApi.h: sorTiled.cnc
	$(DEST_DIR)/bin/cnc $<

##  new api
##
sorTiled_NewApi: sorTiled_NewApi.o
	$(CXX) -o $@ $< -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sorTiled_NewApi.o: sorTiled_NewApi.cpp
	$(CXX) $(TBBDEBUGFLAG) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

# Utility rules
#
$(TESTS:=.tested) $(TESTS:=.failed):
	touch $@
 
clean:
	rm -f $(TESTS:=.tested) $(TESTS:=.failed) $(TESTS:=.out) $(APPS) sorTiled.o sorTiled.h sorTiled_codinghints.txt sorTiled_NewApi.o

# Rules to run tests
#
sorTiled_NewApi_w03_100x100_x50_10x10.test: sorTiled_NewApi sorTiled_NewApi_w03_100x100_x50_10x10.tested sorTiled_NewApi_w03_100x100_x50_10x10.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./sorTiled_NewApi 0.3 100 100 50 10 10 -v > $(@:.test=.out)
	diff sorTiled_NewApi_w03_100x100_x50_10x10.compare sorTiled_NewApi_w03_100x100_x50_10x10.out -I ^Computed.
	rm -f $(@:.test=.failed)
