# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(stream_with_state ../../samples/streaming/stream_with_state/stream_with_state.cpp "1000000" stream_with_state.ref)
add_cnc_test(thread_ring ../../samples/streaming/thread_ring/thread_ring.cpp "1000000" thread_ring.ref)
add_cnc_test(thread_ring_1_stepcoll ../../samples/streaming/thread_ring_1_stepcoll/thread_ring_1_stepcoll.cpp "1000000" thread_ring_1_stepcoll.ref)
