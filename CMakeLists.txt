# cmake lists for building CnC

cmake_minimum_required (VERSION 2.8.7)

project(CnC)

# include common defs
include( "CMake.inc" )

################################################################################
# options

option(BUILD_LIBS_FOR_MPI  "Build additional libraries for using MPI-communication on distributed memory" FALSE)
option(BUILD_LIBS_FOR_ITAC "Build additional libraries for performance analysis with ITAC"  FALSE)
option(CNC_PRODUCT_BUILD   "Build an \"official\" binary release" FALSE)


################################################################################
# Project paths

set(CNC_SRC_ROOT ${CMAKE_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH "bin")
set(LIBRARY_OUTPUT_PATH "lib/intel64")

################################################################################
# source files for different libs

set( CNC_BASE_SOURCES "${CNC_SRC_ROOT}/src/dist/Buffer.cpp"
                      "${CNC_SRC_ROOT}/src/chronometer.cpp"
		      "${CNC_SRC_ROOT}/src/context_base.cpp"
		      "${CNC_SRC_ROOT}/src/debug.cpp"
		      "${CNC_SRC_ROOT}/src/dist/dist_init.cpp"
		      "${CNC_SRC_ROOT}/src/dist/distributable_context.cpp"
		      "${CNC_SRC_ROOT}/src/dist/distributor.cpp"
		      "${CNC_SRC_ROOT}/src/dist/factory.cpp"
		      "${CNC_SRC_ROOT}/src/item_collection_i.cpp"
		      "${CNC_SRC_ROOT}/src/scheduler_i.cpp"
		      "${CNC_SRC_ROOT}/src/simplest_scheduler.cpp"
		      "${CNC_SRC_ROOT}/src/statistics.cpp"
		      "${CNC_SRC_ROOT}/src/step_launcher_i.cpp"
		      "${CNC_SRC_ROOT}/src/step_instance_base.cpp"
		      "${CNC_SRC_ROOT}/src/tag_collection_i.cpp"
		      "${CNC_SRC_ROOT}/src/tbb_concurrent_queue_scheduler.cpp"
		      "${CNC_SRC_ROOT}/src/traceable.cpp"
		      "${CNC_SRC_ROOT}/src/cnc_stddef.cpp"
		      # RRN: It's actually the same for Mac as for linux:
		      "${CNC_SRC_ROOT}/src/tls_${CMAKE_SYSTEM_NAME}.cpp"
		      )

set( CNC_SOCKET_SOURCES "${CNC_SRC_ROOT}/src/dist/generic_comm/ChannelInterface.cpp"
                        "${CNC_SRC_ROOT}/src/dist/generic_comm/GenericCommunicator.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/pal_socket.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/pal_util.cpp"
			"${CNC_SRC_ROOT}/src/dist/generic_comm/Settings.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/SocketChannelInterface.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/SocketClientInitializer.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/SocketCommunicator.cpp"
			"${CNC_SRC_ROOT}/src/dist/socket_comm/SocketHostInitializer.cpp"
			"${CNC_SRC_ROOT}/src/dist/generic_comm/ThreadExecuter.cpp"
			)

set( CNC_MPI_SOURCES "${CNC_SRC_ROOT}/src/dist/generic_comm/ChannelInterface.cpp"
                     "${CNC_SRC_ROOT}/src/dist/generic_comm/GenericCommunicator.cpp"
		     "${CNC_SRC_ROOT}/src/dist/generic_comm/Settings.cpp"
		     "${CNC_SRC_ROOT}/src/dist/mpi_comm/MpiChannelInterface.cpp"
		     "${CNC_SRC_ROOT}/src/dist/mpi_comm/MpiClientInitializer.cpp"
		     "${CNC_SRC_ROOT}/src/dist/mpi_comm/MpiCommunicator.cpp"
		     "${CNC_SRC_ROOT}/src/dist/mpi_comm/MpiHostInitializer.cpp"
		     "${CNC_SRC_ROOT}/src/dist/generic_comm/ThreadExecuter.cpp"
		     "${CNC_SRC_ROOT}/src/dist/socket_comm/pal_util.cpp"
		     )


################################################################################
# Misc vars

set(CNC_LIBS cnc cnc_socket)


################################################################################
# generic compiler options

include_directories("${CNC_SRC_ROOT}" "${CNC_SRC_ROOT}/src/dist/generic_comm")

if(CNC_PRODUCT_BUILD)
  add_definitions("-DCNC_PRODUCT_BUILD")
endif(CNC_PRODUCT_BUILD)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  add_definitions("-D_LIB -D_WIN_DLL -DCNC_DLL_SUFX=_${CNC_DLL_SFX}")
  set(CMAKE_SHARED_LINKER_FLAGS "/INCREMENTAL:NO /OPT:NOREF")
else()
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)

