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
  see statistics.h
*/

#include <cnc/internal/statistics.h>
#include <cnc/internal/tbbcompat.h>
#include "tbb/atomic.h"

namespace CnC {
    namespace Internal {

        statistics::statistics()
            : m_steps_created(),
              m_steps_scheduled(), 
              m_steps_launched(),
              m_steps_suspended(),
              m_steps_resumed(),
              m_msgs_sent(),
              m_msgs_recvd(),
              m_bcasts_sent()
        {
            m_steps_created = 0;
            m_steps_scheduled = 0;
            m_steps_launched = 0;
            m_steps_suspended = 0;
            m_steps_resumed = 0;
            m_msgs_sent = 0;
            m_msgs_recvd = 0;
            m_bcasts_sent = 0;
        }

        int statistics::step_created()
        {
            return ++m_steps_created;
        }

        int statistics::step_scheduled()
        {
            return ++m_steps_scheduled;
        }

        int statistics::step_launched()
        {
            return ++m_steps_launched;
        }

        int statistics::step_terminated()
        {
            return --m_steps_launched;
        }

        int statistics::step_suspended()
        {
            return ++m_steps_suspended;
        }

        int statistics::step_resumed()
        {
            return ++m_steps_resumed;
        }

        int statistics::msg_sent( int gc )
        {
            return m_msgs_sent += gc;
        }

        int statistics::msg_recvd()
        {
            return ++m_msgs_recvd;
        }

        int statistics::bcast_sent()
        {
            return ++m_bcasts_sent;
        }

        void statistics::print_statistics( std::ostream & os )
        {
            os << "Steps created( " << m_steps_created << " )" << std::endl
               << "Steps scheduled( " << m_steps_scheduled 
               << " ) inflight( " << ( m_steps_suspended > m_steps_resumed ? m_steps_suspended - m_steps_resumed : 0 ) << ", " << m_steps_launched << " )" << std::endl
               << "Steps requeued( " << m_steps_suspended << " ) resumed( " << m_steps_resumed << " )" << std::endl;
            if( m_msgs_sent ) os << "Messages sent( " << m_msgs_sent << " ) ";
            if( m_msgs_recvd ) os << "Messages received( " << m_msgs_recvd << " ) ";
            if( m_bcasts_sent ) os << "Bcasts sent( " << m_bcasts_sent << " )";
            if( m_msgs_sent || m_msgs_recvd || m_bcasts_sent ) os << std::endl;
        }
            
    } // namespace Internal
} // end namespace CnC
