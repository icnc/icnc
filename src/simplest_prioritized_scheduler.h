/* *******************************************************************************
 *  Copyright (c) 2007-2021, Intel Corporation
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

#ifndef SIMPLEST_PRIORITIZED_SCHEDULER_H
#define SIMPLEST_PRIORITIZED_SCHEDULER_H

#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/schedulable.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/item_collection_i.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_priority_queue.h>
#include <atomic>
#include <tbb/task_scheduler_init.h>
#include <tbb/task.h>
#include <cnc/internal/statistics.h>

namespace CnC {
    namespace Internal {

        class context_base;

        /// \todo use TBB's priorities. Probably in simplest_scheduler, not here.
        class simplest_prioritized_scheduler: public scheduler_i
        {
        public:
            simplest_prioritized_scheduler( context_base &, int numThreads, int );
            virtual ~simplest_prioritized_scheduler();
            virtual void do_schedule( schedulable * stepInstance );
            virtual void wait( const inflight_counter_type & /*steps_in_flight*/ );
        private:
            enum { RUNNING, WAITING, COMPLETED };
            //typedef concurrent_priority_queue< class schedulable * > run_queue;
            typedef tbb::concurrent_priority_queue< schedulable * > run_queue;
            class apply_step_instance;
            template< typename Body > class TaskWrapper2;
            typedef TaskWrapper2< apply_step_instance > ApplyTask2;

            run_queue                  m_runQueue;
            std::atomic< int >         m_status;
            std::atomic< bool>         m_allocated;
            tbb::task_scheduler_init   m_initTBB ;
            apply_step_instance      * m_applyStepInstance;
            friend class apply_step_instance; //to call set_current
        };

    } // namespace Internal
} // namespace CnC


#endif
