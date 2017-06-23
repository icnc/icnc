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

#ifndef _CnC_FACTORY_H_
#define _CnC_FACTORY_H_

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    // Workaround for overzealous compiler warnings 
    #pragma warning (push)
    #pragma warning (disable: 4251 4275)
#endif

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/creator.h>
#include <cnc/internal/scalable_vector.h>

namespace CnC {
    namespace Internal {

        class void_context {};

        /// This class handles remote instantiation of remotable objects.
        /// Only objects of subscribed types can be instantiated on remote clients.
        /// Subscribed types must be default constructable.
        /// \see CnC::Internal::dist_cnc
        class CNC_API factory
        {
        public:
            factory();
            ~factory();

            /// create object of given type
            static creatable * create( int id );

            /// make remotable type known to factory
            /// sets the type identifier
            template< class Creatable >
            static int subscribe();

        private:
            typedef scalable_vector_p< creator_i * > creator_list_type;
            static creator_list_type m_creators;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Creatable >
        int factory::subscribe()
        {
            if( creator< Creatable >::type_ident() < 0 ) {
                creator< Creatable > * _crtbl = new creator< Creatable >;
                m_creators.push_back( _crtbl );
                _crtbl->set_type_ident( static_cast< int >( m_creators.size() ) - 1 );
            }
            return creator< Creatable >::type_ident();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template<>
        inline int factory::subscribe< void_context >()
        {
            return -1;
        }

    } // namespace Internal
} // namespace CnC

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    #pragma warning (pop)
#endif // warnings 4251 4275 are back

#endif // _CnC_FACTORY_H_
