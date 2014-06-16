# see parent $CNCROOT/samples/CMakeLists.txt

set(myname floyd_3d_tiled)

add_executable(${myname} floyd_cnc.cpp ../common/floyd_base.cpp ../common/utils.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
