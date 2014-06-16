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

#ifndef STEP_INSTANCE_H_ALREADY_INCLUDED
#define STEP_INSTANCE_H_ALREADY_INCLUDED

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/typed_tag.h>
#include <cnc/internal/step_delayer.h>
#include <cnc/internal/tagged_step_instance.h>
#include <cnc/internal/step_launcher.h>
#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    namespace Internal {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// The actual step instance, with alll its depndencies, coming from
        /// its collection, launcher and tuner.
        template< class StepLauncher >
        class step_instance : public tagged_step_instance< typename StepLauncher::tag_type >
        {
        public:
            typedef typename StepLauncher::tag_type tag_type;
            typedef typename StepLauncher::range_type range_type;
            typedef typename StepLauncher::arg_type arg_type;
            typedef typename StepLauncher::step_tuner_type step_tuner_type;

            step_instance( const tag_type & tag, context_base & ctxt, const StepLauncher * step_launcher );
            virtual int affinity() const;
            virtual CnC::Internal::StepReturnValue_t execute();
            virtual char prepare( step_delayer &, int &, const schedulable * );
            virtual void compute_on( int target );
            virtual const step_launcher_base< tag_type, range_type > * get_step_launcher() const { return m_stepLauncher; }

            bool prepare_from_range( tagged_step_instance< range_type > * rsi, const tag_type & tag, step_delayer & sD ) const;
            CnC::Internal::StepReturnValue_t execute_from_range( step_instance_base * rsi, const tag_type & tag ) const;
        
        private:
            CnC::Internal::StepReturnValue_t do_execute();
            const StepLauncher * m_stepLauncher;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        step_instance< StepLauncher >::step_instance( const tag_type & tag, context_base & ctxt, const StepLauncher * sl )
            : tagged_step_instance< tag_type >( tag, ctxt, sl->get_step_tuner().priority( tag, sl->m_arg ), sl->m_scheduler ),
              m_stepLauncher( sl )
        {
            if( m_stepLauncher->timing() ) this->enable_timing();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // template< class StepLauncher >
        // const std::string & step_instance< StepLauncher >::timing_name() const
        // {
        //     return step_timer< typename StepLauncher::step_type >::s_timingName;
        // }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
        template< class StepLauncher >
		int step_instance< StepLauncher >::affinity() const
		{
				return m_stepLauncher->get_step_tuner().affinity( this->m_tag.Value(), m_stepLauncher->m_arg );
		}
		
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        CnC::Internal::StepReturnValue_t step_instance< StepLauncher >::do_execute()
        {
            const tag_type & _tag = this->m_tag.Value();
            if( ! m_stepLauncher->get_step_tuner().was_canceled( _tag, m_stepLauncher->m_arg ) ) {
                return tagged_step_instance< tag_type >::do_execute( this,
                                                                     m_stepLauncher->get_step(),
                                                                     _tag,
                                                                     m_stepLauncher->m_arg,
                                                                     m_stepLauncher->context(),
                                                                     false,
                                                                     m_stepLauncher->timing(),
                                                                     m_stepLauncher->m_stepColl.trace_level() > 0,
                                                                     m_stepLauncher->m_stepColl.name(),
                                                                     m_stepLauncher->itacid() );
            } else if( m_stepLauncher->m_stepColl.trace_level() > 0 ) {
                Speaker oss;
                oss << "Canceled step " << m_stepLauncher->m_stepColl.name() << "(";
                cnc_format( oss, _tag )  << ")";
            }
            return CNC_Success;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        CnC::Internal::StepReturnValue_t step_instance< StepLauncher >::execute()
        {
            return do_execute();
        }

        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        CnC::Internal::StepReturnValue_t step_instance< StepLauncher >::execute_from_range( step_instance_base * rsi, const tag_type & tag ) const
        {
            if( ! m_stepLauncher->get_step_tuner().was_canceled( tag, m_stepLauncher->m_arg ) ) {
                return tagged_step_instance< tag_type >::do_execute( rsi,
                                                                     m_stepLauncher->get_step(),
                                                                     tag,
                                                                     m_stepLauncher->m_arg,
                                                                     m_stepLauncher->context(),
                                                                     true,
                                                                     m_stepLauncher->timing(),
                                                                     m_stepLauncher->m_stepColl.trace_level() > 0,
                                                                     m_stepLauncher->m_stepColl.name(),
                                                                     m_stepLauncher->itacid() );
            } else if( m_stepLauncher->m_stepColl.trace_level() > 0 ) {
                        Speaker oss;
                        oss << "Canceled step " << m_stepLauncher->m_stepColl.name() << "(";
                        cnc_format( oss, tag ) << ")";
            }
            return CNC_Success;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        char step_instance< StepLauncher >::prepare( step_delayer & sD, int & passOnTo, const schedulable * parent )
        {
            // no other thread can know about this step before this method has started
            CNC_ASSERT( ! this->prepared() );
            // let's make sure we have an additional dependency, so no concurrent put will accidently schedule this step
            this->suspend();
            this->reset_was_suspended();
            CNC_ASSERT( this->m_nSuspenders == 1 );

            const step_tuner_type & _tuner = m_stepLauncher->get_step_tuner();
            arg_type              & _arg   = m_stepLauncher->m_arg;
            const tag_type        & _tag   = this->m_tag.Value();

            if( ! this->prepare_compute_on( _tag, passOnTo, _arg, _tuner ) ) {
                this->unsuspend();
                return false;
            }

            bool _seq = _tuner.sequentialize( _tag, _arg );

            if( !_seq && _tuner.preschedule() ) {
                // let's avoid an infinite loop if called from another preschedule
                if( parent != NULL && ! parent->prepared() ) this->suspend();
                // let's make sure we have an additional dependency,
                // so no concurrent put will accidently schedule this step
                // and no re-execution occurs if an item becomes available during pre-scheduling
                // why should we need 2 suspends?
                // this->suspend();
                StepReturnValue_t rv = do_execute();
                if( rv == CNC_Success ) {
                    // here the step was fully executed
                    // we should not be here if we use flush_gets in the step (it always throws an exception when in prepare)
                    // CNC_ASSERT( this->m_nSuspenders == 2 ); // our own 2
                    CNC_ASSERT( parent == NULL || parent->prepared() );
                    this->m_status = CNC_Done;
                    return false;
                } else {
                    CNC_ASSERT( this->was_suspended_since_reset() || rv == CNC_NeedsSequentialize );
                    if( parent != NULL && ! parent->prepared() ) this->unsuspend();
                }
            } else {
                _tuner.depends( _tag, _arg, sD );
            }
            this->m_status = CNC_Prepared;
            
            CNC_ASSERT( this->suspended() );
            return _seq ? CNC_NeedsSequentialize : ( this->unsuspend() ? CNC_Prepared : CNC_Unfinished );
            // this makes sure it gets scheduled if it is not queued with an unavailable item
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        bool step_instance< StepLauncher >::prepare_from_range( tagged_step_instance< range_type > * rsi, const tag_type & tag, step_delayer & sD ) const
        {
            // we don't need an additional dependency, the parent/range step is already guarded
            // no compute_on per individual step-instance
            
            const step_tuner_type & _tuner = m_stepLauncher->get_step_tuner();

            if( _tuner.preschedule() ) {
                if( this->execute_from_range( rsi, tag ) == CNC_Success ) {
                    return false;
                }
            } else {
                _tuner.depends( tag, m_stepLauncher->m_arg, sD );
            }

            CNC_ASSERT( rsi->suspended() );
            return true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class StepLauncher >
        void step_instance< StepLauncher >::compute_on( int target )
        {
            m_stepLauncher->compute_on( this->m_tag.Value(), target );
        }

    } //   namespace Internal
} // namespace CnC

#endif //STEP_INSTANCE_H_ALREADY_INCLUDED
