# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(primes ../../samples/primes/primes/primes.cpp "100000" fp100k.ref "")
add_cnc_test(primes_range ../../samples/primes/primes_range/primes_range.cpp "100000" fp100k.ref)
