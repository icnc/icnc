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


#ifndef _CnC_DISTRIBUTABLE_CONTEXT_H_
#define _CnC_DISTRIBUTABLE_CONTEXT_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/creatable.h>
#include <cnc/internal/dist/distributable.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/spin_mutex.h>
#include <cnc/internal/scalable_vector.h>
#include <tbb/concurrent_queue.h>

namespace CnC {

    class serializer;
    
    namespace Internal {

        class distributable;
        class statistics;

        /// This makes a context distributable.
        /// It is the contact point for their collections/distributables for communication.
        /// Additionally collections/graphs un/register themselves here by calling un/subscribe.
        /// Any distributable context will be cloned on remote processes as soon as needed.
        /// The cloning is lazy, so we can change contexts after initialization and
        /// get the updated data to remote processes through implementing serialize().
        /// Note: once cloned, updates don't promoted!
        /// \see CnC::Internal::dist_init
        class CNC_API distributable_context : public creatable, public distributable, CnC::Internal::no_copy
        {
        public:
            distributable_context( const std::string & name );
            virtual ~distributable_context();

            /// \return true, if ready for communication etc.
            bool dist_ready() const { return m_distributionReady; };
            void init_dist_ready() const;
            void fini_dist_ready();

            /// return true if distribution is enabled, false if not
            bool distributed() const { return m_distributionEnabled; }

            /// de-serialize message from serializer and consume it
            /// might route message to one of its collections
            /// messages can only be sent thorugh the distributor only to its clones
            void recv_msg( serializer * );

            /// collections of a dist_context must get serialiazer through their containing dist_context
            /// when done, serializer must be freed by caller using "delete" (usually done in the Communicator)
            serializer * new_serializer( const distributable * ) const;

            /// collections of a dist_context must send their messages through their containing dist_context
            void send_msg( serializer *, int rcver ) const;

            /// collections of a dist_context must send their messages through their containing dist_context
            void bcast_msg( serializer * ) const;

            /// collections of a dist_context must send their messages through their containing dist_context
            bool bcast_msg( serializer *, const int * rcvers, int nrecvrs ) const;

            /// register distributable and return its global id.
            int subscribe( distributable * );

            /// unregister distributable and return its global id.
            void unsubscribe( distributable * );
            
            /// from creatable
            virtual void serialize( serializer & );
            /// from traceable
            virtual void set_tracing( int level );

            /// reset all its distributables
            void reset_distributables( bool local_only = false );
            /// clean up all its distributables (GC etc.)
            void cleanup_distributables( bool bcast );
            /// must be implemented by child class to spawn cleanup as service task
            virtual void spawn_cleanup() = 0;

            void init_scheduler_statistics();
            void print_scheduler_statistics();
            statistics * stats() const { return m_statistics; }

        protected:
            typedef tbb::spin_mutex mutex_type;
            mutable mutex_type       m_mutex;

        private:
            // Scheduler statistics
            statistics             * m_statistics;
            typedef scalable_vector_p< distributable * > distributable_container;
            distributable_container  m_distributables;
            tbb::concurrent_bounded_queue< int >  m_barrier;      ///< simluates a conditional variable/semaphore
        protected:
            bool                     m_distributionEnabled;
            bool                     m_distributionReady;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CnC_DISTRIBUTABLE_CONTEXT_H_
