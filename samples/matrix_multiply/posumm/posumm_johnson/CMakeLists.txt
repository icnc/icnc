# see parent $CNCROOT/samples/CMakeLists.txt

set(myname mm_johnson)

add_executable(${myname} ${myname}_cnc.cpp kernel.cpp ../common/utils.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
