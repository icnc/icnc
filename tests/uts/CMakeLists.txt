# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(
  uts
  "../../samples/uts/uts-1.1/uts_cnc.cpp ../../samples/uts/uts-1.1/uts.c ../../samples/uts/uts-1.1/rng/brg_sha1.c"
  " "
  uts.ref
  ""
)
set_target_properties(uts PROPERTIES COMPILE_FLAGS "-DBRG_RNG")

if(${RUN_DIST})
  set_target_properties(distuts PROPERTIES COMPILE_FLAGS "-DBRG_RNG")
endif()
