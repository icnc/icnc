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

#ifndef  _CnC_SCHEDULER_H_
#define  _CnC_SCHEDULER_H_

#include <atomic>
#include <thread>
#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/distributable.h>
#include <cnc/internal/scalable_vector.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_queue.h>
#include <tbb/spin_mutex.h>
#include <cnc/internal/tls.h>

namespace CnC {
    namespace Internal {

        class schedulable;
        class context_base;

		/// This class represents the functionality for a scheduler.
        /// Concrete schedulers derive from this and implement virtual functions.
        ///
        /// \todo We should probably break this up into 2 interfaces: one for accessing
        /// services and one to implement them.
        ///
		/// Potential queues, locking, threading, etc. must all be handled internally in those.
		/// The interface is expected to be thread-safe, e.g. multiple threads can call at concurrently, no
		/// matter where they were spawned.
		/// Steps to be executed come in through do_schedule. To execute a schedulable, the scheduler must call
		/// si->scheduler().do_execute( si );
		/// Exception handling, resuming steps and all this is handled by the base scheduler.
		/// When a context is created, it also creates a scheduler. Anything needed for successful operation
		/// needs to be done in the constructor of the conrete class. No other init/config call will ever be called.
		/// Until the interfaces have been cleaned up, don't use anything from schedulable other than priority() and scheduler().
        ///
        /// Waiting for completetion works as follows.
        /// Shared memory wating simply calls actual wait() implementation.
        /// For distCnC the waiting is split into 3 parts: 
        /// 1. sending a wait request
        /// 2. waiting
        /// 3. "barrier" that waits for procs to complete waiting
        /// The process which issues the wait_loop doesn't continue until all
        /// processes are done.  We also count the messages that are sent
        /// within the loop. If messages other than the sync mesasges have been
        /// sent in the meanwhile, the loop is repeated until no other messages
        /// are sent within the loop.  The receiver thread is not allowed to
        /// block, otherwise it can prohibit progress as no other mesages can
        /// be received in the meanwhile. Hence, on non root processes (those
        /// which created the context through the creator/factory) have a
        /// dedicated thread that does waiting.
        class CNC_API scheduler_i : public distributable, CnC::Internal::no_copy
        {
        public:
			enum { AFFINITY_HERE = -1 };

            /// opaque class to identify a group of steps which are waiting for a shared item.
            class suspend_group;

            scheduler_i( context_base & );
            virtual ~scheduler_i();

            /// Call this to init capabilites for distCnC after creation
            void start_dist();
            /// Call this to fini capabilites for distCnC before destructing scheduler
            void stop_dist();


            /// Calls schedulable->prepare and schedules the step and/or sends it to remote processes as appropriate.
            /// Ensures that scheduler::current() returns the instance in preparation while executing schedulable->prepare;
            /// If schedulable->prepare returns true, schedule schedules the step (if requested).
            /// Internally, the parameter do_schedule is used to bypass extra scheduling overhead when unsplittable range is executed.
            /// Doesn't apply to compute_on if target == NO_COMPUTE_ON.
            /// returns whether the step was scheduled (you better delete it if not)
            bool prepare( schedulable *, bool compute_on = true, bool do_schedule = true, bool is_service_task = false );

            /// \brief put current step instance on hold to be resumed at a possibly later point in time
            ///
            /// A number of step instances can be grouped together to be resumed together (see param group)
            /// The scheduler might decide to re-schedule it right away, though.
            /// In any case, for every created group, a call to resume is required.
            /// \param group if NULL, create new group; otherwise add given step to given group
            /// \return the group given step was added to
            static suspend_group * suspend( suspend_group * group );

            /// \brief resume group of instances identified by group id
            ///
            /// For every group which is created by suspend resume must be called once.
            /// The suspend group is deleted and must no longer be used after this call.
            /// \param group identifies step group, will be set to NULL
            /// \return true if suspend_group contained root
            static bool resume( suspend_group *& group );

