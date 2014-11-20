# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(matrix_inverse ../../samples/matrix_inverse/matrix_inverse/matrix_inverse.cpp "1980" matrix_inverse.ref "")
add_cnc_test(matrix_inverse_unsafeget ../../samples/matrix_inverse/matrix_inverse_unsafeget/matrix_inverse_unsafeget.cpp "1980" matrix_inverse.ref)

if(${RUN_DIST})
  add_executable(matrix_inverse_distenv ../../samples/matrix_inverse/matrix_inverse_distenv/matrix_inverse.cpp)
  set_target_properties(matrix_inverse_distenv PROPERTIES COMPILE_FLAGS "-I\"${MPIROOT}/${MPIARCH}/include\" -I$MPI_INC_DIR -D_DIST_")
  target_link_libraries(matrix_inverse_distenv ${MPI_LIB} ${NEEDED_LIBS} )
  add_compare_test(matrix_inverse_distenv matrix_inverse_distenv "1980" matrix_inverse_distenv.out matrix_inverse_distenv.out matrix_inverse.ref "txt" "MPI")
endif()
