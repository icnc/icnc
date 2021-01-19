/* *******************************************************************************
 *  Copyright (c) 2007-2021, Intel Corporation
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

#ifndef _CnC_SCALABLE_VECTOR_H_
#define _CnC_SCALABLE_VECTOR_H_

#include <vector>
#include <cnc/internal/tbbcompat.h>
#include <tbb/scalable_allocator.h>

namespace CnC {
    namespace Internal {

        /// std::vector using TBB's scalable_allocator
        template< class T >
        class scalable_vector : public std::vector< T, tbb::scalable_allocator< T > >
        {
        public:
            scalable_vector() : std::vector< T, tbb::scalable_allocator< T > >() {}
            scalable_vector( const scalable_vector & v ) : std::vector< T, tbb::scalable_allocator< T > >( v ) {}
            scalable_vector( int n, const T & v = T()) : std::vector< T, tbb::scalable_allocator< T > >( n, v ) {}
        };

#if TBB_INTERFACE_VERSION < 6004 && defined(__GNUC__) && defined(__GNUC_MINOR__) && __GNUC__ == 4 && __GNUC_MINOR__ > 5
        // grr g++ 4.6. doesn't like TBB's allocator
#  define scalable_vector_p std::vector
#else
#  define scalable_vector_p Internal::scalable_vector
#endif

    } // namespace Internal
} // namespace CnC

#endif // _CnC_SCALABLE_VECTOR_H_