            /// \brief return false if no more user-steps in queues
            bool active()
            { return m_userStepsInFlight > 0; }

            /// \brief return step instance the calling thread is running; might be NULL but only if root/env
            static schedulable * current();

            /// \brief "hidden" graphs must report quiescence: register an active graph
            void leave_quiescence()
            { ++m_activeGraphs; }

            /// \brief "hidden" graphs must report quiescence: tell a graph is now in quiescent state
            void enter_quiescence()
            { CNC_ASSERT( m_activeGraphs > 0 ); --m_activeGraphs; }


            /// from distributable
            virtual void recv_msg( serializer * );

            virtual void unsafe_reset( bool );

            /// wait loops around schedulers wait method
            /// call this from outside
            /// \param from_schedulable set to true if called from within a step/schedulable
            void wait_loop( bool from_schedulable = false );

            /// register a schedulable as pending for getting re-scheduled in wait()
            /// use this only on ranges and alike; it will only slow down normal steps with no other effect
            void pending( schedulable * );
            // register a schedulable for sequentialized execution on the wait-thread
            void sequentialize( schedulable * );

            /// \return target process id for given return value of tuner::compute_on
            static int get_target_pid( int pot );

			/// counter type for counting steps in flight
			typedef std::atomic< unsigned int > inflight_counter_type;

        protected:
            /// blocks until all scheduled step instances have been fully executed
            /// to be implemented by scheduler implementation (derived classes)
			/// \param steps_in_flight number of steps in flight
			///        is volatile, might change while executing wait()
			///        never falls below 1 while in wait()
            virtual void wait( const inflight_counter_type & steps_in_flight ) = 0;

            /// loops until wait() returned and no more steps are pending
            void wait_all();

            /// request wait on remote processes
            /// This is not part of the interface, but a helper functionality
            /// \param send will initiate remote wait if send == true
            void init_wait( bool send );

            /// waits for all processes to complete waiting. Must be preceded by init_wait();
            /// This is not part of the interface, but a helper functionality
            /// \return true, if another wait-loop is needed
            ///         happens only in distributed env situations on remote procs.
            bool fini_wait();

            ///
            static void set_current( schedulable * s );

            /// schedules a step instance for execution at some point
			/// to execute the step, call si->scheduler().do_execute( si );
            virtual void do_schedule( schedulable * si ) = 0;
			
            /// create a task to wait for completion of all submitted tasks (quiesence)
            /// through calling scheduler->wait_loop( true );
            /// by default a tbb-task is enqueued.
            /// it is recommended to use a service_task for this.
            virtual void enqueue_waiter();
            
        public:
            /// executes a schedulable, deleting it after completion
            /// only to be used by scheduler implementations to launch a task.
            void do_execute( schedulable * s );

        private:
            /// schedule given step instance to be launched at any time 
            void schedule( schedulable *, bool countit = true, schedulable * currStepInstance = NULL );

            typedef tbb::spin_mutex mutex_t;

        protected:
            context_base                         & m_context;
            tbb::concurrent_bounded_queue< int > * m_barrier;      ///< simluates a conditional variable/semaphore
        private:
            const schedulable                    * m_step;         ///< step which created scheduler
            typedef scalable_vector_p< schedulable * > pending_list_type;
            pending_list_type                      m_pendingSteps; ///< steps which need a ping in wait_loop()
            pending_list_type                      m_seqSteps;     ///< steps which need sequentialized execution
            mutex_t                                m_mutex;        ///< for pendingSteps
        protected:
            int                                    m_root;         ///< root process requesting wait
        private:
            std::atomic< unsigned int >            m_userStepsInFlight;
            std::atomic< unsigned int >            m_activeGraphs;
            const bool                             m_bypass;
            static TLS_static< schedulable * >     m_TLSCurrent;

            friend struct re_schedule;
            friend struct loop_wait;
        };

    } // namespace Internal
} // end namespace CnC

#endif // _CnC_SCHEDULER_H_
