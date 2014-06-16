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
  see scheduler_i.h
*/

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/schedulable.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/step_delayer.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/suspend_group.h>
#include <cnc/internal/statistics.h>
#include <cnc/serializer.h>
#include <tbb/task.h>
#include <tbb/compat/thread>
#include <tbb/enumerable_thread_specific.h>
#include <cnc/default_tuner.h>
#include <iostream>


namespace CnC {
    namespace Internal {

        struct tbb_waiter : public tbb::task
        {
            tbb_waiter( scheduler_i * s ) : tbb::task(), m_sched( s ) {}
            tbb::task * execute();
        private:
            scheduler_i * m_sched;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        const int EXIT      = 50;
        const int SELF_WAIT = 51;
        const int REQ_WAIT  = 51;
        

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        TLS_static< schedulable * > scheduler_i::m_TLSCurrent;
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        scheduler_i::scheduler_i( context_base & c )
            : distributable( "schedulerNN" ),
              m_context( c ),
              m_barrier( NULL ),
              m_step( current() ),
              m_pendingSteps(),
              m_seqSteps(),
              m_mutex(),
              m_root( distributor::distributed_env() ? m_root : distributor::myPid() ),
              m_userStepsInFlight(),
              m_activeGraphs(),
              m_bypass( getenv( "CNC_SCHEDULER_BYPASS" ) ?  (atoi( getenv( "CNC_SCHEDULER_BYPASS" ) )!=0) : false )
        {
            set_current( NULL );
            m_userStepsInFlight = 0;
            m_activeGraphs = 0;
            if( 0 == distributor::myPid() || distributor::distributed_env() ) {
                m_barrier = new tbb::concurrent_bounded_queue< int >;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void scheduler_i::start_dist()
        {
            m_root = -1;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void scheduler_i::stop_dist()
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 
        scheduler_i::~scheduler_i()
        {
            m_context.print_scheduler_statistics();
            delete m_barrier;
            set_current( const_cast< schedulable * >( m_step ) );
        }
                
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

		bool scheduler_i::prepare( schedulable * stepInstance, bool compute_on, bool do_schedule, bool is_service_task )
        {
            // we must fake that we currently really execute the new step
            step_delayer sD;
            schedulable * _tmp = current();
            set_current( stepInstance );
            int passOnTo = ( compute_on && m_context.distributed() ) ? distributor::myPid() : NO_COMPUTE_ON;

            // let's avoid infinite recursive calls to prepare if preschedule is enabled
			char _to_be_scheduled = stepInstance->prepare( sD, passOnTo, _tmp );
            set_current( _tmp );
            
            // is this to be passed on to another process?
            if( passOnTo != NO_COMPUTE_ON && passOnTo != distributor::myPid() ) {
                CNC_ASSERT( passOnTo < distributor::numProcs() && passOnTo != COMPUTE_ON_LOCAL );
                stepInstance->compute_on( passOnTo );
                if ( passOnTo != COMPUTE_ON_ALL ) {
                    return false;
                }
            }
            if( do_schedule && _to_be_scheduled == CNC_NeedsSequentialize ) {
                sequentialize( stepInstance );
            } else if( do_schedule && ( _to_be_scheduled == CNC_Prepared || passOnTo == COMPUTE_ON_ALL ) )  {
                schedule( stepInstance, true, _tmp );
            } else if( stepInstance->done() ) {
                return false;
            }
            if (passOnTo != COMPUTE_ON_ALL) {
                passOnTo = COMPUTE_ON_LOCAL;
            }
            return true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        scheduler_i::suspend_group * scheduler_i::suspend( scheduler_i::suspend_group * group )
        {
            schedulable * stepInstance = current();

            if( stepInstance ) {
                stepInstance->suspend();
                //                LOG_STATS( stepInstance->scheduler().m_context.stats(), step_suspended() );
            }
            if( group == NULL ) return new suspend_group( stepInstance );
            group->add( stepInstance );
            return group;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        struct re_schedule
        {
            typedef bool argument_type; // std::pair< context_base &, bool > argument_type;
            schedulable * m_currStepI;
            re_schedule( schedulable * s ) : m_currStepI( s ) {}
            re_schedule( const re_schedule & r ) : m_currStepI( r.m_currStepI ) {}

            void operator()( schedulable * stepInstance, argument_type * arg ) const
            {
                if( stepInstance != NULL ) {
                    if( stepInstance->unsuspend() ) {
                        stepInstance->scheduler().schedule( stepInstance, true, m_currStepI );
                    }
                    LOG_STATS( stepInstance->scheduler().m_context.stats(), step_resumed() );
                } else {
                    *arg = true;
                }
            }
        };

        bool scheduler_i::resume( scheduler_i::suspend_group *& group )
        {
            re_schedule _rs( current() );
            re_schedule::argument_type _arg = false; // _arg.first = m_context; _arg.second = false; // icpc is a little picky about temporaries
            group->for_all( _rs, &_arg );
            delete group;
            group = NULL;
            return _arg;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        schedulable * scheduler_i::current()
        {
            return m_TLSCurrent.get();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        /// TBB binds a task to a thread, and we bind a step to a TBB task, 
        /// so we can use thread local storage within a step.  In particular, 
        /// we keep track of the current StepInstance.
        void scheduler_i::set_current( schedulable * s )
        {
            m_TLSCurrent.set( s );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// register a schedulable as pending for getting re-scheduled in wait()
        /// use this only on ranges and alike; it will only slow down normal steps with no other effect
        void scheduler_i::pending( schedulable * s )
        {
            mutex_t::scoped_lock _l( m_mutex ); 
            s->m_status = CNC_Pending;
            if( m_pendingSteps.capacity() == 0 ) m_pendingSteps.reserve(8);
            m_pendingSteps.push_back( s );
            s->m_inPending = true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// register a schedulable for sequentialized execution on the wait-thread
        void scheduler_i::sequentialize( schedulable * s )
        {
            mutex_t::scoped_lock _l( m_mutex ); 
            s->m_sequentialize = true;
            if( m_seqSteps.capacity() == 0 ) m_seqSteps.reserve(8);
            m_seqSteps.push_back( s );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void scheduler_i::schedule( schedulable * stepInstance, bool countit, schedulable * currStepInstance )
        {
            // if this comes from a range, one step of it might provide the last dependency
            // we are already running and will detect this and succeed. Must schedule it.
            if( stepInstance == currStepInstance ) return;
            CNC_ASSERT( countit == true );
            CNC_ASSERT( &stepInstance->scheduler() == this );
            if( currStepInstance && currStepInstance->is_sequentialized() ) {
                // normal schedule will cause worker threads to start working -> just queue for later schedule
                pending( stepInstance );
                return;
            }
            if( countit ) ++m_userStepsInFlight;
            // if the current thread is already executing a step instance
            // which has no successor, make the new step instance a
            // successor, otherwise call do_schedule()
            if( m_bypass == false || ! ( currStepInstance != NULL && currStepInstance->prepared() && &currStepInstance->scheduler() == this ) ) {
                do_schedule( stepInstance );
            } else {
                if( currStepInstance->m_succStep ) {
                    do_schedule( stepInstance );
                } else {
                    currStepInstance->m_succStep = stepInstance;
                }
            }
            LOG_STATS( m_context.stats(), step_scheduled() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// execute a schedulable object, making the appropriate calls to
        /// set_current() and deleting the object once the step has completed
        // increases the ref-count through suspend by one, execute methods must unsuspend!
        void scheduler_i::do_execute( schedulable * s )
        {
            do {
                set_current( s );
                if( s->from_pending() ) { //from_pending() ) {  
                    // no extra suspend needed: was done in wait
                    CNC_ASSERT( s->num_suspends() >= 1 );
                    s->m_status = CNC_Prepared;
                } else {
                    s->suspend(); // extra suspend to protect the step from being re-scheduled by a put from a different thread
                }
                s->reset_was_suspended(); // reset suspend count
                int _res = s->execute();
                schedulable * _newS = m_bypass == false ? NULL : s->m_succStep;
                if( _res == CNC_Success ) {
                    --m_userStepsInFlight;
                    if( ! s->m_inPending ) {
                        delete s;
                    }
                } else {
                    if( s->unsuspend() && !s->done() && _res != CNC_NeedsSequentialize ) {
                        // oops, all deps available now -> just re-execute
                        continue;
                    } else {
                        --m_userStepsInFlight;
                        if( m_bypass == false ) {
                        } else {
                            // range_is_tag can get here   CNC_ASSERT_MSG( _newS == NULL || s->is_pending() , "Put tag before (unsuccessful) get?" );
                            CNC_ASSERT( _newS == NULL || _newS->m_succStep == NULL || s == _newS );
                            s->m_succStep = NULL;
                            // Prevent the possibility of bypassing for the successor step, otherwise we might run into a cycle
                            if( _newS && _newS != s ) _newS->m_succStep = _newS;
                        }
                    }
                }
                if( m_bypass == false ) {
                    s = NULL;
                } else {
                    // if s == _newS s was a successor of an unsuccesfull step -> stop recursion
                    s = _newS == s ? NULL : _newS;
                    CNC_ASSERT( s == NULL || s->m_succStep == NULL || s->m_succStep == s );
                }
            } while( s != NULL );
            set_current( NULL );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// remote processes require a separate thread which waits for completion upon request (distCnC only).
        /// Otherwise the receiver thread would block which may lead to a dead-lock.
        tbb::task * tbb_waiter::execute()
        {
            increment_ref_count();
            m_sched->wait_loop();
            decrement_ref_count();
            return NULL;
        }

        void scheduler_i::enqueue_waiter()
        {
            tbb_waiter * _waitTask = new( tbb::task::allocate_root() ) tbb_waiter( this );
            tbb::task::enqueue( *_waitTask );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        const char PING = 33; /// request waiting for completion (sent from host to clients)
        const char PONG = 44; /// report completion back to host (from client)
        const char DONE = 55; /// report completion back to cliensts (from host)

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// "owning" process sends PING and its pid to all clones (distCnC only).
        /// on receiving the PING, clones send PONG back after calling wait()
        /// contexts might be owned by processes other than root/host process, e.g. if created in steps
        /// however, the real wait() can only be issued by the owner/creator (currently only the host)
        void scheduler_i::init_wait( bool send )
        {
            CNC_ASSERT( subscribed() && distributor::active() && m_context.distributed() );

            // remote procs do nothing here, caller of calling wait() must be recv_msg
            if( send ) {
                serializer * _ser = m_context.new_serializer( this );
                //            m_root = distributor::myPid();
                CNC_ASSERT( m_root == 0 ); // FIXME wait on clients not supported
                (*_ser) & PING & m_root;
                m_context.bcast_msg( _ser );
            }
        }


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// The actual wait procedure calls wait() of the actual scheduler (child class).
        /// it loops over the wait and scheduling pending steps until the number pending steps no longer decreases.
        void scheduler_i::wait_all( )
        {
            int _curr_pend = 1;
            pending_list_type _seq2;

            // FIXME is there a safe and cheap way to determine if we are making progress?
            //       maybe through context and stats
            //       for now we loop at most 99999 times
            for( int i = 0; i < 99999 && _curr_pend > 0; ++i ) {

                do {
                    wait( m_userStepsInFlight ); // first wait for scheduler (executing steps)
                } while( m_activeGraphs > 0 ); // loop until all hidden graphs entered quiescent state

                // at this point we can assume that no worker is running fs: really? what about distcnc?
                // let's first "lock" the schedulables by an additional call to suspend. No item-put can then trigger execution of a pending step
                for( pending_list_type::iterator i = m_pendingSteps.begin(); i != m_pendingSteps.end(); ++i ) { 
                    if( ! (*i)->done() ) {
                        CNC_ASSERT( (*i)->is_pending() );
                        (*i)->suspend();
                    }
                }
                for( pending_list_type::iterator i = m_seqSteps.begin(); i != m_seqSteps.end(); ++i ) { 
                    if( ! (*i)->done() ) {
                        CNC_ASSERT( (*i)->is_sequentialized() );
                        (*i)->suspend();
                    }
                }
                // Now start sequentialized steps - before scheduling potentially parallel steps.
                // We know it's all sequentialized since also schedule() will set a step to pending if it's scheduled
                // serial execution might requeue pending/serialized steps -> copy m_pendigSteps to avoid endless loop
                if( ! m_seqSteps.empty() ) {
                    _seq2.swap( m_seqSteps );
                    // loop and execute one after the other
                    do {
                        schedulable *_s = _seq2.back();
                        _seq2.pop_back();
                        CNC_ASSERT( _s->is_sequentialized() );
                        ++m_userStepsInFlight;
                        do_execute( _s );
                    } while( ! _seq2.empty() );
                }

                _curr_pend = m_pendingSteps.size() + m_seqSteps.size();
                // even though no thread is running here, the loop can cause threads to start working
                // -> we need a lock
                mutex_t::scoped_lock _l( m_mutex );
                
                while( ! m_pendingSteps.empty() ) {
                    schedulable *_s = m_pendingSteps.back();
                    m_pendingSteps.pop_back();
                    if( ! _s->done() ) {
                        CNC_ASSERT( ! _s->is_sequentialized() );
                        _s->m_status = CNC_FromPending;
                        _s->m_inPending = false;
                        schedule( _s );
                        LOG_STATS( m_context.stats(), step_resumed() );
                    } else {
                        delete _s;
                    }
                }
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static bool _yield() { tbb::this_tbb_thread::sleep(tbb::tick_count::interval_t(0.0002)); return true; }

        /// Calls wait_all() which delegates it to wait() of the actual scheduler (child class).
        /// Distributed systems require communication flushing and barriers. See \ref scheduler_i::init_wait for details.
        void scheduler_i::wait_loop( bool from_schedulable )
        {
            // if called from schedulable, this taks/step will not complete herein
            //   -> don't increment/decrement counter
            if( ! from_schedulable ) ++m_userStepsInFlight;
            bool send = m_root == distributor::myPid() && !distributor::distributed_env(); // if dist_env, the remote processes also call wait explicitly!
            if( subscribed() && m_context.distributed() ) {
                int _nProcs = distributor::numProcs();
                do {
                    init_wait( send );
                    do {
                        wait_all();
                    } while( distributor::has_pending_messages() && _yield() );
                    send = m_root == distributor::myPid();
                } while( fini_wait()
                         || ( m_root == distributor::myPid() //FIXME root != 0 not supported: m_root reset in fini
                              && distributor::flush() > 2*_nProcs - 2 ) ); //3*_nProcs - 2 );
                              // >  5*_nProcs-3 ) ); if ++numMsgRecvd in distributor::new:serializer
                // we need the loop because potentially wait() missed the messages received between reset_recvd_msg_count and flush
                CNC_ASSERT( m_userStepsInFlight == 1 || m_root != distributor::myPid() );
                // root sends done flag in distributed env setup
                if( distributor::distributed_env() && m_root == distributor::myPid() ) {
                    serializer * _ser = m_context.new_serializer( this );
                    (*_ser) & DONE;
                    m_context.bcast_msg( _ser );
                }
            } else {
                wait_all();
                CNC_ASSERT( m_userStepsInFlight == 1 );
            }
            if( ! from_schedulable ) --m_userStepsInFlight;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// finalize wait of clones (distCnC only)
        bool scheduler_i::fini_wait()
        {
            CNC_ASSERT( subscribed() && distributor::active() && m_context.distributed() );

            if( m_root != distributor::myPid() ) { 
                // remote procs send pong to host
                CNC_ASSERT( m_root >= 0 );
                serializer * _ser = m_context.new_serializer( this );
                (*_ser) & PONG;
                m_context.send_msg( _ser, m_root );
                // host will send PING or DONE
                if( distributor::distributed_env() ) {
                    int _tmp;
                    m_barrier->pop( _tmp );
                    return _tmp == 0;
                }
                return false;
            } else {
                // local host wait for all pongs to arrive
                int n = distributor::numProcs() - 1;
                int _tmp;
                for( int i = 0; i < n; ++i ) m_barrier->pop( _tmp );
                return false;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// this receives ping or pong and reacts accordingly (distCnC only)
        void scheduler_i::recv_msg( serializer * ser )
        {
            CNC_ASSERT( subscribed() && distributor::active() && m_context.distributed() );

            char _pingpong;
            (*ser) & _pingpong;
            switch( _pingpong ) {
            case PING : {
                //                { Speaker oss; oss << "recvd PING"; }
                // remote procs are told to to wait
                (*ser) & m_root;
                CNC_ASSERT( m_root >= 0 );
                //                distributor::reset_recvd_msg_count();
                if( distributor::distributed_env() ) {
                    m_barrier->push( 0 );
                } else {
                    enqueue_waiter();
                }
                break;
            }
            case PONG : {
                //                { Speaker oss; oss << "recvd PONG"; }
                CNC_ASSERT( m_barrier );
                m_barrier->push( 1 ); // local host "registers" pong
                break;
            }
            default: {
                //                { Speaker oss; oss << "recvd DONE"; }
                CNC_ASSERT( _pingpong == DONE );
                CNC_ASSERT( distributor::distributed_env() );
                m_barrier->push( 1 );
                break;
            }
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline int round_robin_next( int numP )
        {
            int _n = distributor::numProcs();
            static tbb::enumerable_thread_specific< int > s_currP( distributor::myPid() );
            tbb::enumerable_thread_specific< int >::reference _i = s_currP.local();
            return ++_i % _n;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int scheduler_i::get_target_pid( int pot )
        {
            int _mypid = distributor::myPid();
            int _np = distributor::numProcs();
            if( pot != COMPUTE_ON_LOCAL && pot != _mypid && pot != COMPUTE_ON_ALL && pot != COMPUTE_ON_ALL_OTHERS ) {            
                if( pot < 0 || pot >= _np ) {
                    pot = round_robin_next( _np );
                }
            } else if( pot == COMPUTE_ON_LOCAL ) {
                pot = _mypid;
            }
            CNC_ASSERT_MSG( pot == COMPUTE_ON_ALL || pot == COMPUTE_ON_ALL_OTHERS || ( pot >= 0 && pot < _np ), 
                            "compute_on must return process id, CnC::COMPUTE_ON_LOCAL, CnC::COMPUTE_ON_ROUND_ROBIN, CnC::COMPUTE_ON_ALL_OTHERS  or CnC::COMPUTE_ON_ALL" );
            return pot;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void scheduler_i::unsafe_reset( bool )
        {
        }

    } // namespace Internal
} // end namespace CnC
