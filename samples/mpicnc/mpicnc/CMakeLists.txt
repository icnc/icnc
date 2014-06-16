# see parent $CNCROOT/samples/CMakeLists.txt

set(myname mpicnc)

add_executable(${myname} ${myname}.cpp)

set_target_properties(${myname} PROPERTIES COMPILE_FLAGS "-D_DIST_ -I\"${MPIROOT}/${MPIARCH}/include\"")
target_link_libraries(${myname} ${NEEDED_LIBS} ${MPI_LIB})

if(USE_ITAC)
  set_itac_link_flags(${myname})
endif(USE_ITAC)

unset(myname CACHE)
