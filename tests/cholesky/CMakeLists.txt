# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  cholesky
  "../../samples/cholesky/cholesky/cholesky.cpp ../../samples/cholesky/cholesky/init.cpp ../../samples/cholesky/cholesky/cholesky_cnc.cpp"
  "1000 100 -i ${CMAKE_CURRENT_BINARY_DIR}/m1000.in -o -"
  m1000.compare
  ""
)

add_custom_command(
  TARGET cholesky
  POST_BUILD
  COMMAND unzip -o -d ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_LIST_DIR}/Input_matrix.zip
  COMMAND unzip -o -d ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/compare.zip
)
