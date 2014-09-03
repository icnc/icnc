TOP = ..
include $(TOP)/Makefile.common

APPS = sorDataTiled_NewApi
TESTS = sorDataTiled_NewApi_w03_100x100_10x10_x50

build: $(APPS)
test: $(TESTS:=.test)


# Rules to build applications
#
sorDataTiled_NewApi.h: sorDataTiled.cnc
	$(DEST_DIR)/bin/cnc $<

##  new api
##
sorDataTiled_NewApi: sorDataTiled_NewApi.o
	$(CXX) -o $@ $< -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sorDataTiled_NewApi.o: sorDataTiled_NewApi.cpp 
	$(CXX) $(TBBDEBUGFLAG) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

# Utility rules
#
$(TESTS:=.tested) $(TESTS:=.failed):
	touch $@
 
clean:
	rm -f $(TESTS:=.tested) $(TESTS:=.failed) $(TESTS:=.out) $(APPS) sorDataTiled.o sorDataTiled.h sorDataTiled_codinghints.txt sorDataTiled_NewApi.o

# Rules to run tests
#
sorDataTiled_NewApi_w03_100x100_10x10_x50.test: sorDataTiled_NewApi sorDataTiled_NewApi_w03_100x100_10x10_x50.tested sorDataTiled_NewApi_w03_100x100_10x10_x50.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./sorDataTiled_NewApi 0.3 100 100 50 10 10 -v > $(@:.test=.out)
	diff sorDataTiled_NewApi_w03_100x100_10x10_x50.compare sorDataTiled_NewApi_w03_100x100_10x10_x50.out -I ^Computed
	rm -f $(@:.test=.failed)
