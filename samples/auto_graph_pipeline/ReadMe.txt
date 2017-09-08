Description
===================
This example illustrates how two arbitrary library functions can be
wrapped up as CnC graphs (by experts manually), and composed together
and distributed to a cluster (by a compiler automatically), running in
a parallel, asynchronous fashion without barriers. Intuitively, such
an execution style is more efficient than the traditional style of
Bulk-Synchronous Parallel, where a library function computes in
parallel, then communicates and synchronizes before the next library
function call starts. Additionally, it leverages CnC's capability of
avoiding unnecessary data transfers, e.g. data is only sent to remote
processes if they are actually used/needed there.
 
The example is an early demonstration of the concept. It exemplifies
the approach with the following simple program:
    C = A*B
    E = C*D.
The two matrix multiplications are replaced with CnC graphs, which are
composed into a pipeline.

Although this is an extremely simplified example and the composition
is done manually here, it clearly shows a general mechanism that a
compiler can automate.

More information can be found from the following talk:

    Transparently Composing CnC Graph Pipelines on a Cluster
    Hongbo Rong and Frank Schlimbach
    The Seventh Annual Concurrent Collections Workshop, 2015
    https://www.researchgate.net/publication/285232588_Transparently_Composing_CnC_Graph_Pipelines_on_a_Cluster

Prerequisite
================
Any recent version of g++ with CBLAS, or icpc with CBLAS or MKL,
should work.

The compilers we have tried are 
    gcc version 4.8.1 (Ubuntu 4.8.1-2ubuntu1~12.04) 
and
    icpc version 15.0.0 (gcc version 4.8.0 compatibility)

Build
================
In the Makefile, modify the following variables as needed: 
  rows=100                    # Number of rows of any matrix involved,
                              #  which is assumed to be square
  block_rows=10               # The number of rows in a block.
  use_composed=-DUSE_COMPOSED # Enable composed functions
                              #  (i.e., composed graphs).
                              #  Comment out right hand side to disable
  validate=-DVALIDATE         # Enable printing checksum of the results.

Then 
    make [USE_MKL=1] [all,auto_graph_pipeline,distauto_graph_pipeline,clean]

Usage (distributed memory)
===================
env DIST_CNC=MPI mpirun -n <#processes> ./distauto_graph_pipeline

Known issues
===================
When compiled with icpc, sometimes, at the end of the execution, there
may appear the following message:
    rank 0 in job 50  caused collective abort of all ranks
    exit status of rank 0: killed by signal 11 
But the execution seems to have finished correctly (with the right
checksum printed).
