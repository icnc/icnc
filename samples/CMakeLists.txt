# cmake lists for building CnC samples

SET(MSVC_INCREMENTAL_DEFAULT OFF)
cmake_minimum_required (VERSION 2.6)

project(CnC-samples)


################################################################################
# options

set(CNCROOT $ENV{CNCROOT}     CACHE PATH "path to CnC")

option(DIST_CNC "Compile for distributed memory" FALSE)
option(USE_ITAC "Compile/link with ITAC" FALSE)


################################################################################
# include "external" stuff

# include common defs
include( "../CMake.inc" )


################################################################################
# dependences

include_directories("${CNCROOT}/include")
set(CMAKE_PREFIX_PATH "${CNCROOT}/lib/intel64")
find_library(CNC_LIB NAMES "cnc" "cnc_${CNC_DLL_SFX}")
find_library(CNC_LIB_DBG NAMES "cnc${CNC_DBG_SFX}" "cnc${CNC_DBG_SFX}_${CNC_DLL_SFX}")

set(NEEDED_LIBS optimized ${CNC_LIB} debug ${CNC_LIB_DBG} ${CNC_ADD_LIBS})

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
# now all the samples

function(add_sample sample)
  add_subdirectory(${sample})
  set(ALL_SAMPLES "${ALL_SAMPLES} '${sample}'," PARENT_SCOPE)
#  set(dir_ "UNSET_")
#  if(${ARGC} GREATER 1)
#    foreach(cpp ${ARGN})
#      if(dir_ STREQUAL "UNSET_")
#	set(dir_ ${cpp})
#      else()
#	set(cpp_s_ ${cpp_s_} ${dir_}/${cpp})
#      endif(dir_ STREQUAL "UNSET_")
#    endforeach()
#  else()
#    set(cpp_s_ ${sample}/${sample}/${sample}.cpp)
#  endif(${ARGC} GREATER 1)
#  add_executable(${sample} ${cpp_s_})
#  target_link_libraries(${sample} ${NEEDED_LIBS})
endfunction()

add_sample(fib/fib)
add_sample(fib/fib_getcount)
add_sample(fib/fib_memoize)
add_sample(fib/fib_preschedule)
add_sample(fib/fib_range)
add_sample(fib/fib_stats)
add_sample(fib/fib_trace)
add_sample(fib/fib_tuner)
add_sample(fib/fib_tutorial)
add_sample(primes/primes)
add_sample(primes/primes_range)
add_sample(primes/primes_parfor)
add_sample(primes/primes_serial)
add_sample(mandelbrot/mandelbrot)
add_sample(mandelbrot/mandelbrot_vector)
add_sample(mandelbrot/mandelbrot_serial)
add_sample(blackscholes/blackscholes)
add_sample(matrix_inverse/matrix_inverse)
add_sample(matrix_inverse/matrix_inverse_unsafeget)
add_sample(nqueens/nqueens)
add_sample(reduce_2d/reduce_2d)
add_sample(count_words/count_words)
add_sample(count_all_words/count_all_words)
add_sample(count_all_words/mapreduce)
add_sample(cholesky/cholesky)
add_sample(rtm_stencil/rtm_stencil_halo)
add_sample(rtm_stencil/rtm_stencil_tiled)
add_sample(FaceDetection/FaceDetection)
add_sample(floyd_warshall/floyd_1d_tiled)
add_sample(floyd_warshall/floyd_2d_tiled)
add_sample(floyd_warshall/floyd_3d_tiled)
add_sample(dedup/dedup)
add_sample(uts/uts-1.1)
add_sample(heat_equation/heat_equation)
add_sample(hidden_graph/hidden_graph)
add_sample(db/mysql_simple)
add_sample(raytracer/raytracer)
add_sample(jacobi2d/jacobi2d_heureka)
# example codes from eight patterns document
add_sample(reduce_rows/reduce_rows)
add_sample(producer_consumer/producer_consumer)
add_sample(partition_string/partition_string)
add_sample(divide_conquer/divide_conquer)
add_sample(red_rows_until_pred/red_rows_until_pred)
add_sample(search_in_sorted_tree/search_in_sorted_tree)
add_sample(gen_new_iter_space/gen_new_iter_space)
# distCnC only examples
if(DIST_CNC)
  add_sample(matrix_inverse/matrix_inverse_distenv)
  add_sample(mpicnc/mpicnc)
endif()

#add_sample(binomial_lattice/binLat_K)
configure_file(make_vcxproj.py.in ../make_vcxproj.py @ONLY)
