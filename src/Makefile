ifeq (,$(CNCROOT))
CNCROOT3=$(shell dirname $(CURDIR))
CNCROOT2=$(shell dirname $(CNCROOT3))
CNCROOT=$(shell dirname $(CNCROOT2))
endif

include $(CNCROOT)/makefile.defs
CXXFLAGS = $(OPT) $(OPT_CXX)
#CXXFLAGS = -g -O0 -fno-inline


SOURCES :=  concurrent_priority_queue.cpp \
	    context_base.cpp \
	    debug.cpp \
            item_collection_i.cpp \
            scheduler_i.cpp \
            statistics.cpp \
            step_instance.cpp \
            stealing_scheduler.cpp \
            tbb_concurrent_queue_scheduler.cpp \
            tbb_concurrent_queue_prioritized_scheduler.cpp \
            tbb_parallel_while_scheduler.cpp \
            tbb_parallel_while_queued_scheduler.cpp \
            tbb_for_priority_queue_range_scheduler.cpp \
            simplest_scheduler.cpp \
            simplest_prioritized_scheduler.cpp \
            tag_collection_i.cpp \
            chronometer.cpp \
            tls_linux.cpp \
            cnc_stddef.cpp


HEADERS_PUBLIC := cnc.h debug.h default_tuner.h

HEADERS_INTERNAL := concurrent_priority_queue.h \
	   context_base.h \
	   context.h \
           format.h \
	   item_collection_base.h \
	   item_collection.h \
	   item_collection_i.h \
	   schedulable.h \
	   scheduler_i.h \
	   step_collection_i.h \
	   step_collection.h \
	   step_instance.h \
	   tag_collection_base.h \
	   tag_collection.h \
	   tag_collection_i.h \
	   tag_base.h \
	   typed_tag.h \
           traceable.h\
           step_delayer.h\
           cnc_stddef.h

HEADERS_PRIVATE := tbb_concurrent_queue_scheduler.h\
           tbb_concurrent_queue_prioritized_scheduler.h\
           stealing_scheduler.h \
           tbb_parallel_while_scheduler.h \
           tbb_parallel_while_queued_scheduler.h \
           tbb_for_priority_queue_range_scheduler.h \
           simplest_scheduler.h \
	   simplest_prioritized_scheduler.h\
           statistics.h \
           suspend_group.h \
           chronometer.h \
           tls.h


SRC_HEADERS_PUBLIC:=$(addprefix ../cnc/,$(HEADERS_PUBLIC)) 
SRC_HEADERS_INTERNAL:=$(addprefix ../cnc/internal/,$(HEADERS_INTERNAL))

DEST_OBJS=$(addprefix $(OBJ_DIR)/new_api/libcnc_static/,$(SOURCES:.cpp=$(OBJ)))

LIBDIR=$(DEST_DIR)/new_api/lib

#	Next line is needed to define objects to copy into libcnc
libcnc_SOBJECTS=$(DEST_OBJS)

all:  $(LIBDIR)/libcnc$(SLIBEXT) copy_headers

$(LIBDIR)/libcnc$(SLIBEXT): $(DEST_OBJS)

clean:
	rm -f $(DEST_OBJS)

#	Some explicit dependencies because dependencies are generated

$(OBJ_DIR)/new_api/libcnc_static/%.o:  %.cpp 
	mkdir -p $(OBJ_DIR)/new_api/libcnc_static
	$(CXX) $(CXXFLAGS) -c -I$(TBB)/include -I.. $(FO)$@ $(PWD)/$<

copy_headers: 
	mkdir -p $(DEST_DIR)/new_api/include/cnc
	cp $(SRC_HEADERS_PUBLIC) $(DEST_DIR)/new_api/include/cnc
	mkdir -p $(DEST_DIR)/new_api/include/cnc/internal
	cp $(SRC_HEADERS_INTERNAL) $(DEST_DIR)/new_api/include/cnc/internal 

include $(CNCROOT)/makefile.basic_rules

tscons:
	python2.5.2 $(CNCROOT)/tscons/tscons 
