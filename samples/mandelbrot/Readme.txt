Description
===========

This program computes the mandelbrot set for a complex plane.

The step collection (compute) is indexed by the pair of x and y indexes of
the complex plane. (compute) inputs the instance of [data] with the same
indexes, as well as [max_depth] which is a singleton as the maximum iteration
count. The step outputs a instance of [pixel] which is the iteration count.

The program prints the count of complex numbers in the mandelbrot set.
A complex number is in the mandelbrot set if its instance of [pixel] has
the value max_depth.

If -v is specified, the output displays a simple image of the mandelbrot set.
Within the complex plane, if a complex value is in the mandelbrot set,
it's displayed as blank. Otherwise it's displayed as a dot.

The directory mandelbrot_vector contains a version with a larger
grain-size. Every row in the plane is stored in a vector. The vector
is wrapped in a shared_ptr to facilitate get-counted garbage
collection. Gcc and icc both require the flag -std=c++0x as the
shared_ptr is a feature of the new C++ standard.

DistCnC enabling
================

mandel.cpp is distCnC-ready.
Use the preprocessor definition _DIST_ to enable distCnC.
    
See the runtime_api reference for how to run distCnC programs.


Usage
=====

The command line is:

mandel [-v] rows columns max_depth
    -v :      verbose option, 
    rows :    the first dimension of the complex plane
    columns : the second dimension of the complex plane
    
e.g.
mandel -v 20 20 100
