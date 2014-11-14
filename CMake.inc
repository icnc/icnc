# Commom defs for CMake

################################################################################
# Path options: TBB, MPI, ITAC

set(TBBROOT $ENV{TBBROOT}     CACHE PATH "path to TBB")
set(MPIROOT $ENV{I_MPI_ROOT}  CACHE PATH "path to MPI")
set(ITACROOT $ENV{VT_ROOT}    CACHE PATH "path to ITAC")


################################################################################
# Misc vars and platform specifics

set(CNC_DBG_SFX "_debug")
#$(PlatformToolset)       v110     v110 v120
#$(VisualStudioVersion)   10.0 11.0 12.0
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  if(${MSVC12})
    set(CNC_DLL_SFX "vs12.0")
    set(TBB_DLL_PFX "vc12")
  elseif(${MSVC11})
    set(CNC_DLL_SFX "vs11.0")
    set(TBB_DLL_PFX "vc11")
  elseif(${MSVC10})
    set(CNC_DLL_SFX "vs10.0")
    set(TBB_DLL_PFX "vc10")
  endif()
else()
  set(CNC_DLL_SFX "")
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)


################################################################################
# generic compiler options

include_directories("${TBBROOT}/include")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DCNC_USE_ASSERT=1 -DTBB_USE_ASSERT=1 -DTBB_USE_THREADING_TOOLS -DTBB_DEPRECATED_MUTEX_COPYING")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DTBB_DEPRECATED_MUTEX_COPYING")

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /TP /Od /wd4996 /wd4275 /wd4251 /EHsc /MDd /Gy-")
  add_definitions("-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS -D__TBB_RELAXED_OWNERSHIP=0")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  set(CNC_ADD_LIBS -lm -lrt -ldl)
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)

set(CMAKE_DEBUG_POSTFIX "_debug")


################################################################################
# TBB deps

set(CMAKE_PREFIX_PATH "${TBBROOT}/lib/intel64/gcc4.4" "${TBBROOT}/lib/intel64/cc4.1.0_libc2.4_kernel2.6.16.21" "${TBBROOT}/lib/intel64/${TBB_DLL_PFX}")
find_library(TBB_LIB tbb)
find_library(TBBMALLOC_LIB tbbmalloc)
set(CNC_ADD_LIBS ${CNC_ADD_LIBS} optimized ${TBB_LIB} optimized ${TBBMALLOC_LIB})
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_library(TBB_LIB_DBG tbb_debug)
  find_library(TBBMALLOC_LIB_DBG tbbmalloc_debug)
  set(CNC_ADD_LIBS ${CNC_ADD_LIBS} debug ${TBB_LIB_DBG} debug ${TBBMALLOC_LIB_DBG})
endif()

################################################################################
# MPI deps

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(MPIARCH em64t)
else()
  set(MPIARCH intel64)
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)
set(CMAKE_PREFIX_PATH "${MPIROOT}/${MPIARCH}/include" "${MPIROOT}/include" "${MPIROOT}/include/mpich2")
find_path(MPI_INC_DIR mpi.h)
find_library(MPI_LIB_OPT NO_DEFAULT_PATH HINTS ${MPI_INC_DIR}/../lib NAMES mpi_mt impimt mpich mpi)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
  find_library(MPI_LIB_DBG NO_DEFAULT_PATH HINTS ${MPI_INC_DIR}/../lib NAMES mpi_dbg_mt impidmt mpich mpi)
  set(MPI_LIB optimized ${MPI_LIB_OPT} debug ${MPI_LIB_DBG} CACHE INTERNAL "use for mpi-dependent links")
else()
  set(MPI_LIB optimized ${MPI_LIB_OPT} CACHE INTERNAL "use for mpi-dependent links")
endif()


################################################################################
# ITAC stuff

function(find_itac)
  if("$ENV{VT_ADD_LIBS}" STREQUAL "")
    message("Warning: no $VT_ADD_LIBS env var set.")
    set(VT_ADD_LIBS_ENV "-lm")
  else()
    set(VT_ADD_LIBS_ENV $ENV{VT_ADD_LIBS})
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    string(REPLACE ".lib" "" VT_ADD_LIBS__ ${VT_ADD_LIBS_ENV})
    set(CMAKE_PREFIX_PATH ${ITACROOT}/lib/impi64)
  else()
    string(REPLACE "-l" "" VT_ADD_LIBS__ ${VT_ADD_LIBS_ENV})
    set(CMAKE_PREFIX_PATH "${ITACROOT}/intel64/slib")
  endif(CMAKE_SYSTEM_NAME STREQUAL Windows)
  string(REPLACE " " ";" VT_ADD_LIBS_ ${VT_ADD_LIBS__})
  find_library(ITACMPI_LIB VT)
  find_library(ITACCS_LIB VTcs)
  foreach(lib ${VT_ADD_LIBS_})
    find_library(alib ${lib})
    set(VT_ADD_LIBS ${VT_ADD_LIBS} general ${alib})
    unset(alib CACHE)
  endforeach()
  set(ITAC_LIB ${ITACCS_LIB} CACHE STRING "")
  set(ITAC_LIBS general ${ITAC_LIB} ${VT_ADD_LIBS} CACHE INTERNAL "")
endfunction()

set(CNC_ITAC_FLAGS "-DCNC_WITH_ITAC")
find_itac()

function(set_itac_link_flags tgt)
  target_link_libraries(${tgt} ${ITAC_LIBS})
  if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(${tgt} PROPERTIES LINK_FLAGS "/LIBPATH:\"${ITACROOT}/../../../Composer XE/compiler/lib/intel64\"")
  endif(CMAKE_SYSTEM_NAME STREQUAL Windows)
endfunction()
