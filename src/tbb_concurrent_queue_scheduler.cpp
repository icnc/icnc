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
  see tbb_concurrent_queue_scheduler.h
*/

#include "tbb_concurrent_queue_scheduler.h"
#include "pinning.h"
#include <cnc/internal/schedulable.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/statistics.h>
#include <cnc/internal/tbbcompat.h>
#include <atomic>
#include <thread>
#include <tbb/queuing_rw_mutex.h>
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/service_task.h>
#include <cstdlib>

namespace CnC {
    namespace Internal {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // this can be pushed to global queue to wake up "sleeping" threads
        class wakeup_step : public schedulable
        {
        public:
            wakeup_step( scheduler_i & sched ) : schedulable( 0, sched ) {}
			
			virtual int affinity() const { return scheduler_i::AFFINITY_HERE; }
            virtual StepReturnValue_t execute() { return CNC_Success; } // do nothing
            virtual char prepare( step_delayer &, int &, const schedulable * ) { CNC_ABORT( "Unexpected code path taken" ); return CNC_Unfinished; }       // should never get called
            virtual void compute_on( int ) { CNC_ABORT( "Internal step cannot be distributed." ); }
        };

        namespace {
            static wakeup_step * wakeUpStep = NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename S >
        struct tcq_init
        {
            ~tcq_init()
            {
                // don't use distributor in here, might already be gone
                if( S::m_queues != NULL ) {
                    CNC_ASSERT( S::m_queues->empty() );
                    for( int i = 0; i < S::m_numThreads; ++ i ) S::m_gQueue->push( NULL );
                    for( int i = 0; i < S::m_numThreads; ++ i ) {
                        if( S::m_threads[i] != NULL ) {
                            S::m_threads[i]->join();
                            delete S::m_threads[i];
                        }
                    }
                    delete [] S::m_threads;
                    delete [] S::m_queues;
                    delete S::m_gQueue;
                    delete wakeUpStep;
                    S::m_threads = NULL;
                    S::m_queues = NULL;
                    S::m_gQueue = NULL;
                    S::m_numThreads = 0;
                    wakeUpStep = NULL;
                }
            }
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


