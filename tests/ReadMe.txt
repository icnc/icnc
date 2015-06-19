Testing CnC
===========

This directory contains a set of test to verify CnC works correctly.

Whenever new changes are commited to the master, it is expected that
the test-suite runs successfully.

Newly added features must be covered by some test.


Intro
-----
In general a CnC test works as follows
1. compile sources and link to a binary
2. run the test
3. compare the test output against a reference file
4. do 1-3 on distributed memory using MPI
5. do 1-3 on distributed memory using SOCKETS
6. do 1-3 on distributed memory using SHMEM


Prerequisites
-------------
Cmake and CTest
  We use CMake/CTest for all the test. We use the cmake script
  run_compare.cmake to run the programs to compare them. run_compare
  uses run.py to launch the programs and compare.py to compare output
  against the reference.
Python
  2 python scripts are used to run and compare
Compilers, TBB
  Using TBB version 4.3 (or later) doesn't work with g++ versions
  older than 4.8 (4.8 works fine).
Some tests/smaples have extra dependencies
  dedup: zlib and openssl (libopenssl-dev)
  db/mysql_simple: mysqlcppconn (libmysqlcppconn-dev)


Running the tests
-----------------
cd tests
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=<Release|Debug> <opts like -j 8>
cmake test


1. compile sources and link a binary
------------------------------------
Some tests reside in a tests directory, others keep their sources in
the samples directory. The provided CMake files make it very simple to
compile/link binaries by simply providing the list of source files.


2. run the test
---------------
run_compare uses run.py to launch a given program. run.py takes care
for setting environment variables (like DIST_CNC) and using launchers
(like mpirun) whenever needed. It accepts an output filename to which
it will pipe the programs stdout.


3. compare the test output against a reference file
---------------------------------------------------
compare.py compares binary files as they are.
In txt mode, it sorts the given output and then compares it against
the provided reference file. The reference file can contain python
regular expressions. The output file is accepted even if it contains
additional text as long as the reference expressions are found in the
same order.


Adding a new test
-----------------
The simplest way is to copy a CMakeLists.txt from an existing
directory. Create a new directory and put the CMakeLists.txt
there. You can also just add a new "target" to an existing file if its
of the same kind (e.g. a variation of an existing test).
To create a reference file use a blessed output and run it through
make_ref.py. The script will escape brackets and special characters to
protect them from being interpreted as an regex. You might still need
to manually adjust things, in particualr if the output is legally
variable (e.g. timings).
