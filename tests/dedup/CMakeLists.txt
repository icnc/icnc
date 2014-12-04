# see parent $CNCROOT/tests/CMakeLists.txt

find_library(Z_LIB z)
find_library(SSL_LIB ssl)
find_library(RT_LIB rt)
find_library(CRYPTO_LIB crypto)

if( ${Z_LIB} MATCHES NOTFOUND OR ${SSL_LIB} MATCHES NOTFOUND OR ${RT_LIB} MATCHES NOTFOUND OR ${CRYPTO_LIB} MATCHES NOTFOUND )
message("missing library (zlib, ssl, rt or crypto), skipping dedup")
else()
add_executable(
  dedup
  ../../samples/dedup/dedup/binheap.cpp ../../samples/dedup/dedup/encoder_cnc.cpp
  ../../samples/dedup/dedup/hashtable.cpp ../../samples/dedup/dedup/rabin.cpp
  ../../samples/dedup/dedup/util.cpp ../../samples/dedup/dedup/dedup.cpp
  ../../samples/dedup/dedup/encoder_util.cpp ../../samples/dedup/dedup/queue.cpp
  ../../samples/dedup/dedup/tree.cpp ../../samples/dedup/dedup/getopt/getopt.cpp
)
target_link_libraries(dedup ${NEEDED_LIBS} ${Z_LIB} m ${SSL_LIB} ${RT_LIB} ${CRYPTO_LIB})

add_compare_test(
  dedup
  dedup
  "-c -i ${CMAKE_CURRENT_LIST_DIR}/ipf_man_vol3.pdf -o ${CMAKE_CURRENT_BINARY_DIR}/ipf_man_vol3.out"
  "${CMAKE_CURRENT_BINARY_DIR}/dedup.out"
  "${CMAKE_CURRENT_BINARY_DIR}/ipf_man_vol3.out"
  "ipf_man_vol3.compress"
  "bin"
  "shared"
)
target_link_libraries(dedup ${NEEDED_LIBS} z m ssl rt crypto)
endif()
