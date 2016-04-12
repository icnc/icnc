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
  see context_base.h
*/

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/context_base.h>
#include <cnc/internal/typed_tag.h>
#include <cnc/internal/chronometer.h>
#include <cnc/internal/step_instance_base.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/data_not_ready.h>
#include <cnc/internal/service_task.h>
#include "simplest_scheduler.h"
#include "tbb_concurrent_queue_scheduler.h"
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/tbbcompat.h>
#include "tbb/task_scheduler_init.h"

namespace CnC {
    namespace Internal {
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        context_base::context_base( bool is_dummy )
            : distributable_context( is_dummy ? "ContextDummy" : "ContextNN" ),
              m_timer( NULL ), 
              m_scheduler( NULL ),
              m_envIsWaiting(),
              m_stepInstanceCount()
        {
            if( getenv( "CNC_NUM_THREADS" ) ) {
                m_numThreads = atoi( getenv( "CNC_NUM_THREADS" ) );
                if( m_numThreads <= 0 ) m_numThreads = tbb::task_scheduler_init::default_num_threads();
            } else {
                m_numThreads = tbb::task_scheduler_init::default_num_threads();
            }
            // on remote processes we don't have the env-thread,
            // but we have a service thread which is triggered with wait() and work-waits
            m_scheduler = new_scheduler();
            if( ! is_dummy ) {
                subscribe( m_scheduler );
            }
            m_stepInstanceCount = 0;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// note this is called after the destructors for the ItemCollections, et al, 
        /// so there is very little we can do here.
        context_base::~context_base() 
        {
            delete_scheduler( m_scheduler );
            if ( m_timer )  {
                delete( m_timer );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        scheduler_i * context_base::new_scheduler()
        {
            scheduler_i *_ts = NULL;
            const char * _sched = getenv( "CNC_SCHEDULER" );
            const char * _pin = getenv( "CNC_PIN_THREADS" );
#ifdef __MIC__
			const int _htstride = _pin ? atoi( _pin ) : 4;
#else
			const int _htstride = _pin ? atoi( _pin ) : 0;
#endif
            const char * _prior = getenv( "CNC_USE_PRIORITY" );
			const bool _use_prior = _prior && atoi( _prior ) ? true : false;

			static bool _first = true;
			_prior = _first && _use_prior    ? " [PRIORITY ON]"  : " [PRIORITY OFF]";
			_pin   = _first && _htstride > 0 ? " [PINNING ON]" : " [PINNING OFF]";


			if( _sched ) {
                Speaker oss;
				if( ! strcmp( _sched, "FIFO_STEAL" ) ) {
					if( _first ) oss << "Using FIFO_STEAL scheduler" << _prior << _pin;
					if( _use_prior ) {
						_ts = new tbb_concurrent_queue_prioritized_scheduler( *this, m_numThreads, true, _htstride );
					} else  {
						_ts = new tbb_concurrent_queue_scheduler( *this, m_numThreads, true, _htstride );
					}
				} else if( ! strcmp( _sched, "FIFO_SINGLE" ) ) {
					if( _first ) oss << "Using FIFO_SINGLE scheduler" << _prior << _pin;
					if( _use_prior ) {
						_ts = new tbb_concurrent_queue_prioritized_scheduler( *this, m_numThreads, false, _htstride );
					} else  {
						_ts = new tbb_concurrent_queue_scheduler( *this, m_numThreads, false, _htstride );
					}
				} else if( ! strcmp( _sched, "FIFO_AFFINITY" ) ) {
					if( _first ) oss << "Using FIFO_AFFINITY scheduler" << _prior << _pin;
					if( _use_prior ) {
						_ts = new tbb_concurrent_queue_prioritized_affinity_scheduler( *this, m_numThreads, true, _htstride );
					} else {
						_ts = new tbb_concurrent_queue_affinity_scheduler( *this, m_numThreads, true, _htstride );
					}
				} 
			}
            if( _ts == NULL ) {
				if( _first ) {
					_prior = _first && _use_prior    ? " [PRIORITY UNSUPPORTED]": "";
					if( _sched ) {
                        Speaker oss;
                        if( strcmp( _sched, "TBB_TASK" ) ) oss << "Unsupported scheduler \"" << _sched << "\". Using default (TBB_TASK) scheduler" << _prior << _pin;
                        else oss << "Using TBB_TASK scheduler" << _prior << _pin;
                    }
                }
				_ts = new simplest_scheduler( *this, m_numThreads, _htstride );
            }
			if( _first ) {
				_first = false;
			}
			return _ts;
        }
        
        void context_base::on_creation()
        {
            // on_create is only called if the context was created for distCnC on a remote process
            // -> let's start the dist-services
            m_scheduler->start_dist();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void context_base::delete_scheduler( scheduler_i * s )
        {
            if( s ) {
                s->stop_dist();
                delete s;
            }            
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void context_base::wait( scheduler_i * sched )
        {
            CNC_ASSERT( !distributed() || distributor::distributed_env() || distributor::myPid() == 0 );
            if( sched == NULL ) sched = m_scheduler;
            sched->wait_loop();
            if( subscribed() && sched->subscribed() ) {
                if( distributor::myPid() == 0 ) cleanup_distributables( true );
                sched->wait_loop();
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        step_instance_base * context_base::current_step_instance()
        {
            // aarrgghh, the cast is the price for separating schedulable from step_instance
            return reinterpret_cast< step_instance_base * >( m_scheduler->current() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // all items might have become available before throwing the exception.
        // The scheduler must know whether it needs to re-schedule
        // or whether the step will be resumed by a put.
        void context_base::flush_gets()
        {
             step_instance_base * _stepInstance = this->current_step_instance();
             CNC_ASSERT( _stepInstance != NULL );
             if( _stepInstance == NULL ) return;
             if( _stepInstance->was_suspended_since_reset() ) {
                 throw( DATA_NOT_READY );
             }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        std::ostream & operator<<( std::ostream & output, const context_base & )
        {
            return output << "G";
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        namespace {
            struct cleaner
            {
                void execute( context_base * ctxt )
                {
                    // unblock waiting env thread
                    ctxt->unblock_for_put();
                    ctxt->cleanup_distributables( false );
                }
            };
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void context_base::spawn_cleanup()
        {
            CNC_ASSERT( m_envIsWaiting.size() == 0 );
            new service_task< cleaner, context_base* > ( *m_scheduler, this );
            // wait for service thread to start before returning
            block_for_put();
        }
            
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void context_base::leave_quiescence()
        {
            m_scheduler->leave_quiescence();
        }
        
        void context_base::enter_quiescence()
        {
            m_scheduler->enter_quiescence();
        }

    } // namespace Internal
} // end namespace CnC
