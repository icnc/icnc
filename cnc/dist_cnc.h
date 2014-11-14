/* *******************************************************************************
 *  Copyright (c) 2007-2014, Intel Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

// ===============================================================================
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
//    INCLUDE THIS FILE ONLY TO MAKE YOUR PROGRAM READY FOR DISTRIBUTED CnC
//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// ===============================================================================

#ifndef __DIST_CNC__H_
#define __DIST_CNC__H_

/**
\page distcnc Running CnC applications on distributed memory

In principle, every clean CnC program should be immediately
applicable for distributed memory systems. With only a few trivial
changes most CnC programs can be made distribution-ready. You will
get a binary that runs on shared and distributed memory.  Most of
the mechanics of data distribution etc. is handled inside the
runtime and the programmer does not need to bother about the gory
details. Of course, there are a few minor changes needed to make a
program distribution-ready, but once that's done, it will run on
distributed CnC as well as on "normal" CnC (decided at runtime).

\section dc_comm Inter-process communication
Conceptually, CnC allows data and computation distribution
across any kind of network; currently CnC supports SOCKETS and MPI.

\section dc_link Linking for distCnC
Support for distributed memory is part of the "normal" CnC
distribution, e.g. it comes with the necessary communication
libraries (cnc_socket, cnc_mpi). The communication library is
loaded on demand at runtime, hence you do not need to link against
extra libraries to create distribution-ready applications. Just
link your binaries like a "traditional" CnC application (explained
in the CnC User Guide, which can be found in the doc directory).
\note a distribution-ready CnC application-binary has no dependencies
      on an MPI library, it can be run on shared memory or over SOCKETS
      even if no MPI is available on the system

Even though it is not a separate package or module in the CNC kit,
in the following we will refer to features that are specific for
distributed memory with "distCnC".

\section dc_prog Making your program distCnC-ready
As a distributed version of a CnC program needs to do things which
are not required in a shared memory version, the extra code for
distCnC is hidden from "normal" CnC headers. To include the
features required for a distributed version you need to
\code #include <cnc/dist_cnc.h> \endcode
instead of \code #include <cnc/cnc.h> \endcode .
If you want to be able to create optimized binaries for shared
memory and distributed memory from the same source, you might
consider protecting distCnC specifics like this:
  @code
   #ifdef _DIST_
   # include <cnc/dist_cnc.h>
   #else
   # include <cnc/cnc.h>
   #endif
  @endcode

In "main", initialize an object CnC::dist_cnc_init< list-of-contexts >
before anything else; parameters should be all context-types that
you would like to be distributed. Context-types not listed in here
will stay local. You may mix local and distributed contexts, but
in most cases only one context is needed/used anyway.
  @code
   #ifdef _DIST_
       CnC::dist_cnc_init< my_context_type_1 //, my_context_type_2, ...
                         > _dinit;
   #endif
  @endcode

Even though the communication between process is entirely handled by
the CnC runtime, C++ doesn't allow automatic marshaling/serialization
of arbitrary data-types. Hence, if and only if your items and/or tags
are non-standard data types, the compiler will notify you about the
need for serialization/marshaling capability. If you are using
standard data types only then marshaling will be handled by CnC
automatically.

Marshaling doesn't involve sending messages or alike, it only
specifies how an object/variable is packed/unpacked into/from a
buffer. Marshaling of structs/classes without pointers or virtual
functions can easily be enabled using
\code CNC_BITWISE_SERIALIZABLE( type); \endcode
others need a "serialize" method or function. The CnC kit comes
with an convenient interface for this which is similar to BOOST
serialization. It is very simple to use and requires only one
function/method for packing and unpacking. See \ref serialization for
more details.

<b>This is it! Your CnC program will now run on distributed memory!</b>

\attention Global variables are evil and must not be used within
           the execution scope of steps. Read \ref dist_global
           about how CnC supports global read-only data.
           Apparently, pointers are nothing else than global
           variables and hence need special treatment in distCnC
           (see \ref serialization).
\note Even if your program runs on distributed memory, that does not
      necessarily imply that the trivial extension above will make it
      run fast.  Please consult \ref dist_tuning for the tuning
      options for distributed memory.

The above describes the default "single-program" approach for
distribution.  Please refer to to CnC::dist_cnc_init for more advanced
modes which allow SPMD-style interaction as well as distributing 
parts of the CnC program over groups of processes.


\section dc_run Running distCnC
The communication infrastructure used by distCnC is chosen at
runtime.  By default, the CnC runtime will run your application in
shared memory mode.  When starting up, the runtime will evaluate
the environment variable "DIST_CNC".  Currently it accepts the
following values
- SHMEM : shared memory (default)
- SOCKETS : communication through TCP sockets
- MPI : using Intel(R) MPI

Please see \ref itac on how to profile distributed programs

\subsection dc_sockets Using SOCKETS
On application start-up, when DIST_CNC=SOCKETS, CnC checks the
environment variable "CNC_SOCKET_HOST".  If it is set to a number,
it will print a contact string and wait for the given number of
clients to connect. Usually this means that clients need to be
started "manually" as follows: set DIST_CNC=SOCKETS and
"CNC_SOCKET_CLIENT" to the given contact string and launch the
same executable on the desired machine.

If "CNC_SOCKET_HOST" is not a number it is interpreted as a
name of a script. CnC executes the script twice: First with "-n"
it expects the script to return the number of clients it will
start. The second invocation is expected to launch the client
processes.

There is a sample script "misc/start.sh" which you can
use. Usually all you need is setting the number of clients and
replacing "localhost" with the names of the machines you want the
application(-clients) to be started on. It requires password-less
login via ssh. It also gives some details of the start-up
procedure. For windows, the script "start.bat" does the same,
except that it will start the clients on the same machine without
ssh or alike. Adjust the script to use your preferred remote login
mechanism.

\subsection dc_mpi MPI
CnC comes with a communication layer based on MPI. You need the
Intel(R) MPI runtime to use it. You can download a free version of
the MPI runtime from
http://software.intel.com/en-us/articles/intel-mpi-library/ (under
"Resources").  A distCnC application is launched like any other
MPI application with mpirun or mpiexec, but DIST_CNC must be set
to MPI:
\code
env DIST_CNC=MPI mpiexec -n 4 my_cnc_program
\endcode
Alternatively, just run the app as usually (with DIST_CNC=MPI) and
control the number (n) of additionally spawned processes with
CNC_MPI_SPAWN=n.  If host and client applications need to be
different, set CNC_MPI_EXECUTABLE to the client-program
name. Here's an example:
\code
env DIST_CNC=MPI env CNC_MPI_SPAWN=3 env CNC_MPI_EXECUTABLE=cnc_client cnc_host
\endcode
It starts your host executable "cnc_host" and then spawns 3 additional 
processes which all execute the client executable "cnc_client".

\subsection dc_mic Intel Xeon Phi(TM) (MIC)
for CnC a MIC process is just another process where work can be computed
on. So all you need to do is
- Build your application for MIC (see
  http://software.intel.com/en-us/articles/intel-concurrent-collections-getting-started)
- Start a process with the MIC executable on each MIC card just
  like on a CPU. Communication and Startup is equivalent to how it
  works on intel64 (\ref dc_mpi and \ref dc_sockets).

\note Of course the normal mechanics for MIC need to be considered
      (like getting applications and dependent libraries to the MIC
      first).  You'll find documentation about this on IDZ, like 
      <A HREF="http://software.intel.com/en-us/articles/how-to-run-intel-mpi-on-xeon-phi">here</A>
      and/or <A HREF="http://software.intel.com/en-us/articles/using-the-intel-mpi-library-on-intel-xeon-phi-coprocessor-systems">here</A>
\note We recommend starting only 2 threads per MIC-core, e.g. if your
      card has 60 cores, set CNC_NUM_THREADS=120
\note To start different binaries with one mpirun/mpiexec command you
      can use a syntax like this:<br>
      mpirun -genv DIST_CNC=MPI -n 2 -host xeon xeonbinary : -n 1 -host mic0 -env CNC_NUM_THREADS=120 micbinary


\section def_dist Default Distribution
Step instances are distributed across clients and the host. By
default, they are distributed in a round-robin fashion. Note that
every process can put tags (and so prescribe new step instances).
The round-robin distribution decision is made locally on each
process (not globally).

If the same tag is put multiple times, the default scheduling
might execute the multiply prescribed steps on different processes
and the preserveTags attribute of tag_collections will then not
have the desired effect.

The default scheduling is intended primarily as a development aid.
your CnC application will be distribution ready with only little effort.
In some cases it might lead to good performance, in other cases
a sensible distribution is needed to achieve good performance.
See \ref dist_tuning.

Next: \ref dist_tuning


\page dist_tuning Tuning for distributed memory
The CnC tuning interface provides convenient ways to control the 
distribution of work and data across the address spaces. The
tuning interface is separate from the actual step-code and its
declarative nature allows flexible and productive experiments with
different distribution strategies.

\section dist_work Distributing the work
Let's first look at the distribution of work/steps.  You can specify
the distribution of work (e.g. steps) across the network by providing
a tuner to a step-collection (the second template argument to
CnC::step_collection, see \ref tuning). Similar to other tuning
features, the tuner defines the distribution plan based on the
control-tags and item-tags. For a given instance (identified by the
control-tag) the tuner defines the placement of the instance in the
communication network. This mechanism allows a declarative definition
of the distribution and keeps it separate from the actual program code
- you can change the distribution without changing the actual program.

The method for distributing steps is called "compute_on". It takes the
tag of the step and the context as arguments and has to return the
process number to run the step on. The numbering of processes is
similar to ranks in MPI. Running on "N" processes, the host process is
"0" and the last client "N-1".

  @code
   struct my_tuner : public CnC::step_tuner<>
   {
       int compute_on( const tag_type & tag, context_type & ) const { return tag % numProcs(); }
   };
  @endcode

The shown tuner is derived from CnC::step_tuner. To allow a flexible
and generic definition of the distribution CnC::step_tuner provides
information specific for distributed memory:
CnC::tuner_base::numProcs() and CnC::tuner_base::myPid(). Both return
the values of the current run of your application.  Using those allows
defining a distribution plan which adapts to the current runtime
configuration.

If you wonder how the necessary gets distributed - this will be
covered soon. Let's first look at the computation side a bit more
closely; but if you can't wait see \ref dist_data.

The given tuner above simply distributes the tags in a
round-robin fashion by applying the modulo operator on the tag. Here's
an example of how a given set of tags would be mapped to 4 processes
(e.g. numProcs()==4):
\verbatim
1  -> 1
3  -> 3
4  -> 0
5  -> 1
10 -> 2
20 -> 0
31 -> 3
34 -> 2
\endverbatim

An example of such a simple tuner is \ref bs_tuner.

Now let's do something a little more interesting. Let's assume our tag
is a pair of x and y coordinates. To distribute the work per row, we
could simply do something like

  @code
   struct my_tuner : public CnC::step_tuner<>
   {
       int compute_on( const tag_type & tag, context_type & ) const { return tag.y % numProcs(); }
   };
  @endcode

As you see, the tuner entirely ignores the x-part of the tag. This
means that all entries on a given row (identified by tag.y) gets
executed on the same process.  Similarly, if you want to distribute
the work per column instead, you simply change it to

  @code
   struct my_tuner : public CnC::step_tuner<>
   {
       int compute_on( const tag_type & tag, context_type & ) const { return tag.x % numProcs(); }
   };
  @endcode

As we'll also see later, you can certainly also conditionally switch
between row- and column-wise (or any other) distribution within
compute_on.

To avoid the afore-mentioned problem of becoming globally
inconsistent, you should make sure that the return value is
independent of the process it is executed on.

CnC provides special values to make working with compute_on more
convenient, more generic and more effective:
CnC::COMPUTE_ON_LOCAL, CnC::COMPUTE_ON_ROUND_ROBIN,
CnC::COMPUTE_ON_ALL, CnC::COMPUTE_ON_ALL_OTHERS.

\section dist_data Distributing the data
By default, the CnC runtime will deliver data items automatically
to where they are needed. In its current form, the C++ API does
not express the dependencies between instances of steps and/or
items. Hence, without additional information, the runtime does not
know what step-instances produce and consume which
item-instances. Even when the step-distribution is known
automatically automatic distribution of data requires
global communication. Apparently this constitutes a considerable
bottleneck. The CnC tuner interface provides two ways to reduce
this overhead.

The ideal, most flexible and most efficient approach is to map
items to their consumers.  It will convert the default pull-model
to a push-model: whenever an item becomes produced, it will be
sent only to those processes, which actually need it without any
other communication/synchronization. If you can determine which
steps are going to consume a given item, you can use the above
compute_on to map the consumer step to the actual address
spaces. This allows changing the distribution at a single place
(compute_on) and the data distribution will be automatically
optimized to the minimum needed data transfer.

The runtime evaluates the tuner provided to the item-collection
when an item is put.  If its method consumed_on (from
CnC::item_tuner) returns anything other than CnC::CONSUMER_UNKNOWN
it will send the item to the returned process id and avoid all the
overhead of requesting the item when consumed.
  @code
   struct my_tuner : public CnC::item_tuner< tag_type, item_type >
   {
       int consumed_on( const tag_type & tag ) 
       {
           return my_step_tuner::consumed_on( consumer_step );
       }
   };
  @endcode

As more than one process might consume the item, you
can also return a vector of ids (instead of a single id) and the
runtime will send the item to all given processes.
  @code
   struct my_tuner : public CnC::item_tuner< tag_type, item_type >
   {
       std::vector< int > consumed_on( const tag_type & tag ) 
       {
           std::vector< int > consumers;
           foreach( consumer_step of tag ) {
               int _tmp = my_step_tuner::consumed_on( consumer_step );
               consumers.push_back( _tmp );
           }
           return consumers;
       }
   };
  @endcode

Like for compute_on, CnC provides special values to facilitate and
generalize the use of consumed_on: CnC::CONSUMER_UNKNOWN,
CnC::CONSUMER_LOCAL, CnC::CONSUMER_ALL and
CnC::CONSUMER_ALL_OTHERS.

Note that consumed_on can return CnC::CONSUMER_UNKOWN for some
item-instances, and process rank(s) for others.

Sometimes the program semantics make it easier to think about the
producer of an item. CnC provides a mechanism to keep the
pull-model but allows declaring the owner/producer of the item. If
the producer of an item is specified the CnC-runtime can
significantly reduce the communication overhead because it on
longer requires global communication to find the owner of the
item. For this, simply define the depends-method in your
step-tuner (derived from CnC::step_tuner) and provide the
owning/producing process as an additional argument.

  @code
   struct my_tuner : public CnC::step_tuner<>
   {
       int produced_on( const tag_type & tag ) const
       {
           return producer_known ? my_step_tuner::consumed_on( tag ) : tag % numProcs();
       }
   };
  @endcode

Like for consumed_on, CnC provides special values
CnC::PRODUCER_UNKNOWN and CnC::PRODUCER_LOCAL to facilitate and
generalize the use of produced_on.

The push-model consumed_on smoothly cooperates with the
pull-model as long as they don't conflict.

\section dist_sync Keeping data and work distribution in sync
For a more productive development, you might consider implementing
consumed_on by thinking about which other steps (not processes)
consume the item. With that knowledge you can easily use the
appropriate compute_on function to determine the consuming process.
The great benefit here is that you can then change compute
distribution (e.g. change compute_on) and the data will automatically
follow in an optimal way; data and work distribution will always be in
sync.  It allows experimenting with different distribution plans with
much less trouble and lets you define different strategies at a single
place.  Here is a simple example code which lets you select different
strategies at runtime. Adding a new strategy only requires extending
the compute_on function:
\ref bs_tuner
A more complex example is this one: \ref cholesky_tuner

\section dist_global Using global read-only data with distCnC
Many algorithms require global data that is initialized once and
during computation it stays read-only (dynamic single assignment,
DSA).  In principle this is aligned with the CnC methodology as
long as the initialization is done from the environment.  The CnC
API allows global DSA data through the context, e.g. you can store
global data in the context, initialize it there and then use it in
a read-only fashion within your step codes.

The internal mechanism works as follows: on remote processes the
user context is default constructed and then
de-serialized/un-marshaled. On the host, construction and
serialization/marshaling is done in a lazy manner, e.g. not
before something actually needs being transferred.  This allows
creating contexts on the host with non-default constructors, but
it requires overloading the serialize method of the context.  The
actual time of transfer is not statically known, the earliest
possible time is the first item- or tag-put. All changes to the
context until that point will be duplicated remotely, later
changes will not.

Here is a simple example code which uses this feature:
\ref bs_tuner

Next: \ref non_cnc
**/

