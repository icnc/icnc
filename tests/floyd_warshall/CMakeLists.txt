# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  floyd1d
  "../../samples/floyd_warshall/floyd_1d_tiled/floyd_cnc.cpp ../../samples/floyd_warshall/common/utils.cpp ../../samples/floyd_warshall/common/floyd_base.cpp"
  "-n 1500 -b 100 -cnc -check"
  ../no.ref
  ""
)
add_cnc_test(
  floyd2d
  "../../samples/floyd_warshall/floyd_2d_tiled/floyd_cnc.cpp ../../samples/floyd_warshall/common/utils.cpp ../../samples/floyd_warshall/common/floyd_base.cpp"
  "-n 1500 -b 100 -cnc -check"
  ../no.ref
  ""
)
add_cnc_test(
  floyd3d
  "../../samples/floyd_warshall/floyd_3d_tiled/floyd_cnc.cpp ../../samples/floyd_warshall/common/utils.cpp ../../samples/floyd_warshall/common/floyd_base.cpp"
  "-n 1500 -b 100 -cnc -check"
  ../no.ref
  ""
)
