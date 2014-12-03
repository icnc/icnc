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

#ifndef _TBB_CONCURRENT_QUEUE_SCHEDULER_H_
#define _TBB_CONCURRENT_QUEUE_SCHEDULER_H_

#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_queue.h>
#include <tbb/tbb_thread.h>
#include <cnc/internal/tls.h>
#include <tbb/concurrent_priority_queue.h>

namespace CnC {
    namespace Internal {

        template< typename S >  struct tcq_init;
        class schedulable;

        /// \brief manual task stealing scheduler with local queues per thread and one global queue.
        ///
        /// If stealing is disabled, all schedulables are dispatched using a single global FIFO queue.
        /// All threads take their work from there.
        ///
        /// With stealing, each thread owns his own local queue and identified by a local tid, which is passed to run_steps.
        /// m_queues[0] is the global queue, where env puts its steps.
        /// The logic around which queue to pop a step from is implemented in pop_next.
        /// It first tries the local queue, second is the global queue, last it iterates through all other queues
        /// trying to steal. After a round of unsuccessfull stealing attempts, it blocks on the global queue.
        ///
        /// Each thraed pushes steps into its local queue. It sends a wakeup call to the global queue if
        /// at least one thread is waiting there.
        ///
        /// when all threads are blocking in the global queue, we know that no more steps are available for processing.
        ///
        /// Potentially (but unlikely) a newly available step might not be executed immediately, even though
        /// threads are available: in schedule: when other threads are about to block on the global queue, 
        /// a newly scheduled step might will not trigger a wakeup call, even though just after the check
        /// all other threads can potentially block on the global queue. It cannot cause a dead/live lock, because
        /// at least one thread is still active and progressing.
        template< typename Q, bool use_affinity >
        class tbb_concurrent_queue_scheduler_base : public scheduler_i
        {
        public:
            tbb_concurrent_queue_scheduler_base( context_base &, int numThreads, bool steal, int hts );
            virtual ~tbb_concurrent_queue_scheduler_base();
            virtual void do_schedule( schedulable * stepInstance );
            virtual void wait( const inflight_counter_type & steps_in_flight );

        protected:
            virtual void enqueue_waiter();

        private:
            struct thread_info;

            void pop_next( int tid, schedulable *& si, bool block = true );

            typedef Q my_concurrent_queue;
            typedef TLS_static< my_concurrent_queue * > my_tls_queue;

            static my_concurrent_queue   * m_queues;
            static tbb::concurrent_bounded_queue< schedulable * > * m_gQueue;
            static tbb::tbb_thread      ** m_threads;
            static int                     m_numThreads;
            static my_tls_queue            m_localQueue;
            const bool                     m_steal;
			const int                      m_htstride;
            struct run_steps;
            friend struct run_steps;
            friend struct tcq_init< tbb_concurrent_queue_scheduler_base< Q, use_affinity > >;
        };

        typedef tbb_concurrent_queue_scheduler_base< tbb::concurrent_bounded_queue< schedulable * >, false > tbb_concurrent_queue_scheduler;
        typedef tbb_concurrent_queue_scheduler_base< tbb::concurrent_priority_queue< schedulable * >, false > tbb_concurrent_queue_prioritized_scheduler;
        typedef tbb_concurrent_queue_scheduler_base< tbb::concurrent_bounded_queue< schedulable * >, true > tbb_concurrent_queue_affinity_scheduler;
        typedef tbb_concurrent_queue_scheduler_base< tbb::concurrent_priority_queue< schedulable * >, true > tbb_concurrent_queue_prioritized_affinity_scheduler;

    } // namespace Internal
} // namespace CnC


#endif //_TBB_CONCURRENT_QUEUE_SCHEDULER_H_
