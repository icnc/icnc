# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(rtm_stencil_tiled ../../samples/rtm_stencil/rtm_stencil_tiled/rtm_stencil_tiled.cpp "480 480 480 6" rtm_stencil.ref "")
add_cnc_test(rtm_stencil ../../samples/rtm_stencil/rtm_stencil_halo/rtm_stencil_halo.cpp "480 480 480 6" rtm_stencil.ref "")

if(${RUN_DIST})
  set_target_properties(distrtm_stencil_tiled rtm_stencil_tiled PROPERTIES COMPILE_FLAGS "-DUSE_BSIZE=80")
  set_target_properties(distrtm_stencil rtm_stencil PROPERTIES COMPILE_FLAGS "-DUSE_BSIZE=100")
endif()
