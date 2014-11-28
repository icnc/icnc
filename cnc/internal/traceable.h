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

#ifndef _CnC_TRACEABLE_H_
#define _CnC_TRACEABLE_H_

#include <string>
#include <tbb/queuing_mutex.h>
#include <cnc/internal/cnc_api.h>

namespace CnC {
    namespace Internal {
 
        typedef tbb::queuing_mutex tracing_mutex_type;

        /// derive from this if your new class shall leave traces
        class traceable
        {
        public:
            traceable( const std::string & name, int level = 0 )
                : m_traceName( name ), m_traceLevel( level ) {}
            traceable( )
                : m_traceName( "traceable" ), m_traceLevel( 0 ) {}
            virtual ~traceable() {}

            /// \return level of tracing, 0 == no tracing
            int trace_level() const { return m_traceLevel & 0xffff; }
            /// \return name to be used for tracing
            const std::string & name() const { return m_traceName; }
            /// set tracing level
            virtual void set_name( const std::string & name )
            { m_traceName = name; }
            /// set tracing level
            virtual void set_tracing( int level )
            { m_traceLevel = level; }
            /// set tracing name and level
            void set_timing()
            { m_traceLevel |= 0x10000; }
            bool timing() const
            { return (m_traceLevel & 0x10000) != 0; }

        private:
            std::string  m_traceName;
            int          m_traceLevel;
        };

        extern CNC_API tracing_mutex_type s_tracingMutex;

    } // namespace Internal
} // namespace cnc

#endif // _CnC_TRACEABLE_H_
