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
  Stuff to check/enforce compatibility with different versions of TBB.
*/

#ifndef  _CnC_TBBCOMPAT_H_
#define  _CnC_TBBCOMPAT_H_

// #ifndef TBB_DEPRECATED_MUTEX_COPYING
// # define TBB_DEPRECATED_MUTEX_COPYING 1
// #endif

#include <tbb/tbb_stddef.h>
//#include <cnc/internal/cnc_stddef.h>

// we actually require TBB version 4.1 Update 1
// # define CNC_REQUIRED_TBB_VERSION 6101
// # define CNC_REQUIRED_TBB_VERSION_STRING "4.1 Update 1"
// but the TBB packages only ship with binaries for Visual Studio 2013
// with 4.2 Update 3 which uses 7003
#ifndef CNC_REQUIRED_TBB_VERSION
# define CNC_REQUIRED_TBB_VERSION 7003
# define CNC_REQUIRED_TBB_VERSION_STRING 4.2 Update 3
#else
# ifndef CNC_REQUIRED_TBB_VERSION_STRING
#  define CNC_REQUIRED_TBB_VERSION_STRING CNC_REQUIRED_TBB_VERSION
# endif
#endif

#ifdef CNC_PRODUCT_BUILD
// if we build a product package, we must use the minimal TBB version that's supported
# if TBB_INTERFACE_VERSION != CNC_REQUIRED_TBB_VERSION
#error Need TBB version CNC_REQUIRED_TBB_VERSION_STRING for product build
# endif
#endif

// any build of CnC must use at least the minimal TBB version
#if TBB_INTERFACE_VERSION < CNC_REQUIRED_TBB_VERSION
#error Need TBB version CNC_REQUIRED_TBB_VERSION_STRING or newer
#endif

// runtime check to make sure we are not running with an older TBB than
// what our build of CnC used.
#define CNC_CHECK_TBB_VERSION()                                         \
    if( tbb::TBB_runtime_interface_version() < TBB_INTERFACE_VERSION ) { \
        CNC_ABORT( "Running with older TBB than used at compile time" ); \
    }

#endif // _CNC_TBBCOMPAT_H_
