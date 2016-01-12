# FIXME blas/mkl dependences
# see parent $CNCROOT/samples/CMakeLists.txt

set(myname auto_graph_pipeline)

add_executable(${myname} src/user_program/hello_world.c src/auto_composed_graph/auto_composed_graph.cpp)
target_link_libraries(${myname} ${NEEDED_LIBS})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
