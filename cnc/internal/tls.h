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

#ifndef _CnC_TLS_H_
#define _CnC_TLS_H_

// A generic interface to Thread Local Storage (TLS) that we
// can implement on Windows or Linux.

namespace CnC {
    namespace Internal {

        /// A TLS wrapper class to be used for global static TLS vars only.
        /// It is not safe for any other use, it relies on zero initilization AND sequential initialization
        template< typename T >
        class TLS_static
        {
        public:
            typedef T value_type;
            /// constructor
            TLS_static();
            ~TLS_static();
            /// return thread-local value
            T get() const;
            void set( const T & v ) const;
        private:
            int m_key;
            enum state_t { UNINITIALIZED = 0, INITED, FINI };
            state_t m_state;
        };

         
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        int CnC_TlsAlloc();
        void *CnC_TlsGetValue( int index );
        void CnC_TlsSetValue( int index, void *value);
        void CnC_TlsFree( int index );
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename T >
        TLS_static< T >::TLS_static()
        {
            // this will only work if zero-initilized in a non-threaded env
            if( m_state == UNINITIALIZED ) {
                m_key = CnC_TlsAlloc();
                m_state = INITED;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename T >
        TLS_static< T >::~TLS_static() {
            if( m_state == INITED ) {
                CnC_TlsFree( m_key );
                m_state = FINI;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename T >
        T TLS_static< T >::get() const
        {
            CNC_ASSERT( sizeof( T ) <= sizeof( void* ) );
            return (T)( CnC_TlsGetValue( m_key ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename T >
        void TLS_static< T >::set( const T & v ) const
        {
            CNC_ASSERT( sizeof( T ) <= sizeof( void* ) );
            CnC_TlsSetValue( m_key, (void*)( v ) );
        }

    } //    namespace Internal
} // namespace CnC

#endif // _CnC_TLS_H_