#ifdef _CnC_H_ALREADY_INCLUDED_
#warning dist_cnc.h included after cnc.h. Distribution capabilities will not be activated.
#endif

#ifndef _DIST_CNC_
# define _DIST_CNC_
#endif

#include <cnc/internal/dist/dist_init.h>

namespace CnC {
    namespace Internal {
        class void_context;
    }

    /// To enable remote CnC you must create one such object.  The
    /// lifetime of the object defines the "scope" of
    /// distribution. Contexts created in the "scope" of the
    /// dist_cnc_init objects (e.g. when it exists) will get
    /// distributed to participating processes (see \ref dc_run).
    ///
    /// Usually, a single dist_cnc_init object is created for the
    /// entire lifetime of a program.  e.g. the dist_cnc_init object
    /// is created right when entering main and (auto-)destructed when
    /// main terminates. In this default mode all processes other than
    /// the root/host process exit the program when the dist_cnc_init
    /// objects gets dextructed.
    ///
    /// Actually, the current implementation allows only a single
    /// dist_cnc_init object at a time for every process.  Hence, all
    /// contexts on a given process are distributed in the same way.
    /// However, an optional parameter/flag allows allows defining the
    /// processes that actually "share" the dist_cnc_init object (and
    /// so their contexts). An optional flag/parameter is interpreted
    /// as a MPI_Comm to be used by the dist_cnc_init scope.  This
    /// allows different groups of processes (defined by the
    /// MPI_Comm's) to work on different CnC contexts/graphs
    /// concurrently. If no MPI_Comm was specified (e.g. the default)
    /// client processes exit the program when the host dist_cnc_init
    /// object is destructed. If a MPI_Comm is provided they also wait
    /// until the host process destructs its dist_cnc_init object but
    /// simply returns from the constructor rather than exiting the
    /// program.  Apparently all this only works when using the MPI
    /// communication infrastructure.
    ///
    /// Additionally, two modes of operation are supported:
    /// 1. By default, constructing a dist_cnc_init objects blocks all
    ///    processes except the root process in the constructor.
    ///    Hence, code after the object instantiation will be executed
    ///    only on the host process.
    /// 2. If dist_env is set to true, the constructor returns on all
    ///    processes and execution continues in a SPMD style, e.g. all
    ///    processes continue program execution. The SPMD style mode
    ///    allows alternating between MPI phases and CnC phases. This
    ///    mode is currently supported only using MPI communication.
    ///    You have to ensure that all processes fully completed their
    ///    local context creation before putting any data into a
    ///    context's collection. Similarly, you have to synchronize
    ///    context-destruction. It is recommended to put a MPI_Barrier
    ///    right after instantiating a context and just before it gets
    ///    destructed (e.g. at the end of its scope).
    ///
    /// \note It is possible to combine SPMD mode and providing a 
    ///       MPI_Comm. You can even change the grouping in phases by
    ///       using different MPI_Comm's at different times of the
    ///       execution. E.g. the lifetime of a dist_cnc_object might
    ///       be a (collective) function call. Make sure each process
    ///       has only single dist_cnc_object alive at each point in
    ///       time.
    ///
    /// \note All context classes ever used in the program must be
    ///       referenced as template arguments if they should be
    ///       distributed.
    /// \note All distributed contexts must have all
    ///       collections they use as members and must be
    ///       default-constructible.
    /// \note Pointers as tags are not supported by distCnC.
    ///
    /// Execution and other internal details described in
    /// CnC::Internal::dist_init
    template< class C1, class C2 = Internal::void_context, class C3 = Internal::void_context,
              class C4 = Internal::void_context, class C5 = Internal::void_context >
    struct /*CNC_API*/ dist_cnc_init : public Internal::dist_init< C1, C2, C3, C4, C5 >
    {
        dist_cnc_init() : Internal::dist_init< C1, C2, C3, C4, C5 >() {}
        /// \param dist_env enable SPMD-style access to contexts
        /// \param flag MPI_Comm to be used (MPI only)
        dist_cnc_init( bool dist_env, long flag = 0  ) : Internal::dist_init< C1, C2, C3, C4, C5 >( flag, dist_env ) {}
        /// \param dist_env enable SPMD-style access to contexts
        /// \param flag MPI_Comm to be used (MPI only)
        dist_cnc_init( long flag, bool dist_env = false ) : Internal::dist_init< C1, C2, C3, C4, C5 >( flag, dist_env ) {}
    };

} // namespace CnC

#include <cnc/cnc.h>

#endif // __DIST_CNC__H_
