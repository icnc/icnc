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
  Misc utilities and defintions.
*/

#ifndef  _CnC_STDDEF_H_
#define  _CnC_STDDEF_H_
 
#include <cnc/internal/cnc_api.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/queuing_mutex.h>
#include <iostream>
#include <sstream>
#include <cstdlib>


#ifdef  __GNUC__
#define _UNUSED_ __attribute__((unused))
#else
#define _UNUSED_ 
#endif

namespace CnC {

    typedef unsigned long long size_type;

    namespace Internal {


        /// Base class for types that should not be copied or assigned.
        class no_copy
        {
        private:
            // Disallow assignment
            void operator=( const no_copy& );
            // Disallow copy construction
            no_copy( const no_copy& );
        public:
            // Allow default construction
            no_copy() {}
        };

        /// Use this whenever printing something.
        /// A Speaker is a shallow wrapper that makes writing to a stream thread-safe. 
        /// A Speaker gets initialized with the output stream and flushes
        /// the stream only when it gets deleted. This allows locking to a minimum.
        class CNC_API Speaker : public std::ostringstream 
        {
        public:
            Speaker( std::ostream & os = std::cout );

            template< typename T > Speaker & operator<<( const T & obj )
            {
                static_cast< std::ostringstream & >( *this ) << obj;
                return *this;
            }
            // catch cnc_format things so that they don't get printed twice
            Speaker & operator<<( const std::ostream & )
            {
                return *this;
            }

            ~Speaker();

        private:
            std::ostream & m_os;
        };



#ifdef CNC_USE_ASSERT
#define CNC_ASSERT_MSG( _predicate_, _message_ ) ((_predicate_)?((void)0):CnC::Internal::cnc_assert_failed( __FILE__, __LINE__, #_predicate_, (_message_) ))
#define CNC_ASSERT( _predicate_ )                ((_predicate_)?((void)0):CnC::Internal::cnc_assert_failed( __FILE__, __LINE__, #_predicate_, NULL ))

        void CNC_API cnc_assert_failed( const char* filename, int line, const char* expression, const char* message );

#else
#define CNC_ASSERT_MSG( _predicate_, _message_ ) ((void)0)
#define CNC_ASSERT( _predicate_ )                ((void)0)

#endif /* CNC_USE_ASSERT */

#define CNC_ABORT( _message_ ) { { ::CnC::Internal::Speaker oss( std::cerr ); oss << __FILE__ << ":" << __LINE__ << " " << (_message_) << ", aborting execution."; } abort(); }

    } // namespace Internal
} // namespace CnC

#endif // _CnC_STDDEF_H_
