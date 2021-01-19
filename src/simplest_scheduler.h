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

#ifndef SIMPLEST_SCHEDULER_H
#define SIMPLEST_SCHEDULER_H

#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/schedulable.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/item_collection_i.h>
#include <cnc/internal/tbbcompat.h>
#include <atomic>
#include <tbb/task_arena.h>
#include <tbb/task_group.h>
#include <cnc/internal/statistics.h>

namespace CnC {
    namespace Internal {

        class context_base;

        /// This scheduler is only simple because the complexity is hidden in TBB.
        /// It spawns a task for every schedulable that's scheduled.
        /// Each task is a member of task group. Every scheduler instance has its own task group.
        /// So we can wait for groups of tasks (as needed for parallel_for).
        /// See TBB docs for the scheduling policies of TBB.
        /// \todo use tbb::task_arena to avoid dead-locks in distCnC.
        ///           Without arenas several master threads cannot steal work from other masters,
        ///           distCnC has sender/receiver-threads which produce tasks. A low
        ///           number of threads can cause dead-locks.
        class simplest_scheduler: public scheduler_i
        {
        public:
            simplest_scheduler( context_base &, int numThreads, int );
            virtual ~simplest_scheduler() noexcept;
            virtual void do_schedule( schedulable * stepInstance );
            virtual void wait( const inflight_counter_type & /*steps_in_flight*/ );

        private:
            enum { RUNNING, WAITING, COMPLETED };
            
            std::atomic< int > m_status;
            tbb::task_arena    m_taskArena;
            tbb::task_group    m_taskGroup;
        };

    } // namespace Internal
} // namespace CnC


#endif
