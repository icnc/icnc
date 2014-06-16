# see parent $CNCROOT/samples/CMakeLists.txt

set(myname uts-1.1)

add_executable(${myname} uts_cnc.cpp dequeue.c dlist.c uts.c rng/brg_sha1.c)
set_target_properties(${myname} PROPERTIES COMPILE_FLAGS "-DBRG_RNG")
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
