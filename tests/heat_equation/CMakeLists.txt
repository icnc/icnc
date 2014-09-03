# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  heat_equation
  ../../samples/heat_equation/heat_equation/heat_equation.cpp
  "${CMAKE_CURRENT_LIST_DIR}/../../samples/heat_equation/heat_equation/heatEquationData.txt 16 y"
  heat_equation.ref
  ""
)
