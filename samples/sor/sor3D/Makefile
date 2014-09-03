TOP = ..
include $(TOP)/Makefile.common

APPS = sor3D_NewApi sor3D_NewApi_R
TESTS = sor3D_NewApi_w03_100x100_10x10_x50

build: $(APPS)
test: $(TESTS:=.test)


##  new api
##
sor3D_NewApi: sor3D_NewApi.o TiledInputArray.o Tile.o
	$(CXX) -o $@ $? -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sor3D_NewApi_R: sor3D_NewApi_R.o TiledInputArray.o Tile.o
	$(CXX) -o $@ $? -L$(TBBLIB) -L$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) -lcnc -ltbb -ltbbmalloc 

sor3D_NewApi.o: sor3D_NewApi.cpp 
	$(CXX) $(TBBDEBUGFLAG) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

sor3D_NewApi_R.o: sor3D_NewApi.cpp 
	$(CXX) $(TBBDEBUGFLAG) -c -I$(TBB)/include -I$(DEST_DIR)/include -I. $(OPT) $(OPT_CXX) $(FO)$@ $< -DREVERSE

Tile.o: Tile.cpp
	$(CXX) -c -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

TiledInputArray.o: TiledInputArray.cpp
	$(CXX) -c -I. $(OPT) $(OPT_CXX) $(FO)$@ $<

# Utility rules
#
$(TESTS:=.tested) $(TESTS:=.failed):
	touch $@
 
clean:
	rm -f $(TESTS:=.tested) $(TESTS:=.failed) $(TESTS:=.out) $(APPS) sor3D_NewApi.o sor3D_NewApi_R.o sor3D_NewApi_codinghints.txt Tile.o TiledInputArray.o

# Rules to run tests
#
sor3D_NewApi_w03_100x100_10x10_x50.test: sor3D_NewApi sor3D_NewApi_w03_100x100_10x10_x50.tested sor3D_NewApi_w03_100x100_10x10_x50.failed
	LD_LIBRARY_PATH=$(TBBLIB):$(DEST_DIR)/lib/$(CNC_ARCH_PLATFORM) ./sor3D_NewApi 0.3 100 100 50 10 10 -v > $(@:.test=.out)
	diff sor3D_NewApi_w03_100x100_10x10_x50.compare sor3D_NewApi_w03_100x100_10x10_x50.out -I ^Computed.
	rm -f $(@:.test=.failed)
