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

#ifndef  _CnC_SCHEDULABLE_H_
#define  _CnC_SCHEDULABLE_H_

#include <string>
#include <cnc/internal/tbbcompat.h>
#include <tbb/atomic.h>
#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    namespace Internal {

        class step_delayer;
        class scheduler_i;

        typedef char StepReturnValue_t;
        const char CNC_Success = 0;            ///< The step execution finished with success
        const char CNC_Unfinished = 1;         ///< The step execution has not finished (unavailable item)
        const char CNC_Failure = 2;            ///< An error occured during step execution
        const char CNC_NeedsSequentialize = 3; ///< Steps needs sequentialized execution
        const char CNC_NeedsExtraWake = 4;     ///< Need to be re-scheduled at a later point (range steps)
        const char CNC_Unprepared = 13;        ///< Step is still unprepared
        const char CNC_Prepared = 14;          ///< Step was prepared
        const char CNC_Pending = 15;           ///< Step is pending (waiting for input)
        const char CNC_FromPending = 16;       ///< Step was launched from pending state
        const char CNC_Done = 22;              ///< Step done

        /// \brief Objects which can be scheduled by the scheduler.
        ///
        /// Whenever a schedulable is put on a waiting/suspend-queue/group, it must be notified through calling suspend().
        /// To re-schedule it after removing it from a queue, the call to unsuspend() must return true.
        /// Otherwise it is still waiting for something else and must not be scheduled.
        /// suspend/unsuspend can be used  to protect a schedulable from being scheduled.
        /// reset_was_suspended() and was_suspended_since_reset() can be used in a single thread to observe whether the 
        /// schedulable was suspended between the 2 calls. This is needed when calls to unsuspend equals the number of calls to suspend
        /// but the schedulable was protected by a an extra call to suspend. Otherwise we'd lose a re-schedule.
        class schedulable
        {
        public:
            /// constructor with priority
            schedulable( int p, scheduler_i & sched );

            virtual ~schedulable(){}

            /// return priority
            inline int priority() const;
            /// return thread affinity
            virtual int affinity() const = 0;

            /// suspend from execution;
            /// several calls to suspend should be followed by the same number of calls to unsuspend
            void suspend();
            /// return true if it is still suspended
            bool suspended() const;
            /// return the number of pending suspend calls
            int num_suspends() const;
            /// unsuspends once and returns true if at least as many calls to suspend have been followed by unsuspend
            bool unsuspend();
            /// an extra flag observes if instance was suspended since a reset of the flag
            /// make sure "was_suspended_since_reset  and "reset_was_suspended" is called from a single thread!
            bool was_suspended_since_reset() const;
            // reset the observing suspended flag
            void reset_was_suspended();
            /// returns true if schedulable has been prepared
            /// not thread safe! To be used inside prepare and/or execute only!
            inline bool prepared() const; 
            /// returns true if schedulable has been entirely processed
            /// not thread safe!
            inline bool done() const;
            /// returns true if schedulable was re-launched from pending queue (in wait())
            // not thread safe!
            inline bool from_pending() const;
            /// returns true if schedulable is pending
            // not thread safe!
            inline bool is_pending() const;
            /// returns true if step needs serial execution
            inline bool is_sequentialized() const;
            /// returns the scheduler it is assigned to
            scheduler_i & scheduler() const;

            virtual StepReturnValue_t execute() = 0;
            virtual char prepare( step_delayer &, int &, const schedulable * ) = 0;
            virtual void compute_on( int target ) = 0;

            // we need this for proper error messages
            virtual std::ostream & format( std::ostream & os ) const
            { return os << "schedulable"; }

            /// for debug output, memorize step had a put
            inline void setHadPut();
            inline void resetHadPut();
            inline void assumeNoPutBeforeGet( const char * msg ) const;
                

        protected:
            schedulable       * m_succStep;
            scheduler_i       & m_scheduler;
            const int           m_priority;
            tbb::atomic< int >  m_nSuspenders;
            tbb::atomic< bool > m_wasSuspendedSinceReset;  // should be set to false, when execution starts
            //            bool                m_time;
            char                m_status;
            bool                m_inPending;
            bool                m_sequentialize;
            bool                m_hadPut;
            template< class T > friend class tagged_step_instance;
            friend class scheduler_i; // FIXME
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline schedulable::schedulable( int p, scheduler_i & sched )
            : m_succStep( NULL ),
              m_scheduler( sched ),
              m_priority( p ),
              m_nSuspenders(),
              m_wasSuspendedSinceReset(),
              //              m_time( false ),
              m_status( CNC_Unprepared ),
              m_inPending( false ),
              m_sequentialize( false ),
              m_hadPut( false )
              
        {
            m_nSuspenders = 0;
            m_wasSuspendedSinceReset = false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline int schedulable::priority() const
        {
            return m_priority;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline void schedulable::suspend() 
        {
            m_nSuspenders.fetch_and_increment();
            m_wasSuspendedSinceReset.compare_and_swap( true, false );
            CNC_ASSERT( m_wasSuspendedSinceReset );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::suspended() const
        {
            return m_nSuspenders > 0;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline int schedulable::num_suspends()  const
        {
            return m_nSuspenders;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::unsuspend() 
        {
            CNC_ASSERT( m_nSuspenders >= 1 );
            return m_nSuspenders.fetch_and_decrement() == 1;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool inline schedulable::was_suspended_since_reset() const
        {
            return m_wasSuspendedSinceReset;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void inline schedulable::reset_was_suspended()
        {
            m_wasSuspendedSinceReset = false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::prepared() const 
        {
            return m_status != CNC_Unprepared;
        } // not thread safe!

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::done() const 
        {
            return m_status == CNC_Done;
        } // not thread safe! To be used inside prepare and/or execute only!

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::from_pending() const 
        {
            return m_status == CNC_FromPending;
        } // not thread safe! To be used inside prepare and/or execute only!

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::is_pending() const 
        {
            return m_status == CNC_Pending; //m_pending;
        } // not thread safe! To be used inside prepare and/or execute only!

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline bool schedulable::is_sequentialized() const 
        {
            return m_sequentialize;
        } // not thread safe! To be used inside prepare and/or execute only!

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline scheduler_i & schedulable::scheduler() const 
        {
            return m_scheduler;
        }
            
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        /// for debug output, memorize step had a put
        inline void schedulable::setHadPut()
        {
            m_hadPut = true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline void schedulable::resetHadPut()
        {
            m_hadPut = false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline void schedulable::assumeNoPutBeforeGet( const char * msg ) const
        {
            if( m_hadPut ) {
                Speaker oss( std::cerr );
                oss << "Warning: ";
                format( oss ) << msg;
            }
        }

    } // namespace Internal
} // end namespace CnC

#include <functional>

namespace std {
    template<>
    struct less< CnC::Internal::schedulable * >
    {
    public:
        inline bool operator()( const CnC::Internal::schedulable * const a, const CnC::Internal::schedulable * const b ) const
        {
            return a->priority() < b->priority();
        }
    };
}

#endif // _CnC_SCHEDULABLE_H_
