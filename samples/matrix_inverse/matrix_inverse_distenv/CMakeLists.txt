# see parent $CNCROOT/samples/CMakeLists.txt

set(myname matrix_inverse_distenv)

add_executable(${myname} matrix_inverse.cpp)

set_target_properties(${myname} PROPERTIES COMPILE_FLAGS -I\"${MPIROOT}/${MPIARCH}/include\")
target_link_libraries(${myname} ${NEEDED_LIBS} ${MPI_LIB})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
