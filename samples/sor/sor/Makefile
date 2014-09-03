TOP = ..
include $(TOP)/Makefile.common

APPS = sor
TESTS = sor_w03_100x100_x50

build: $(APPS)
test: $(TESTS:=.test)


# Rules to build applications
#

##  new api
##
sor: sor.o
	$(CXX) -o $@ $< -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sor.h: sor.cnc
	$(DEST_DIR)/bin/$(CNC_ARCH_PLATFORM)/cnc $<

sor.o: sor.cpp sor.h
	$(CXX) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

# Utility rules
#
$(TESTS:=.tested) $(TESTS:=.failed):
	touch $@
 
clean:
	rm -f $(TESTS:=.tested) $(TESTS:=.failed) $(TESTS:=.out) $(APPS) sor.o sor.h sor_codinghints.txt 

# Rules to run tests
#
sor_w03_100x100_x50.test: sor sor_w03_100x100_x50.tested sor_w03_100x100_x50.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./sor 0.3 100 100 50 -v > $(@:.test=.out)
	diff sor_w03_100x100_x50.compare sor_w03_100x100_x50.out -I ^Computed.
	rm -f $(@:.test=.failed)
