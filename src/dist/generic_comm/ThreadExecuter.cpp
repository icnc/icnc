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
  see ThreadExecuter.h
*/

#include "pal_config.h" // _CRT_SECURE_NO_WARNINGS
#include "ThreadExecuter.h"
#include "itac_internal.h"
#include <cnc/internal/cnc_stddef.h>
#include <cstdio> // sprintf

namespace CnC
{
    namespace Internal
    {
        class ThreadExecuter::ThreadArgs
        {
            ThreadExecuter & m_instance;
        public:
            ThreadArgs( ThreadExecuter & instance ) : m_instance( instance ) {}
            void operator()();
        };

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void ThreadExecuter::ThreadArgs::operator()()
        {
             // Initialize ITAC thread name:
            if ( ! m_instance.m_threadName.empty() ) {
                if ( m_instance.m_threadNameSuffix < 0 ) {
                    VT_THREAD_NAME( m_instance.m_threadName.c_str() );
                } else {
                    // append the threadNameSuffix to the name
                    const int bufLen = 128;
                    char buf[bufLen]; // should be long enough for name + suffix
                    CNC_ASSERT( m_instance.m_threadName.length() + 10 < static_cast< unsigned int >( bufLen ) );
                    int nChars = sprintf( buf, "%s%d", m_instance.m_threadName.c_str(),
                                          m_instance.m_threadNameSuffix );
                    CNC_ASSERT( 0 < nChars && nChars < bufLen );
                    VT_THREAD_NAME( buf );
                }
            }

            // Perform the thread's "main" function:
            m_instance.runEventLoop();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        ThreadExecuter::ThreadExecuter()
            : m_thread( NULL ),
              m_threadArgs( NULL ),
              m_threadName()
        {}

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        ThreadExecuter::~ThreadExecuter()
        {
            stop();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void ThreadExecuter::defineThreadName( const char name[], int nameSuffix )
        {
            m_threadName = name;
            m_threadNameSuffix = nameSuffix;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void ThreadExecuter::start()
        {
            if ( m_thread != 0 ) return;

            m_threadArgs = new ThreadArgs( *this );
            m_thread = new std::thread( *m_threadArgs );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void ThreadExecuter::stop()
        {
            // You have to make sure that the worker thread will stop executing.
            // Here, we only join with it and rely on this.
            if ( m_thread ) {
                m_thread->join();
                delete m_thread;
                delete m_threadArgs;
                m_thread = NULL;
                m_threadArgs = NULL;
            }
        }

    } // namespace Internal
} // namespace CnC
