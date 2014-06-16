# see parent $CNCROOT/samples/CMakeLists.txt

set(myname dedup)

add_executable(${myname} binheap.cpp encoder_cnc.cpp hashtable.cpp rabin.cpp util.cpp dedup.cpp encoder_util.cpp queue.cpp tree.cpp getopt/getopt.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS} z m ssl rt crypto)

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
