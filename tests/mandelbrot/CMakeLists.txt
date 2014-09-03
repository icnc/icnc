# see parent $CNCROOT/tests/CMakeLists.txt

add_cnc_test(mandelbrot ../../samples/mandelbrot/mandelbrot/mandelbrot.cpp "100 100 10000" mandel10k.ref "")
add_cnc_test(mandelbrot_vector ../../samples/mandelbrot/mandelbrot_vector/mandelbrot_vector.cpp "100 100 10000" mandel10k.ref "")
