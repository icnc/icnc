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
  see simplest_prioritized_scheduler.h
*/

#define _CRT_SECURE_NO_DEPRECATE

#include "simplest_prioritized_scheduler.h"
#include "pinning.h"
#include <cnc/internal/dist/distributor.h>

namespace CnC {
    namespace Internal {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
        static std::atomic< bool > s_have_pinning_observer;
		
		namespace {
			static pinning_observer * s_po = NULL;
			struct poi {
				~poi() { delete s_po; }
			};
			static poi s_poi;
		}

        template<typename Body>
        class simplest_prioritized_scheduler::TaskWrapper2 : public tbb::task
        {
            const Body& my_body;
            typename Body::argument_type my_value;
            TaskWrapper2< Body > * toBeReturned;

        public: 
            task* execute() {
                my_body(my_value); 
                return toBeReturned;
            };

            void setNext(TaskWrapper2<Body>* givenNext) { toBeReturned = givenNext; }

            TaskWrapper2( const typename Body::argument_type& value, const Body& body ) : 
                my_body(body), my_value(value), toBeReturned(NULL){}
        };

        class simplest_prioritized_scheduler::apply_step_instance
        {
        public:
            void operator()( schedulable * stepInstance ) const
            {
                CNC_ASSERT( stepInstance );
                stepInstance->scheduler().do_execute( stepInstance );
            }

            typedef schedulable * argument_type;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        simplest_prioritized_scheduler::simplest_prioritized_scheduler( context_base &context, int numThreads, int htstride ) 
            : scheduler_i( context ),
              m_runQueue(),
              m_status(),
              m_allocated(),
              m_initTBB( numThreads  + ( distributor::myPid() == 0 ? 0 : 1 ) ),
              m_applyStepInstance( NULL )
        {
			if( htstride && s_have_pinning_observer.compare_and_swap( true, false ) == false ) {
				s_po = new pinning_observer( htstride );
			}
            m_status = COMPLETED;
            m_allocated = false;
            if( m_allocated.compare_and_swap( true, false ) == false ) {
                m_applyStepInstance = new apply_step_instance();
            }

        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        simplest_prioritized_scheduler::~simplest_prioritized_scheduler()
        {
            if( m_allocated.compare_and_swap( false, true ) == true ) {
                delete m_applyStepInstance;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // bool simplest_prioritized_scheduler::is_active()
        // {
        //     return m_status != COMPLETED;
        // }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void simplest_prioritized_scheduler::do_schedule( schedulable * stepInstance )
        { 
            m_status.compare_and_swap(RUNNING, COMPLETED);
            m_runQueue.push(stepInstance); //, stepInstance->priority());
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void simplest_prioritized_scheduler::wait( const inflight_counter_type & /*steps_in_flight*/ )
        {
            m_status = WAITING;
            // sagnak
            // the chain structure of tasks guarantees that this approach would not scale
            // I believe no steal from a chain is possible, since there is no branching
            // compiles, works almost as fast, the others do not scale either, so
            // keeping this for the time being
            while(!m_runQueue.empty())
            {
                tbb::task_group_context ctx( tbb::task_group_context::isolated, tbb::task_group_context::default_traits | tbb::task_group_context::concurrent_wait );
                tbb::empty_task * m_rootTask = new( tbb::task::allocate_root( ctx ) ) tbb::empty_task;
                m_rootTask->set_ref_count( 1 );

                schedulable* rootExec;
                m_runQueue.try_pop(rootExec);
                ApplyTask2& rootApply = *new( tbb::task::allocate_additional_child_of( *m_rootTask ) ) ApplyTask2( rootExec, *m_applyStepInstance );

                ApplyTask2* currPrev = &rootApply;
                schedulable* next;
                while(!m_runQueue.empty())
                {
                    m_runQueue.try_pop(next);
                    ApplyTask2* newNextTask = new(tbb::task::allocate_additional_child_of(*m_rootTask)) ApplyTask2(next,*m_applyStepInstance );
                    currPrev->setNext(newNextTask);
                    currPrev = newNextTask;
                }

                tbb::task::self().spawn(rootApply);
                m_rootTask->wait_for_all();
                m_rootTask->set_ref_count( 0 );
                m_rootTask->destroy( *m_rootTask );
            }

            m_status = COMPLETED;
        }
    } // namespace Internal
} // namespace CnC

