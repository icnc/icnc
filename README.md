## Intel(R) Concurrent Collections for C++

**Parallelism without the Pain**

The CnC homepage is here: https://icnc.github.io

### Prerequisites
* Linux\* or Windows\* (iOS\* should work, but hasn’t been tested yet)
* TBB (Intel(R) Threading Building Blocks) version 4.1 or later (http://threadingbuildingblocks.org/download)
* cmake (http://www.cmake.org/cmake/resources/software.html)
* C++ compiler: g++ >= 4.4, icc >= 11.0, Microsoft Visual Studio* >= 10 (2010)
* make (Linux\*), nmake or Microsoft Visual Studio\* (Windows\*)


### Building CnC
You need cmake to prepare building the CnC libraries from source. A python script is provided for your convenience to build the package:
```bash
cd <cnc_src_root>
python make_kit.py
```
It will create a tbz package for you in directory \<cnc_src_root\>/kit.pkg where you also find an "installation" of CnC under \<cnc_src_root\>/kit.pkg/cnc/current

You can specify the MPI root/install-dir with "--mpi=\<mpiroot\>. Also try "python make_kt.py -h" for more options.

#### Building CnC using CnC directly
You can of course drive cmake yourself. It is recommended to avoid in-source builds and to do something like
```bash
cd <cnc_src_root>
mkdir build
cd build
cmake <options> ..
```
Primary \<options\> for building the CnC libraries are
* Build libraries to use MPI (distributed memory): -DBUILD_LIBS_FOR_MPI=TRUE
* Build libraries for profiling with Intel(R) Trace Analyzer and Collector: -DBUILD_LIBS_FOR_ITAC=TRUE
* See generic options below

After successfully running cmake you can you use preferred build tool to build the libraries:
On Linux, just type 'make', On Windows use the tool according to the generator you selected above.

The libraries will be build in the "build" directory, on Linux in "lib", on Windows you'll find them in "Debug" or "Release".

### Building CnC samples
Building samples also requires cmake. 
```bash
cd <cnc_src_root>
mkdir build
cd build
cmake <options> ..
```
Primary \<options\> for building samples are
* Specify CnC install/root directory: -DCNCROOT=\<path_to_cnc\>
* Enable distributed memory: -DDIST_CNC=TRUE
* Enable hooks for tracing with Intel(R) Trace Analyzer and Collector: -DUSE_ITAC=TRUE
* See generic options below

After successfully running cmake you can you use your preferred build tool to build the samples:
On Linux, just type 'make', On Windows use the tool according to the generator you selected above.

### Generic cmake options
* Linux*: Select optimized or debug build mode: -DCMAKE_BUILD_TYPE=[Release|Debug]
* Windows*: Select appropriate generator
  * For nmake: -G "NMake Makefiles"
  * For Visual Studio 2010 32bit: -G "Visual Studio 10"
  * For Visual Studio 2010 64bit: -G "Visual Studio 10 Win64"
  * For Visual Studio 2011 32bit: -G "Visual Studio 11"
  * For Visual Studio 2011 64bit: -G "Visual Studio 11 Win64"
  * For Visual Studio 2012 32bit: -G "Visual Studio 12"
  * For Visual Studio 2012 64bit: -G "Visual Studio 12 Win64"
* Specify TBB install/root directory: -DTBBROOT=\<path_to_tbb\> (defaults to $TBBROOT)
* Specify MPI install/root directory: -DMPIROOT=\<path_to_mpi\> (defaults to $I_MPI_ROOT)
* Specify ITAC install/root directory: -DITACROOT=\<path_to_itac\> (defaults to $VT_ROOT)

### Testing
See [tests/ReadMe.txt](tests).

### Code documentation
Most header files start with a description of the functionality of the given module. As this is a template library, most code is actually in header files.

### Source structure
All header files which are needed to write a CnC application with pre-compiled libraries reside in the cnc directory. Internal headers are in cnc/internal, internals for distributed memory can be found in cnc/internal/dist. Anything else is in src.

Every class has its own header-file (and sometimes also a cpp file).

### Your contribution is welcome!