set(CMAKE_DEBUG_POSTFIX "_debug")


################################################################################
# base library

add_library(cnc SHARED ${CNC_BASE_SOURCES} )
add_library(cnc_socket SHARED ${CNC_SOCKET_SOURCES} )

# only the base library exports symbols!!
set_target_properties(cnc PROPERTIES COMPILE_FLAGS "-DCNC_DLL_EXPORTS")
target_link_libraries(cnc ${CNC_ADD_LIBS})
target_link_libraries(cnc_socket general cnc ${CNC_ADD_LIBS})


################################################################################
# MPI stuff

if(BUILD_LIBS_FOR_MPI)
  set(CNC_LIBS ${CNC_LIBS} cnc_mpi)
  add_library(cnc_mpi SHARED ${CNC_MPI_SOURCES})
  set(CNC_MPI_FLAGS "-DNO_MSG_TRACING -DPRE_SEND_MSGS")
  set_target_properties(cnc_mpi PROPERTIES COMPILE_FLAGS "${CNC_MPI_FLAGS}")
  # doesn't seem to work:  set_target_properties(cnc_mpi PROPERTIES INCLUDE_DIRECTORIES "${MPIROOT}/${MPIARCH}/include")
  include_directories(${MPI_INC_DIR})
  target_link_libraries(cnc_mpi cnc ${CNC_ADD_LIBS} ${MPI_LIB})
endif(BUILD_LIBS_FOR_MPI)


################################################################################
# ITAC stuff

if(BUILD_LIBS_FOR_ITAC)
  set(CNC_LIBS ${CNC_LIBS} cnc_socket_itac)
  add_library(cnc_socket_itac SHARED ${CNC_SOCKET_SOURCES} )
  set(CNC_ITAC_FLAGS "${CNC_ITAC_FLAGS} -DINTERNAL_ITAC")
  set_target_properties(cnc_socket_itac PROPERTIES COMPILE_FLAGS "${CNC_ITAC_FLAGS}")
  set_target_properties(cnc_socket_itac PROPERTIES INCLUDE_DIRECTORIES "${ITACROOT}/include")
  set_itac_link_flags(cnc_socket_itac)
  target_link_libraries(cnc_socket_itac cnc ${CNC_ADD_LIBS} ${ITACCS_LIB})
  if(BUILD_LIBS_FOR_MPI)
    set(CNC_LIBS ${CNC_LIBS} cnc_mpi_itac)
    add_library(cnc_mpi_itac SHARED ${CNC_MPI_SOURCES} )
    set_target_properties(cnc_mpi_itac PROPERTIES COMPILE_FLAGS "${CNC_MPI_FLAGS} ${CNC_ITAC_FLAGS}")
    # doesn't seem to work:  set_target_properties(cnc_mpi_itac PROPERTIES INCLUDE_DIRECTORIES "${MPIROOT}/${MPIARCH}/include" INCLUDE_DIRECTORIES "${ITACROOT}/include")
    include_directories("${MPIROOT}/${MPIARCH}/include" "${ITACROOT}/include")
    set_itac_link_flags(cnc_mpi_itac)
    target_link_libraries(cnc_mpi_itac cnc ${CNC_ADD_LIBS} ${MPI_LIB} ${ITAC_LIB})
  endif(BUILD_LIBS_FOR_MPI)
endif(BUILD_LIBS_FOR_ITAC)

#            {'option': 'cflags=-DWITHOUT_SENDER_THREAD'}


################################################################################
# add a target to generate API documentation with Doxygen

find_package(Doxygen)
if(DOXYGEN_FOUND)
configure_file(${CNC_SRC_ROOT}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
add_custom_target(doc
	${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
)
endif(DOXYGEN_FOUND)


################################################################################
# Install

install(TARGETS ${CNC_LIBS} RUNTIME DESTINATION bin/ LIBRARY DESTINATION lib)
install(DIRECTORY ${CNC_SRC_ROOT}/cnc DESTINATION include FILES_MATCHING PATTERN "*.h")
