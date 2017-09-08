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
set(EXECUTABLE_OUTPUT_PATH "bin/${PARCH}")
set(LIBRARY_OUTPUT_PATH "lib/${PARCH}")

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

set(CNC_LIBS cnc_socket)


################################################################################
# generic compiler options

include_directories("${CNC_SRC_ROOT}" "${CNC_SRC_ROOT}/src/dist/generic_comm")

if(CNC_PRODUCT_BUILD)
  add_definitions("-DCNC_PRODUCT_BUILD")
endif(CNC_PRODUCT_BUILD)
if(SDL)
  if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -GS")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -NXCompat -DynamicBase")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector -fPIC -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now")
  endif(CMAKE_SYSTEM_NAME)
endif(SDL)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  add_definitions("-D_LIB -D_WIN_DLL -DCNC_DLL_SUFX=${CNC_DLL_SFX}")
  set(${CMAKE_SHARED_LINKER_FLAGS} "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO /OPT:NOREF")
else()
endif(CMAKE_SYSTEM_NAME STREQUAL Windows)

set(CMAKE_DEBUG_POSTFIX "_debug${CNC_DLL_SFX}")
set(CMAKE_RELEASE_POSTFIX "${CNC_DLL_SFX}")


################################################################################
# base library

#increase if ABI changes
set(SOVERSION 0)

add_library(cnc SHARED ${CNC_BASE_SOURCES} )
set_target_properties(cnc PROPERTIES SOVERSION ${SOVERSION})
add_library(cnc_socket SHARED ${CNC_SOCKET_SOURCES} )
set_target_properties(cnc_socket PROPERTIES SOVERSION ${SOVERSION})

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
  set_target_properties(cnc_mpi PROPERTIES COMPILE_FLAGS "${CNC_MPI_FLAGS}" SOVERSION ${SOVERSION})
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
  set_target_properties(cnc_socket_itac PROPERTIES COMPILE_FLAGS "${CNC_ITAC_FLAGS}" SOVERSION ${SOVERSION})
  include_directories(${ITACROOT}/include)
  #set_target_properties(cnc_socket_itac PROPERTIES INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} "${ITACROOT}/include")
  set_itac_link_flags(cnc_socket_itac)
  target_link_libraries(cnc_socket_itac cnc ${CNC_ADD_LIBS} ${ITACCS_LIB})
  if(BUILD_LIBS_FOR_MPI)
    set(CNC_LIBS ${CNC_LIBS} cnc_mpi_itac)
    add_library(cnc_mpi_itac SHARED ${CNC_MPI_SOURCES} )
    set_target_properties(cnc_mpi_itac PROPERTIES COMPILE_FLAGS "${CNC_MPI_FLAGS} ${CNC_ITAC_FLAGS}" SOVERSION ${SOVERSION})
    # doesn't seem to work:  set_target_properties(cnc_mpi_itac PROPERTIES INCLUDE_DIRECTORIES "${MPIROOT}/${MPIARCH}/include" INCLUDE_DIRECTORIES "${ITACROOT}/include")
    include_directories("${MPIROOT}/${MPIARCH}/include") # "${ITACROOT}/include")
    set_itac_link_flags(cnc_mpi_itac)
    target_link_libraries(cnc_mpi_itac cnc ${CNC_ADD_LIBS} ${MPI_LIB} ${ITAC_LIB})
  endif(BUILD_LIBS_FOR_MPI)
endif(BUILD_LIBS_FOR_ITAC)

#            {'option': 'cflags=-DWITHOUT_SENDER_THREAD'}


################################################################################
# add a target to generate API documentation with Doxygen

if (NOT DEFINED DATA)
  set(DATA "share/icnc")
