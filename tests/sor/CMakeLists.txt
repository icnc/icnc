# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  sor3D
  "../../samples/sor/sor3D/sor3D_NewApi.cpp ../../samples/sor/sor3D/TiledInputArray.cpp ../../samples/sor/sor3D/Tile.cpp"
  "0.3 100 100 50 10 10 -v"
  sor3D_NewApi_w03_100x100_10x10_x50.ref
)
add_cnc_test(
  sorDataTiled
  ../../samples/sor/sorDataTiled/sorDataTiled_NewApi.cpp
  "0.3 100 100 50 10 10 -v"
  sorDataTiled_NewApi_w03_100x100_10x10_x50.ref
)
add_cnc_test(
  sorTiled
  ../../samples/sor/sorTiled/sorTiled_NewApi.cpp
  "0.3 100 100 50 10 10 -v"
  sorTiled_NewApi_w03_100x100_x50_10x10.ref
)
add_cnc_test(
  sorPrior
  "../../samples/sor/sorPrior/sorPrior_NewApi.cpp ../../samples/sor/sorPrior/TiledInputArray.cpp ../../samples/sor/sorPrior/Tile.cpp"
  "0.3 100 100 50 10 10 -v"
  sorPrior_NewApi_w03_100x100_10x10_x50.ref
)

#    { 'name': 'sor',
#      'file': 'sor/sor.cpp',
#      # TEMPTOGGLE: Ryan, disabling this for now as we transition to new API/translator:
#          'cnc-spec': 'sor/sor.cnc',
#      'arguments': [{'string': '0.3'},{'string': '100'},{'string': '100'},{'string': '50'},{'string': '-v'},],
#      'redirect-output': '$TESTDIR/sor.out',
#      'verify': 'sor/sor_w03_100x100_x50.compare',
#      },