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
  see tls.h
*/

#include <pthread.h>
#include <cstdlib>

#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/tls.h>

namespace CnC {
    namespace Internal {

        int CnC_TlsAlloc()
        {
            pthread_key_t key;
    
            int status = pthread_key_create( &key, NULL );
            if( status ) CNC_ABORT( "TLS key creation failed." );

            return (int)key;
        }


        void *CnC_TlsGetValue( int key )
        {
            return pthread_getspecific((pthread_key_t)key );
        }


        void CnC_TlsSetValue( int key, void *value)
        {
            pthread_setspecific( (pthread_key_t)key, value );
        }

        void CnC_TlsFree( int key )
        {

            int status = pthread_key_delete( (pthread_key_t)key );
            if( status ) CNC_ABORT( "TLS key deletion failed." );
        }

    } //    namespace Internal
} // namespace CnC