endif(NOT DEFINED DATA)

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CNC_SRC_ROOT}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  
  set(DOXY_DEPS
    ${CNC_SRC_ROOT}/doc/main.dox
    ${CNC_SRC_ROOT}/cnc/cnc.h
    ${CNC_SRC_ROOT}/cnc/debug.h
    ${CNC_SRC_ROOT}/cnc/default_tuner.h
    ${CNC_SRC_ROOT}/cnc/dist_cnc.h
    ${CNC_SRC_ROOT}/cnc/join.h
    ${CNC_SRC_ROOT}/cnc/reduce.h
    ${CNC_SRC_ROOT}/cnc/itac.h
    ${CNC_SRC_ROOT}/cnc/serializer.h
    ${CNC_SRC_ROOT}/cnc/default_partitioner.h
    ${CNC_SRC_ROOT}/cnc/internal/cnc_tag_hash_compare.h
    ${CNC_SRC_ROOT}/doc/code.dox
  )
  ADD_CUSTOM_COMMAND( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile ${CNC_SRC_ROOT}/Doxyfile.in
    DEPENDS ${DOXY_DEPS}
    COMMENT "Generating HTML documentation"
    )
  ADD_CUSTOM_TARGET(doc ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doc/html/index.html)
  INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html/ DESTINATION ${DATA}/doc/api)
endif(DOXYGEN_FOUND)


################################################################################
# Install

if(NOT DEFINED LIB)
  set(LIB "lib/${PARCH}")
endif(NOT DEFINED LIB)
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
  install(TARGETS ${CNC_LIBS} RUNTIME DESTINATION bin/${PARCH})
  install(TARGETS cnc LIBRARY DESTINATION ${LIB} RUNTIME DESTINATION bin/${PARCH} ARCHIVE DESTINATION ${LIB})
  install(DIRECTORY ${CNC_SRC_ROOT}/src/dist/socket_comm/test/ DESTINATION ${DATA}/misc/distributed/socket FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ FILES_MATCHING REGEX ".*(sh|bat)$")
else()
  install(TARGETS cnc ${CNC_LIBS} LIBRARY DESTINATION ${LIB} RUNTIME DESTINATION bin/${PARCH})
  install(DIRECTORY ${CNC_SRC_ROOT}/src/dist/socket_comm/test/ DESTINATION ${DATA}/misc/distributed/socket FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_WRITE GROUP_READ FILES_MATCHING REGEX ".*sh$")
  install(DIRECTORY ${CNC_SRC_ROOT}/src/dist/socket_comm/test/ DESTINATION ${DATA}/misc/distributed/socket FILE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_WRITE GROUP_READ FILES_MATCHING REGEX ".*bat$")
endif()
install(DIRECTORY ${CNC_SRC_ROOT}/cnc DESTINATION include FILES_MATCHING PATTERN "*.h")
foreach(smpl fib
             primes
	     mandelbrot
	     rtm_stencil
	     blackscholes
	     cholesky
	     matrix_inverse
	     FaceDetection
	     floyd_warshall
	     nqueens
	     dedup
	     uts
	     reduce_2d
	     count_words
	     count_all_words
	     db
	     hidden_graph
	     mpicnc
	     heat_equation
	     search_in_sorted_tree
	     raytracer
	     jacobi2d
	     auto_graph_pipeline)
    install(DIRECTORY ${CNC_SRC_ROOT}/samples/${smpl} DESTINATION ${DATA}/samples/)
    install(FILES samples/Makefile DESTINATION ${DATA}/samples/${smpl}/)
endforeach(smpl)
# example codes from eight patterns document
foreach(smpl reduce_rows
	     producer_consumer
	     partition_string
	     divide_conquer
	     red_rows_until_pred
	     gen_new_iter_space)
    install(DIRECTORY ${CNC_SRC_ROOT}/samples/${smpl} DESTINATION ${DATA}/samples/eight_patterns/)
    install(FILES samples/Makefile DESTINATION ${DATA}/samples/eight_patterns/${smpl}/)
endforeach(smpl)
install(FILES samples/CMakeLists.txt samples/Makefile samples/allsamples.sln DESTINATION ${DATA}/samples/)

option(ENABLE_TESTING "Enable in-source testing" FALSE)
if(ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif(ENABLE_TESTING)
