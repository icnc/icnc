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
  see simplest_scheduler.h
*/

#define _CRT_SECURE_NO_DEPRECATE

#include "simplest_scheduler.h"
#include <cnc/internal/dist/distributor.h>
#include "pinning.h"
#include <algorithm> // min/max

namespace CnC {
    namespace Internal {
        
        static std::atomic< bool > s_have_pinning_observer;

		namespace {
			static pinning_observer * s_po = NULL;
			struct poi {
				~poi() { delete s_po; }
			};
			static poi s_poi;
		}

        //        template< typename Step >
        class simplest_scheduler::TaskWrapper : public tbb::task
        {
        public: 
            tbb::task * execute()
            {
                CNC_ASSERT( m_schedulable );
                m_schedulable->scheduler().do_execute( m_schedulable );
                return NULL;
            }

            TaskWrapper( schedulable * s ) : 
                m_schedulable( s )
            {}

        private:
            schedulable * m_schedulable;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        simplest_scheduler::simplest_scheduler( context_base &context, int numThreads, int htstride ) 
            : scheduler_i( context ),
              m_status(),
              m_rootTask(),
              m_initTBB( std::max( 2, numThreads + ( distributor::myPid() == 0 ? 0 : 1 ) ) ),
              m_taskGroupContext( tbb::task_group_context::isolated, tbb::task_group_context::default_traits | tbb::task_group_context::concurrent_wait )
        {
            //            {Speaker oss; oss << std::max( 2, numThreads + ( distributor::myPid() == 0 ? 0 : 1 ) );}
            bool _tmp(false);
			if( htstride && s_have_pinning_observer.compare_exchange_strong( _tmp, true ) ) {
				s_po = new pinning_observer( htstride );
			}
            m_status = COMPLETED;
            m_rootTask = new( tbb::task::allocate_root( m_taskGroupContext ) ) tbb::empty_task;
            m_rootTask->set_ref_count( 1 );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        simplest_scheduler::~simplest_scheduler()
        {
            m_rootTask->decrement_ref_count();
            m_rootTask->destroy( *m_rootTask );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void simplest_scheduler::do_schedule( schedulable * stepInstance )
        {
            int _tmp(COMPLETED);
            m_status.compare_exchange_strong( _tmp, RUNNING );
            //            tbb_waiter * _waitTask = new( tbb::task::allocate_root() ) TaskWrapper( stepInstance );
            TaskWrapper * newTask = new( tbb::task::allocate_additional_child_of( *m_rootTask ) ) TaskWrapper( stepInstance );
            //tbb::task::enqueue( *newTask );
            tbb::task::spawn( *newTask );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void simplest_scheduler::wait( const inflight_counter_type & /*steps_in_flight*/ )
        {
            m_rootTask->wait_for_all();
            m_status = COMPLETED;
        }
    } // namespace Internal
} // namespace CnC
