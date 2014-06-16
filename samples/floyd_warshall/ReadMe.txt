Description
===========
Floyd Warshall All Pairs Shortest Paths Algorithm.
Using step-priorities in step-tuners.

3 flavors:
  - floyd_1d_tiled 
    Description of the distribution strategy can be found
    at http://www.mcs.anl.gov/~itf/dbpp/text/node35.html

  - floyd_2d_tiled
    Description of the distribution strategy can be found 
    at http://www.mcs.anl.gov/~itf/dbpp/text/node35.html 

  - floyd_3d_tiled (best implementation)
    Details of the blocked algorithm can be found in the
    paper titled A Blocked All-Pairs Shortest-Paths Algorithm
    citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.13.2925

DistCnC enabling
================

The code is distCnC-ready with a tuner which implements a
different distribution plans.
Use the preprocessor definition _DIST_ to enable distCnC.
    
See the runtime_api reference for how to run distCnC programs.


Usage
=====

./floyd_cnc.exe -n <problem_size> -b <block_size>
                [-check] [-cnc] [-omp] [-seq] 

    -check: Compares the results with seq version of the algorithm
    -cnc: Runs a cnc implementation. Each directory has a different 
          parallel implementation 
    -omp: Runs a simple 2d blocked OpenMP implementation
    -seq: Runs a sequential implementaion
    -n: The number of nodes in the graph 
    -b: Block size used for all dimensions of blocking 

e.g. ./floyd_cnc.exe -n 2000 -b 1000 -omp -cnc
