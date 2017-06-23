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

#ifndef  _CnC_STATSISTICS_H_
#define  _CnC_STATSISTICS_H_

#include <cnc/internal/tbbcompat.h>
#include "tbb/atomic.h"
#include <cnc/internal/cnc_api.h>
#include <ostream>

namespace CnC {
    namespace Internal {

/// define NO_STATS to get rid of stats code entirely
#ifndef NO_STATS
/// \def LOG_STATS
/// Using  LOG_STATS( _stats, _attr ) allows us to get rid of stats code entirely
#define LOG_STATS( _stats, _attr ) if ( (_stats) ) (_stats)->_attr
#else
#define LOG_STATS( _stats, _attr )
#endif

        /// Different statistics are collected in one object.
        /// Usually each context has its own statics object.
        class CNC_API statistics
        {
        public:
            statistics();
            /// a step was created
            int step_created();
            /// a step was scheduled
            int step_scheduled();
            /// a step was launched
            int step_launched();
            /// a step returned (completed or not)
            int step_terminated();
            /// a step was suspended to local queue
            int step_suspended();
            /// a step was re-scheduled
            int step_resumed();
            /// one or more message were sent
            int msg_sent( int gc = 1 );
            /// one message was received
            int msg_recvd( );
            /// one bcast message was sent
            int bcast_sent( );
            /// print statistics gathered so far
            void print_statistics( std::ostream & );

        private:
            // Scheduler statistics
            tbb::atomic< int >  m_steps_created;  
            tbb::atomic< int >  m_steps_scheduled;  
            tbb::atomic< int >  m_steps_launched;  
            tbb::atomic< int >  m_steps_suspended;  
            tbb::atomic< int >  m_steps_resumed; 
            tbb::atomic< int >  m_msgs_sent;
            tbb::atomic< int >  m_msgs_recvd;
            tbb::atomic< int >  m_bcasts_sent;
        }; // class context_base

    } // namespace Internal
} // end namespace CnC
#endif // _CnC_STATSISTICS_H_
