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

/*
  Defintions for internal ITAC instrumentation.
  See cnc/itac.h
*/

#ifndef _CNC_COMM_ITAC_INTERNAL_H_
#define _CNC_COMM_ITAC_INTERNAL_H_

#ifdef CNC_WITH_ITAC

# include <cnc/itac.h>
//# define VT_THREAD_NAME( thrName ) --> defined in itac.h
# define VT_SERVER_INIT( name, numContacts, contacts ) { const char* dummy = NULL; VT_serverinit( name, numContacts, contacts, &dummy ); }
# define VT_CLIENT_INIT( rank, name, contact ) { *contact = NULL; VT_clientinit( rank, name, contact ); }
# define VT_FINALIZE() VT_finalize()
//# define VT_ENTER( funcName ) VT_enter( _VT_DEFINE_##funcName##_, VT_NOSCL )
//# define VT_LEAVE( funcName ) VT_leave( _VT_DEFINE_##funcName##_ )
//# define VT_FUNC( name ) --> defined in itac.h
# ifdef NO_MSG_TRACING
#  define VT_SEND( rank, nBytes, id )
#  define VT_RECV( rank, nBytes, id )
# else
#  define VT_SEND( rank, nBytes, id ) VT_log_sendmsg( rank, nBytes, id, VT_COMM_WORLD, VT_NOSCL )
#  define VT_RECV( rank, nBytes, id ) VT_log_recvmsg( rank, nBytes, id, VT_COMM_WORLD, VT_NOSCL )
# endif

#else // CNC_WITH_ITAC

# define VT_THREAD_NAME( thrName )
# define VT_SERVER_INIT( name, numContacts, contacts )
# define VT_CLIENT_INIT( rank, name, contact )
# define VT_FINALIZE()
//# define VT_ENTER( name )
//# define VT_LEAVE( name )
# define VT_FUNC( name )
# define VT_SEND( rank, nBytes, id )
# define VT_RECV( rank, nBytes, id )

#endif

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// !! Function tracing of internal worklet functions must be
// !! switched on explicitly via INTERNAL_ITAC !!
#ifdef INTERNAL_ITAC
# define VT_FUNC_I( f ) VT_FUNC( f )
#else
# define VT_FUNC_I( f )
#endif

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#endif // _CNC_COMM_ITAC_INTERNAL_H_
