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

#ifndef  _CnC_CONTEXT_BASE_H_
#define  _CnC_CONTEXT_BASE_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/distributable_context.h>
#include <cnc/internal/traceable.h>
#include <tbb/atomic.h>
#include <tbb/concurrent_queue.h>
#include <tbb/tbb_thread.h>
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/tbbcompat.h>

namespace CnC {
    namespace Internal {

        class scheduler_i;
        class step_instance_base;
        class item_collection_i;
        class chronometer;

        typedef tbb::atomic< int > Counter_t;

        /// The actualy core of a context.
        /// A context_base manages the dynamic instantiation of a runnable graph.
        /// Deals as as a proxy between collections and sub-graphs.
        /// It also holds the scheduler.
        class CNC_API context_base : public distributable_context
        {
        public:
            //static const int COUNTERS_NUMBER = 20;

            context_base( bool is_dummy = false );
            virtual ~context_base(); 
            // this is called right after the factory has created the object
            virtual void on_creation();

            step_instance_base * current_step_instance();
    
            //Counter_t * getPriorityCounterPtr( int index );

            void wait( scheduler_i * = NULL );

            void flush_gets();

            scheduler_i & scheduler() const { return *m_scheduler; }

            void block_for_put();
            void unblock_for_put();

            int incrementStepInstanceCount() { return ++m_stepInstanceCount; }
            /// from distributable_context
            virtual void spawn_cleanup();

			int numThreads() const { return m_numThreads; }

            /// \brief "hidden" graphs must report quiescence: register an active graph
            void leave_quiescence();

            /// \brief "hidden" graphs must report quiescence
            /// REports that a previously active graph is now in quiescent state
            void enter_quiescence();

        protected:
            scheduler_i * new_scheduler();
            void delete_scheduler( scheduler_i * );


        private:
            /// This is logically a queue of  bools, but popping the queue into
            /// a stack variable triggers an assertion check on Windows if the
            /// datatype is 1 byte.  (It checks if the surrounding bytes have been changed.)
            typedef tbb::concurrent_bounded_queue< int > blocking_queue;

            template< typename T >
            class PtrCompare
            {
            public:
                PtrCompare(){}
                inline bool equal( T * a, T * b ) const { return a == b; }
                inline size_t hash( T * a ) const { return ( reinterpret_cast< size_t >( a ) >> 6 ); }
            };

            void runInternal();

            chronometer       * m_timer;
            scheduler_i       * m_scheduler;
            blocking_queue      m_envIsWaiting;
            tbb::tbb_thread   * m_gcThread;
            int                 m_numThreads;
            tbb::atomic< int >  m_stepInstanceCount;

            friend std::ostream & operator<<( std::ostream &, const context_base & );
            template< typename T, typename item_type, typename Tuner > friend class item_collection_base;
        }; // class context_base

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline void context_base::block_for_put()
        {
            int tmp;
            m_envIsWaiting.pop( tmp );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%ß

        inline void context_base::unblock_for_put()
        {
            const int _tmp(1);
            // FIXME TBB4.3 seg-faults if we call put(1) directly, need a temporary
            m_envIsWaiting.push( _tmp ); 
        }
        
    } // namespace Internal
} // end namespace CnC
#endif // _CnC_CONTEXT_BASE_H_
