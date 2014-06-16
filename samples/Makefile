MAKEFLS = $(wildcard */Makefile)
DIRS = $(subst /,,$(dir $(MAKEFLS)))
DMYS = $(addsuffix /.dmy,$(DIRS))
DMYS_CLEAN = $(DMYS:.dmy=.clean)

export OPTS
export CXX

all: $(DMYS)

clean: $(DMYS_CLEAN)
	rm -f *~

$(DIRS): %: %/.dmy

$(DMYS):
	cd $(@D); $(MAKE) all

$(DMYS_CLEAN):
	cd $(@D); $(MAKE) clean
