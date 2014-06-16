# see parent $CNCROOT/samples/CMakeLists.txt

set(myname cholesky)

add_executable(${myname} ${myname}.cpp cholesky_cnc.cpp init.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
