# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  cholesky
  "../../samples/cholesky/cholesky/cholesky.cpp ../../samples/cholesky/cholesky/init.cpp ../../samples/cholesky/cholesky/cholesky_cnc.cpp"
  "1000 100 -i /tmp/m1000.in -o -"
  m1000.compare
  ""
)

add_custom_command(
  TARGET cholesky
  POST_BUILD
  COMMAND unzip -o -d /tmp ${CMAKE_CURRENT_LIST_DIR}/Input_matrix.zip
  COMMAND unzip -o -d ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/compare.zip
)

#SET_TESTS_PROPERTIES(cholesky PROPERTIES DEPENDS /tmp/m1000.in)
#set_property(TEST cholesky APPEND PROPERTY DEPENDS /tmp/m1000.compare)
