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

#pragma once
#ifndef _CNC_ITAC_H_
#define _CNC_ITAC_H_

/**
\page itac Using Intel(R) Trace Analyzer and Collector with CnC
A CnC program run can be traced with Intel(R) Trace Collector (ITC)
and post-morten analyzed with Intel(R) Trace Analyzer (ITA)
(http://software.intel.com/en-us/intel-trace-analyzer)

The CnC header files are instrumented with hooks into ITC; when
compiling with CNC_WITH_ITAC being defined the compiler will insert
calls into ITC. Those work in shared and distributed mode, with distCnC
you'll not only see the step-execution but also the communication.
If you construct your collections with names (string parameters to 
their constructors) then the instrumentation will use those names and
create a meaningful trace without further manual instrumentation.

The CnC runtime will initialize ITC automatically. In shared memory 
this will happen when the first context is created, in distributed memory
right when dist_cnc_init is created. You can call VT_INIT earlier
in the code if you want to to see ITC things before the first context
creation (shared memory).

Please see \ref itac_in on how to instrument your code further.

After instrumentation/recompilation the execution of your applicaton
"app" will let ITC write a tracefile "app.stf". To start analysis,
simply execute "traceanalyzer app.stf".

\section itac-tips Hints
For a more convinient file handling it is
recommended to let ITC write the trac in the "SINGLESTF" format:
simply set VT_LOGFILE_FORMAT=SINGLESTF when runnig your application.

Usually CnC codes are threaded. ITC will create a trace in which every
thread is shown as busy from it's first acitivity to its termination,
even if it is actually idle. To prevent this set VT_ENTER_USCERCODE=0
at application runtime.

\section mpitrace Intel(R) MPI and ITC
The switch "-trace" to mpi[exec|run|cc] does not work
out-of-the-box. However, if using the instrumented version ITC will
load automatically. For this, compile with -DCNC_WITH_ITAC and link
against ITAC (see \ref itac_build). To load ITC in a
"plain" MPI environment (without recompiling or -linking), just
preload the following libraries:
\code
env LD_PRELOAD="libcnc.so:libcnc_mpi_itac.so:libVT.so" env DIST_CNC=MPI mpiexec -n ...
\endcode
or in debug mode
\code
env LD_PRELOAD="libcnc_debug.so:libcnc_mpi_itac_debug.so:libVT.so" env DIST_CNC=MPI mpiexec -n ...
\endcode

\section itac_build Compiling and linking with ITAC instrumentation
Apparently you need a working isntallation of ITAC. When compiling add
the include-directory of ITAC to your include path
(-I$VT_ROOT/include).  When linking, add the slib directory to the
lib-path and link against the following libraries
- SOCKETS:-L$VT_SLIB_DIR -lVTcs $VT_ADD_LIBS
- MPI: use mpicxx/mpiicpc -trace -L$VT_SLIB_DIR
Please refer to the ITAC documentation for more details.
**/

/// \defgroup itac_in Instrumenting with/for ITAC
/// @{
/// Two Macros are provided for a convenient way to instrument your code with ITAC.
/// When adding instrumentation you need to take care about building your bainry properly (\ref itac_build).
#ifdef CNC_WITH_ITAC

#include <VT.h>
#include <iostream>

namespace CnC {
namespace Internal {

    struct ITAC_FUNC {
        ITAC_FUNC( int id ) { VT_enter( id, VT_NOSCL ); }
        ~ITAC_FUNC() { VT_leave( VT_NOSCL ); }
    };

} // namespace Internal
} // namespace CnC

#define VT_THREAD_NAME( threadName ) VT_registernamed( threadName, -1 )

#define VT_FUNC( funcName ) __VT_FUNC__( funcName, __LINE__ )
#define VT_FUNC_D( funcName ) __VT_FUNC_D__( funcName, __LINE__ )

#define VT_INIT() VT_initialize( 0, 0 )

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// -- Implementational macros for VT_FUNC --
// Guarantee that generated variable names get the line number appended.
// This allows to have several VT_FUNC statements in the same scope.
#define __VT_FUNC__( _funcName, _line ) ____VT_FUNC____( _funcName, _line, static )
#define __VT_FUNC_D__( _funcName, _line ) ____VT_FUNC____( _funcName, _line, )
#define ____VT_FUNC____( _funcName, _line, _qual )      \
    _qual int ____CnC_vt_func_id____##_line = 0; \
    if ( ____CnC_vt_func_id____##_line == 0 ) { VT_funcdef( _funcName, VT_NOCLASS, &____CnC_vt_func_id____##_line ); } \
    CnC::Internal::ITAC_FUNC ____CnC_vt_func_obj____##_line( ____CnC_vt_func_id____##_line )

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// you better not use this
#define VT_SYMDEF( _id, _name ) VT_symdef( _id, _name, NULL )

#else

#define VT_SYMDEF( _id, _name )

/// \def VT_THREAD_NAME( threadName ) 
/// Use this macro to define an (optional) name for your application thread, e.g. VT_THREAD_NAME( "myApp" ).
/// This name will appear for your thread in ITAC's event timeline.
/// \param threadName  name of calling thread (in double quotes), to appear in ITAC tracefile
#define VT_THREAD_NAME( threadName )

/// \def VT_FUNC( funcName )
/// Use this macro to manually instrument your function (or scope) at its beginning, e.g. VT_FUNC( "MyClass::myMethod" );
/// See also \ref itac_build
/// \param funcName  name of function (in double quotes), potentially including class/namespace hierachy
#define VT_FUNC( funcName )

/// \def VT_FUNC_D( funcName )
/// Similar to VT_FUNC, but the recorded function name can change between invocations.
/// It is slower, so don't use it unless really needed.
/// See also \ref itac_build
/// \param funcName  name of function (in double quotes), potentially including class/namespace hierachy
#define VT_FUNC_D( funcName )

/// \def VT_INIT()
/// In shared memory, ITC needs to be initialized. This is done automatically with the first 
/// context construction. Use this only if you want to see things in the trace before the first
/// context is created. Don't use it for distributed memory.
# define VT_INIT()

#endif //CNC_WITH_ITAC

/// @}

#endif // _CNC_ITAC_H_