        /// main loop for TBB worker threads
        template< typename Q, bool use_affinity >
        struct tbb_concurrent_queue_scheduler_base< Q, use_affinity >::run_steps
        {
            void operator()( tbb_concurrent_queue_scheduler_base< Q, use_affinity > * scheduler, int tid, const scheduler_i::inflight_counter_type * num = NULL )
            {
				if( tid > 0 && scheduler->m_htstride ) pin_thread( tid, -1, scheduler->m_htstride );
                // first set local queue
                if( scheduler->m_steal ) scheduler->m_localQueue.set( &scheduler->m_queues[tid] );
                schedulable * _si = NULL;

                do {
                    scheduler->pop_next( tid, _si, num == NULL ); // get next step, might block in global queue
                    if( _si ) {
                        CNC_ASSERT( _si );
                        if( _si != wakeUpStep ) {
                            _si->scheduler().do_execute( _si );
                        } else {
                            //scheduler_i::set_current( _si );
                            //_si->execute();
                            scheduler_i::set_current( NULL );
                        }
                    }
                } while( _si && ( num == NULL || *num > 1 ) );
                scheduler->m_localQueue.set( NULL );
                //                { Speaker oss; oss << "*************** Thread is done " << tid << " " << _si; }
            }
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Q, bool use_affinity >
        void tbb_concurrent_queue_scheduler_base< Q, use_affinity >::pop_next( int tid, schedulable *& si, bool block )
        {
            if( m_steal ) {
                my_concurrent_queue * _currQueue = &m_queues[tid];
                if( _currQueue->try_pop( si ) ) return;
                // if( m_queues->try_pop( si ) ) {
                //     if( si == wakeUpStep ) m_gQueue->push( wakeUpStep ); // accidently stole wakup step
                //     else return;
                // }
#if __MIC__ || __MIC2__
                int _s = tid % 60; // first thread on my cpu
                if( _s == tid ) _s += 60;
# define _TFI( _i, _np ) ((_i)*60/(_np)+((_i)%((_np)/60)*60))
#else
                int _s = ( tid + 1 ) % m_numThreads;
# define _TFI( _i, _np ) _i
#endif
                for( int i = 0; i != m_numThreads; ++i ) {
                    int _sid = ( _s + i ) % m_numThreads;
                    _sid = _TFI( _sid, m_numThreads );
                    if( m_queues[_sid].try_pop( si ) ) {
                        return;
                    }
                }
                if( block ) {
                    // block, unless we are the root thread
                    // all threads should gather here if local queues are empty
                    m_gQueue->pop( si );
                    CNC_ASSERT( si == NULL || si == wakeUpStep );
                    return;
                }
                // nothing found on root thread, must not block, so let's return
                si = NULL;
            } else {
                si = NULL;
                if( m_queues->try_pop( si ) ) return;
                if( block ) {
                    m_gQueue->pop( si );
                } else {
                    m_gQueue->try_pop( si );
                }
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // this must be a global var, local static vars are not thread-safe
        static tbb::queuing_rw_mutex _mtx;

        template< typename Q, bool use_affinity >
        tbb_concurrent_queue_scheduler_base< Q, use_affinity >::tbb_concurrent_queue_scheduler_base( context_base & c, int numThreads, bool steal, int hts )
            : scheduler_i( c ),
              m_steal( steal ),
              m_htstride( hts )
        {
            tbb::queuing_rw_mutex::scoped_lock _lock( _mtx );
            if( m_numThreads == 0 && numThreads > 0 ) {
                bool _amroot = distributor::myPid() == 0;
                CNC_ASSERT( m_gQueue == NULL && m_queues == NULL && m_threads == NULL && wakeUpStep == NULL );
                m_gQueue = new tbb::concurrent_bounded_queue< schedulable * >;
                m_numThreads = numThreads;
                m_queues = new my_concurrent_queue[m_steal ? m_numThreads : 1];
                m_threads = new std::thread*[m_numThreads];
                m_threads[0] = NULL;
                for( int i = _amroot ? 1 : 0; i < m_numThreads; ++i ) {
                    m_threads[i] = new std::thread( run_steps(), this, i );
                }
                wakeUpStep = new wakeup_step( *this );
				if( m_htstride ) pin_thread( 0, -1, m_htstride );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Q, bool use_affinity >
        tbb_concurrent_queue_scheduler_base< Q, use_affinity >::~tbb_concurrent_queue_scheduler_base()
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Q, bool use_affinity >
        void tbb_concurrent_queue_scheduler_base< Q, use_affinity >::do_schedule( schedulable * stepInstance )
        {
            if( m_steal ) {
				const int _aff = use_affinity ? stepInstance->affinity() : AFFINITY_HERE;
				if( _aff == AFFINITY_HERE || _aff >= m_numThreads ) {
                    typename my_tls_queue::value_type _myQueue = m_localQueue.get();
					if( _myQueue == NULL ) {
						_myQueue = m_queues;
					}
					_myQueue->push( stepInstance );
				} else {
					m_queues[_aff].push( stepInstance );
				}
            } else {
                m_queues->push( stepInstance );
            }
                // wake another thread if at least one thread is waiting
            if( m_gQueue->size() < 0 ) m_gQueue->push( wakeUpStep );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Q, bool use_affinity >
        void tbb_concurrent_queue_scheduler_base< Q, use_affinity >::wait( const inflight_counter_type & steps_in_flight )
        {
            //            const int _remain = distributor::myPid() == 0 ? 0 : 1;
            // while( m_gQueue->size() != _remain - m_numThreads ) {
			while( steps_in_flight > 1 ) {
                // help executing steps to empty queue
                run_steps r;
                r( this, 0, &steps_in_flight );
            }
            // now we would like to assume that the queue is empty; might not be in dist case
			// but even with nested scheduling this doesn't hold
            // CNC_ASSERT( m_gQueue->empty() || distributor::active() );
        }

        namespace {
            struct tcq_waiter
            {
                template< typename Q, bool use_affinity >
                void execute( tbb_concurrent_queue_scheduler_base< Q, use_affinity > * scheduler )
                {
                    scheduler->wait_loop( true );
                }
            };
        }

        template< typename Q, bool use_affinity >
        void tbb_concurrent_queue_scheduler_base< Q, use_affinity >::enqueue_waiter()
        {
            new service_task< tcq_waiter, tbb_concurrent_queue_scheduler_base< Q, use_affinity > * >( *this, this );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // concrete specializations and instantiations

        template class tbb_concurrent_queue_scheduler_base< tbb::concurrent_bounded_queue< schedulable * >, false >;
        static tcq_init< tbb_concurrent_queue_scheduler > sg_initer1;
        template class tbb_concurrent_queue_scheduler_base< tbb::concurrent_priority_queue< schedulable * >, false >;
        static tcq_init< tbb_concurrent_queue_prioritized_scheduler > sg_initer2;
        template class tbb_concurrent_queue_scheduler_base< tbb::concurrent_bounded_queue< schedulable * >, true >;
        static tcq_init< tbb_concurrent_queue_affinity_scheduler > sg_initer3;
        template class tbb_concurrent_queue_scheduler_base< tbb::concurrent_priority_queue< schedulable * >, true >;
        static tcq_init< tbb_concurrent_queue_prioritized_affinity_scheduler > sg_initer4;

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Q, bool use_affinity > typename tbb_concurrent_queue_scheduler_base< Q, use_affinity >::my_concurrent_queue  * tbb_concurrent_queue_scheduler_base< Q, use_affinity >::m_queues = NULL;
        template< typename Q, bool use_affinity > std::thread ** tbb_concurrent_queue_scheduler_base< Q, use_affinity >::m_threads = NULL;
        template< typename Q, bool use_affinity > int tbb_concurrent_queue_scheduler_base< Q, use_affinity >::m_numThreads = 0;
        template< typename Q, bool use_affinity > typename tbb_concurrent_queue_scheduler_base< Q, use_affinity >::my_tls_queue tbb_concurrent_queue_scheduler_base< Q, use_affinity >::m_localQueue;
        template< typename Q, bool use_affinity > tbb::concurrent_bounded_queue< schedulable * > * tbb_concurrent_queue_scheduler_base< Q, use_affinity >::m_gQueue = NULL;

    } // namespace Internal
} // namespace CnC
