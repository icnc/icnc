# see parent $CNCROOT/tests/CMakeLists.txt

add_executable(
  dedup
  ../../samples/dedup/dedup/binheap.cpp ../../samples/dedup/dedup/encoder_cnc.cpp
  ../../samples/dedup/dedup/hashtable.cpp ../../samples/dedup/dedup/rabin.cpp
  ../../samples/dedup/dedup/util.cpp ../../samples/dedup/dedup/dedup.cpp
  ../../samples/dedup/dedup/encoder_util.cpp ../../samples/dedup/dedup/queue.cpp
  ../../samples/dedup/dedup/tree.cpp ../../samples/dedup/dedup/getopt/getopt.cpp
)
target_link_libraries(dedup ${NEEDED_LIBS} z m ssl rt crypto)

add_compare_test(
  dedup
  dedup
  "-c -i ${CMAKE_CURRENT_LIST_DIR}/ipf_man_vol3.pdf -o /tmp/ipf_man_vol3.out"
  ""
  "/tmp/ipf_man_vol3.out"
  "ipf_man_vol3.compress"
  "bin"
  "shared"
)
target_link_libraries(dedup ${NEEDED_LIBS} z m ssl rt crypto)
