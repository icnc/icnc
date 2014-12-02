#-------------------------------------------------
# some argument checking:
# test_cmd is the command to run with all its arguments
if( NOT test_cmd )
   message( FATAL_ERROR "Variable test_cmd not defined" )
endif( NOT test_cmd )

# output_blessed contains the name of the "blessed" output file
if( NOT output_blessed )
   message( FATAL_ERROR "Variable output_blessed not defined" )
endif( NOT output_blessed )

# output_test contains the name of the output file the test_cmd will produce
if( NOT output_test )
  set(out_file "d.u.m.m.y")
#   message( FATAL_ERROR "Variable output_test not defined" )
endif( NOT output_test )

# output_test contains the name of the output file the test_cmd will produce
if( NOT compare_file )
  message( FATAL_ERROR "Variable compare_file not defined" )
endif( NOT compare_file )

# mode of execution: shared, MPI, SOCKETS or SHMEM
if( NOT dist_mode )
   message( FATAL_ERROR "Variable dist_mode not defined" )
endif( NOT dist_mode )

# mode of comparison: txt or bin
if(NOT compare_mode)
   message( FATAL_ERROR "Variable compare_mode not defined" )
endif(NOT compare_mode)

if( NOT test_env )
   message( FATAL_ERROR "Variable test_env not defined" )
endif( NOT test_env )

# convert the space-separated string to a list
separate_arguments( test_args )

#get_filename_component(test_outdir ${CMAKE_CURRENT_BINARY_DIR}/${test_cmd} DIRECTORY )
#if(${output_test} STREQUAL ${compare_file})
#  set(compare_file ${CMAKE_CURRENT_BINARY_DIR}/${compare_file})
#endif()
#set(output_test ${CMAKE_CURRENT_BINARY_DIR}/${output_test})

message("python ${CMAKE_CURRENT_LIST_DIR}/run.py ${CMAKE_CURRENT_BINARY_DIR}/${test_cmd} ${dist_mode} ${output_test} ${test_env} ${test_args}")

execute_process(
   COMMAND python ${CMAKE_CURRENT_LIST_DIR}/run.py ${CMAKE_CURRENT_BINARY_DIR}/${test_cmd} ${dist_mode} ${output_test} "${test_env}" ${test_args} RESULT_VARIABLE hadd_error
)

if(had_error)
  message(SEND_ERROR "Running test ${test_cmd} failed")
  #FATAL_ERROR?
endif(had_error)

message("python ${CMAKE_CURRENT_LIST_DIR}/compare.py ${output_blessed} ${compare_file} ${compare_mode}")

execute_process(
   COMMAND python ${CMAKE_CURRENT_LIST_DIR}/compare.py ${output_blessed} ${compare_file} ${compare_mode} RESULT_VARIABLE differs
)

if(differs)
   message( SEND_ERROR "${compare_file} does not match ${output_blessed}!" )
endif(differs)
#------------------------------------------------- 
