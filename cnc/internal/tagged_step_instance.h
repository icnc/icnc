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


#ifndef TAGGED_STEP_INSTANCE_H_ALREADY_INCLUDED
#define TAGGED_STEP_INSTANCE_H_ALREADY_INCLUDED

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/step_instance_base.h>
#include <cnc/internal/typed_tag.h>
#include <cnc/internal/chronometer.h>
#include <cnc/internal/statistics.h>
#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/data_not_ready.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/itac.h>

#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    namespace Internal {

        /// The core of a concrete step-instance: we have the prescribing tag and its type available.
        /// Everything we can do with the tag only (no collection, tuner etc).
        /// Every step execution is done here.
        /// The final part of garbage_collection is done here: get_list::decrement
        /// \see CnC::Internal::get_list
        template< class Tag >
        class tagged_step_instance : public step_instance_base
        {
        public:
            tagged_step_instance( const Tag & tag, context_base & ctxt, int p, scheduler_i & sched );

            virtual const tag_base & tag() const;
            virtual const step_launcher_i * get_step_launcher() const = 0;
            
            template< class Arg, class Tuner >
            static bool prepare_compute_on( const Tag & tag, int & passOnTo, Arg & arg, const Tuner & tuner );
            // all step execution finally goes through here (except the shortcut for parallel_for which does not check dependencies)
            // child classes calls this static method in execute and execute_from_range
            template< class Step, class Arg >
            static StepReturnValue_t do_execute( step_instance_base * si,
                                                 const Step & step,
                                                 const Tag & tag,
                                                 Arg & arg,
                                                 context_base & context,
                                                 const bool fromRange,
                                                 const bool do_time,
                                                 const bool do_trace,
                                                 const std::string & traceName,
                                                 const int gid );
            
        protected:
            template< class Step, class Arg >
            static StepReturnValue_t execute_step( const Step & step,
                                                   const Tag & tag,
                                                   Arg & arg );

            typed_tag< Tag > m_tag;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag >
        tagged_step_instance< Tag >::tagged_step_instance( const Tag & tag, context_base & ctxt, int p, scheduler_i & sched )
            : step_instance_base( ctxt, p, sched ),
              m_tag( tag )
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag >
        const tag_base & tagged_step_instance< Tag >::tag() const
        {
            return m_tag;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag >
        template< class Arg, class Tuner >
        bool tagged_step_instance< Tag >::prepare_compute_on( const Tag & tag,
                                                           int & passOnTo,
                                                           Arg & arg,
                                                           const Tuner & tuner )
        {  
            if( passOnTo != NO_COMPUTE_ON ) {
                int _pot = scheduler_i::get_target_pid( tuner.compute_on( tag, arg ) );
                if( _pot != distributor::myPid() ) {
                    passOnTo = _pot;
                    return false;
                }
            }
            return true;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag >
        template< class Step, class Arg >
        StepReturnValue_t tagged_step_instance< Tag >::execute_step( const Step & step,
                                                                     const Tag & tag,
                                                                     Arg & arg )
        {
            return step.execute( tag, arg );
        }


        /// we protect this step instance from being scheduled until we are done by adding an extra call suspend.
        /// We have to re-schedule the step if we catch the exception, because it means an item was unavailable and the step has been suspended.
        /// We request counting the number of suspends during until we undo our extra suspend, because if done_getting is used,
        /// it must throw an exception if at least one item was unavailable. Looking at the suspend-status is unsufficient, since an item might
        /// might have been put in the meanwhile, but the get had still failed.
        template< class Tag >
        template< class Step, class Arg >
        StepReturnValue_t tagged_step_instance< Tag >::do_execute( step_instance_base * si,
                                                                   const Step & step,
                                                                   const Tag & tag,
                                                                   Arg & arg,
                                                                   context_base & context,
                                                                   const bool fromRange,
                                                                   const bool do_time,
                                                                   const bool do_trace,
                                                                   const std::string & traceName,
                                                                   const int gid ) 
        {
            if( fromRange ) {
                if( si == NULL ) si = context.current_step_instance();
            } else {
                if( do_time ) si->start_timing();
            }
#ifndef NDEBUG
            si->resetHadPut();
#endif
            if( do_trace ) {
                Speaker oss;
                oss << "Start step " << traceName << "(";
                cnc_format( oss, tag ) << ")";
            }
            
            StepReturnValue_t rv = CNC_Unfinished;
            LOG_STATS( context.stats(), step_launched() );

            {
#ifdef CNC_WITH_ITAC
                CNC_ASSERT( gid );
                ITAC_FUNC _itacfunc( gid );
#endif
                try {
                    rv = execute_step( step, tag, arg );
                }
                catch( data_not_ready /*dnr*/ ) {
                }
            }
            CNC_ASSERT( ( rv==CNC_Unfinished ) || ( rv==CNC_Failure ) || ( rv==CNC_Success ) || ( rv==CNC_NeedsExtraWake ) || ( rv==CNC_NeedsSequentialize )  );

            switch( rv ) {
                // FIXME: what is the purpose of these return codes?
            case CNC_Success :
                // Decrement count for items retreived in the step
                si->glist().decrement();
                si->glist().clear();
                if( ! fromRange ) {
                    si->m_status = CNC_Done;
                }
                if( do_trace ) {
                    Speaker oss;
                    oss << "End step " << traceName << "(";
                    cnc_format( oss, tag ) << ")";
                }
                break;

            case CNC_Unfinished :
            case CNC_NeedsExtraWake :
            case CNC_NeedsSequentialize :
                // We tried itemcollection->Get( tag ) and the item was not there.
                // or the step needs to be run sequentialized

                si->glist().clear();

                // We are using local queues, where step instances waiting for
                // data queue up on an tag.
                // We know the stepinstance has been queued/suspended to wait for data.
                CNC_ASSERT( fromRange || si->was_suspended_since_reset() || ! si->prepared() || rv == CNC_NeedsSequentialize );
                // Any attempt to resume it must have failed, so we need to re-schedule if our suspend is the last.


                if( do_trace ) {
                    Speaker oss;
                    oss << "Suspend step " << traceName << "(";
                    cnc_format( oss, tag ) << ")";
                    if( rv == CNC_NeedsExtraWake ) oss << " {needs extra wake}";
                    else if( rv == CNC_NeedsSequentialize ) oss << ": needs sequentialized execution";
                }

                if( ! fromRange ) {
                    LOG_STATS( context.stats(), step_suspended() );
                    //FS grmpf, FIXME
                    // I do this here so that not every scheduler needs to follow the protocol
                    // Note: range-step_instance::execute will from now on always return CNC_Unfinished to protect from being deleted
                    if( rv == CNC_NeedsExtraWake ) {
                        si->scheduler().pending( si );
                    } else if( rv == CNC_NeedsSequentialize || si->is_sequentialized() ) {
                        si->scheduler().sequentialize( si );
                    }
                } 
                    
                if( rv == CNC_NeedsExtraWake ) {
                    rv = CNC_Unfinished;
                } else if( rv != CNC_NeedsSequentialize && si->is_sequentialized() ) {
                    rv = CNC_NeedsSequentialize;
                }
                CNC_ASSERT( rv == CNC_Unfinished || rv == CNC_NeedsSequentialize );
                break;

            case CNC_Failure:
            default :
                // try again
                // why re try if the step failed?
                CNC_ABORT( "Step failed" );
                si->glist().clear();
                if( do_trace ) {
                    Speaker oss( std::cerr );
                    oss << "Should we Requeue step " << traceName << "(";
                    cnc_format( oss, tag ) << ")?  Execution failed!";
                }
                break;
                CNC_ABORT( "Unknown return value. Don't know what to do." );
            }

            LOG_STATS( context.stats(), step_terminated() );

            // we are done, this should not be needed:   unsuspend();
            
            if( do_time && ! fromRange ) si->stop_timing( rv, traceName );
            return rv;
        }
        
} //   namespace Internal
} // namespace CnC

#endif //TAGGED_STEP_INSTANCE_H_ALREADY_INCLUDED
