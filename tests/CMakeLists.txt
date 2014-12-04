# cmake lists for building CnC samples
cmake_minimum_required(VERSION 2.8)

project(CnC-tests)
enable_testing()

################################################################################
# options

option(RUN_DIST "run tests on distributed memory" TRUE)
set(CNCROOT $ENV{CNCROOT}     CACHE PATH "path to CnC")

## TODO option(USE_ITAC "Compile/link with ITAC" FALSE)


################################################################################
# include "external" stuff

# include common defs
include( "../CMake.inc" )


################################################################################
# dependences

include_directories("${CNCROOT}/include")
set(CMAKE_PREFIX_PATH "${CNCROOT}/lib/intel64")
find_library(CNC_LIB NAMES "cnc" "cnc${CNC_DLL_SFX}")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_library(CNC_LIB_DBG NAMES "cnc${CNC_DBG_SFX}" "cnc${CNC_DBG_SFX}${CNC_DLL_SFX}")
else()
  set(CNC_LIB_DBG ${CNC_LIB})
endif()
set(NEEDED_LIBS optimized ${CNC_LIB} debug ${CNC_LIB_DBG})
set(NEEDED_LIBS ${NEEDED_LIBS} ${CNC_ADD_LIBS}) 

if(DIST_CNC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DIST_")
endif(DIST_CNC)

if(USE_ITAC)
  include_directories("${ITACROOT}/include")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CNC_ITAC_FLAGS}")
  add_definitions(${CNC_ITAC_FLAGS})
  set(NEEDED_LIBS ${NEEDED_LIBS} general ${ITACCS_LIB})
endif(USE_ITAC)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)


################################################################################
# now some functions and settings to make adding tests easy

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CNC_TESTING_")

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
SET (test_env "PATH=${CNCROOT}/bin/intel64\;${TBBROOT}/bin/intel64/${TBB_DLL_PFX}")#\;$ENV{PATH}")
else()
SET (test_env "LD_LIBRARY_PATH=${CNCROOT}/bin/intel64:${TBBROOT}/bin/intel64/${TBB_DLL_PFX}:$ENV{LD_LIBRARY_PATH}")
endif()

# add a cnc test: run the test and compare output against reference
# use this only if you can't use add_cnc_test
# uses run_compare.cmake
# treats reference file as relative to ${CMAKE_CURRENT_LIST_DIR}
# reference file can be no.ref if checks are done in the test itself
function( add_compare_test
    tname    # name of the test
    exe      # executable to run
    args     # string with space-separated arguments
    outfile  # file to pipe output into
    cmpfile  # output file to compare against reference
    reffile  # reference file, compared with compare.py
    cmpmode  # compare mode, if txt compares in python-regex mode, otherwise binary mode
    distmode # mode of distribution: shared, MPI, SOCKETS or SHMEM
    )
  add_test( NAME ${tname}
    COMMAND ${CMAKE_COMMAND}
    -Dtest_cmd=${exe}
    -Dtest_env="${test_env}"
    -Dtest_args:string=${args}
    -Doutput_blessed=${CMAKE_CURRENT_LIST_DIR}/${reffile}
    -Doutput_test=${outfile}
    -Dcompare_file=${cmpfile}
    -Dcompare_mode=${cmpmode}
    -Ddist_mode=${distmode}
    -P ${CMAKE_SOURCE_DIR}/run_compare.cmake
    ) 
endfunction()

# short-cut for adding a "standard" CnC test on shared and optionally on distributed memory
# assumes test provides text output which will be compared to reference file
# treats reference file as relative to ${CMAKE_CURRENT_LIST_DIR}
# reference file can be no.ref if checks are done in the test itself
function( add_cnc_test
    tname # name of test
    src   # string with space-separated list of source files
    args  #  string with space-separated arguments
    ref   # reference file, compared with compare.py
    )
  # convert the space-separated string to a list
  separate_arguments(src)
  add_executable(${tname} ${src})
  target_link_libraries(${tname} ${NEEDED_LIBS})
  add_compare_test(${tname} ${tname} ${args} ${CMAKE_CURRENT_BINARY_DIR}/${tname}.out ${CMAKE_CURRENT_BINARY_DIR}/${tname}.out ${ref} "txt" "shared")
  if(${RUN_DIST} AND ${ARGC} GREATER 4)
    list(LENGTH ARGN len)
    if(${len} GREATER 0)
      list(GET ARGN 0 dist)
    else()
      set(dist "")
    endif()
    add_executable(dist${tname} ${src})
    set_target_properties(dist${tname} PROPERTIES COMPILE_FLAGS "-D_DIST_")
    target_link_libraries(dist${tname} ${NEEDED_LIBS})
    add_compare_test(mpi_${tname} dist${tname} ${args} ${CMAKE_CURRENT_BINARY_DIR}/mpi_${tname}.out ${CMAKE_CURRENT_BINARY_DIR}/mpi_${tname}.out ${dist}${ref} "txt" "MPI")
    add_compare_test(sockets_${tname} dist${tname} ${args} ${CMAKE_CURRENT_BINARY_DIR}/sockets_${tname}.out ${CMAKE_CURRENT_BINARY_DIR}/sockets_${tname}.out ${dist}${ref} "txt" "SOCKETS")
#TODO    add_compare_test(shmem_${tname} dist${tname} ${args} shmem_${tname}.out shmem_${tname}.out ${ref} "txt" "SHMEM")
  endif()
endfunction()

################################################################################
# Now let's add the tests, each directory defines its own tests

add_subdirectory(simple)
add_subdirectory(fib)
add_subdirectory(primes)
add_subdirectory(pipeline)
add_subdirectory(quicksort)
add_subdirectory(mandelbrot)
add_subdirectory(mpicnc)
add_subdirectory(join)
add_subdirectory(reduce)
add_subdirectory(hidden_graph)
add_subdirectory(current_vals)
add_subdirectory(pass_on)
add_subdirectory(streaming)
add_subdirectory(uts)
add_subdirectory(heat_equation)
add_subdirectory(nqueens)
add_subdirectory(dedup)
add_subdirectory(cholesky)
add_subdirectory(rtm_stencil)
add_subdirectory(sor)
add_subdirectory(floyd_warshall)
add_subdirectory(matrix_inverse)
