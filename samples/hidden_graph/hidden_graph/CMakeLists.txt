# see parent $CNCROOT/samples/CMakeLists.txt

set(myname hidden_graph)

add_executable(${myname} ${myname}.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
