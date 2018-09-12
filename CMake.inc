# Commom defs for CMake

################################################################################
# Path options: TBB, MPI, ITAC

if(NOT TBBROOT)
    set(TBBROOT $ENV{TBBROOT} CACHE PATH "path to TBB")
endif()
if(NOT MPIROOT)
    set(MPIROOT $ENV{I_MPI_ROOT} CACHE PATH "path to MPI")
endif()
if(NOT ITACROOT)
    set(ITACROOT $ENV{VT_ROOT} CACHE PATH "path to ITAC")
endif()
if(NOT PARCH)
    set(PARCH "intel64")
endif()

################################################################################
# Misc vars and platform specifics

set(CNC_DBG_SFX "_debug")
#$(VisualStudioVersion)   10.0 11.0 12.0 13.0 14.0
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  if(MSVC_VERSION GREATER_EQUAL 2000)
    set(CNC_DLL_SFX "_vsxx.0")
    set(TBB_DLL_PFX "vcxx")
  elseif(MSVC_VERSION GREATER_EQUAL 1900)
    set(CNC_DLL_SFX "_vs14.0")
    set(TBB_DLL_PFX "vc14")
  elseif(MSVC_VERSION GREATER_EQUAL 1800)
    set(CNC_DLL_SFX "_vs12.0")
    set(TBB_DLL_PFX "vc12")
  elseif(MSVC_VERSION GREATER_EQUAL 1700)
    set(CNC_DLL_SFX "_vs11.0")
    set(TBB_DLL_PFX "vc11")
  elseif(MSVC_VERSION GREATER_EQUAL 1600)
    set(CNC_DLL_SFX "_vs10.0")
    set(TBB_DLL_PFX "vc10")
  else()
    set(CNC_DLL_SFX "_vsxx")
    set(TBB_DLL_PFX "vcxx")
  endif(MSVC_VERSION GREATER_EQUAL 1600)
else()
  set(CNC_DLL_SFX "")
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)

if(${PARCH} STREQUAL mic)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmic")
endif()

################################################################################
# generic compiler options

include_directories("${TBBROOT}/include")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DCNC_USE_ASSERT=1 -DTBB_USE_ASSERT=1 -DTBB_USE_THREADING_TOOLS")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

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

set(CMAKE_PREFIX_PATH "${TBBROOT}/lib" "${TBBROOT}/lib/${PARCH}/gcc4.4" "${TBBROOT}/lib/${PARCH}/cc4.1.0_libc2.4_kernel2.6.16.21" "${TBBROOT}/lib/${PARCH}/${TBB_DLL_PFX}")
find_library(TBB_LIB tbb)
find_library(TBBMALLOC_LIB tbbmalloc)
set(CNC_ADD_LIBS ${CNC_ADD_LIBS} optimized ${TBB_LIB} optimized ${TBBMALLOC_LIB})
if (CMAKE_BUILD_TYPE STREQUAL "Debug" OR ";${CMAKE_CONFIGURATION_TYPES};" MATCHES ";Debug;")
  find_library(TBB_LIB_DBG tbb_debug)
  find_library(TBBMALLOC_LIB_DBG tbbmalloc_debug)
  set(CNC_ADD_LIBS ${CNC_ADD_LIBS} debug ${TBB_LIB_DBG} debug ${TBBMALLOC_LIB_DBG})
endif()

################################################################################
# MPI deps

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  set(MPIPARCH em64t)
else()
  set(MPIPARCH ${PARCH})
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)
set(CMAKE_PREFIX_PATH "${MPIROOT}/${MPIPARCH}/include" "${MPIROOT}/include" "${MPIROOT}/include/mpich2" "${MPIROOT}/include/mpich")
find_path(MPI_INC_DIR mpi.h)
find_library(MPI_LIB_OPT NO_DEFAULT_PATH HINTS ${MPI_INC_DIR}/../lib ${MPIROOT}/lib NAMES mpi_mt impimt mpich mpi)
if(CMAKE_BUILD_TYPE STREQUAL Debug)
  find_library(MPI_LIB_DBG NO_DEFAULT_PATH HINTS ${MPI_INC_DIR}/../lib ${MPIROOT}/lib NAMES mpi_dbg_mt impidmt mpich mpi)
  set(MPI_LIB optimized ${MPI_LIB_OPT} debug ${MPI_LIB_DBG} CACHE INTERNAL "use for mpi-dependent links")
else()
  set(MPI_LIB optimized ${MPI_LIB_OPT} CACHE INTERNAL "use for mpi-dependent links")
endif()
# extra libs required by openmpi
foreach(mpilib mpi_cxx mca_common_sm open-rte)
  find_library(MPI_LIB_${mpilib} NO_DEFAULT_PATH HINTS ${MPI_INC_DIR}/../lib ${MPIROOT}/lib NAMES ${mpilib})
  if(MPI_LIB_${mpilib})
    set(MPI_LIB ${MPI_LIB} optimized ${MPI_LIB_${mpilib}} debug ${MPI_LIB_${mpilib}})
  endif(MPI_LIB_${mpilib})
  unset(MPI_LIB_${mpilib} CACHE)
endforeach(mpilib)

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
    set(CMAKE_PREFIX_PATH "${ITACROOT}/${PARCH}/slib")
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
    set_target_properties(${tgt} PROPERTIES LINK_FLAGS "/LIBPATH:\"${ITACROOT}/../../../Composer XE/compiler/lib/${PARCH}\"")
  endif(CMAKE_SYSTEM_NAME STREQUAL Windows)
endfunction()
