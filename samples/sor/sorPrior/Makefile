TOP = ..
include $(TOP)/Makefile.common

APPS = sorPrior_NewApi
TESTS = sorPrior_NewApi_w03_100x100_10x10_x50

build: $(APPS)
test: $(TESTS:=.test)


##  new api
##
sorPrior_NewApi: sorPrior_NewApi.o TiledInputArray.o Tile.o
	$(CXX) -o $@ $? -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sorPrior_NewApi.o: sorPrior_NewApi.cpp
	$(CXX) $(TBBDEBUGFLAG) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

Tile.o: Tile.cpp
	$(CXX) -c -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

TiledInputArray.o: TiledInputArray.cpp
	$(CXX) -c -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

# Utility rules
#
$(TESTS:=.tested) $(TESTS:=.failed):
	touch $@
 
clean:
	rm -f $(TESTS:=.tested) $(TESTS:=.failed) $(TESTS:=.out) $(APPS) sorPrior.o sorPrior_codinghints.txt sorPrior_NewApi.o Tile.o TiledInputArray.o

# Rules to run tests
#
sorPrior_NewApi_w03_100x100_10x10_x50.test: sorPrior_NewApi sorPrior_NewApi_w03_100x100_10x10_x50.tested sorPrior_NewApi_w03_100x100_10x10_x50.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./sorPrior_NewApi 0.3 100 100 50 10 10 -v > $(@:.test=.out)
	diff sorPrior_NewApi_w03_100x100_10x10_x50.compare sorPrior_NewApi_w03_100x100_10x10_x50.out -I ^Computed.
	rm -f $(@:.test=.failed)
