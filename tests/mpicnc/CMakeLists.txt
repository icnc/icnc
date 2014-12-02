# see parent $CNCROOT/tests/CMakeLists.txt

if(${RUN_DIST})
    add_executable(mpicnc ../../samples/mpicnc/mpicnc/mpicnc.cpp)
    set_target_properties(mpicnc PROPERTIES COMPILE_FLAGS "-I\"${MPIROOT}/${MPIARCH}/include\" -I$MPI_INC_DIR -D_DIST_")
    target_link_libraries(mpicnc ${MPI_LIB} ${NEEDED_LIBS} )
    add_compare_test(mpicnc mpicnc " " ${CMAKE_CURRENT_BINARY_DIR}/mpicnc.out ${CMAKE_CURRENT_BINARY_DIR}/mpicnc.out mpicnc.ref txt "MPI")
endif()
