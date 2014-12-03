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

#ifndef _SCALABLE_OBJECT_HH_ALREADY_INCLUDED
#define _SCALABLE_OBJECT_HH_ALREADY_INCLUDED

#include <new>

#include <cnc/internal/tbbcompat.h>
#include <tbb/scalable_allocator.h>
#include <cnc/internal/cnc_api.h>

#ifdef WIN32
#pragma warning( push )
#pragma	warning( disable : 4290 )
#endif

namespace CnC {
    namespace Internal {

        /// derive from this to use TBB's scalable_malloc/free fro your class/objects
        class CNC_API scalable_object
        {
        public:
            inline void * operator new( size_t size ) throw( std::bad_alloc )
            {
                void * _ptr = scalable_malloc( size );
                if ( _ptr ) return _ptr;
                throw std::bad_alloc();
            }
            inline void operator delete( void * ptr ) throw()
            {
                if ( ptr != 0 ) scalable_free( ptr );
            }
            
        }; // class scalable_object

    } //    namespace Internal
} // end namespace CnC

#ifdef WIN32
#pragma warning( pop )
#endif

#endif //_SCALABLE_OBJECT_HH_ALREADY_INCLUDED
