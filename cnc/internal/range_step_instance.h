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

#ifndef RANGE_STEP_INSTANCE_H_ALREADY_INCLUDED
#define RANGE_STEP_INSTANCE_H_ALREADY_INCLUDED

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/range_step.h>
#include <cnc/internal/typed_tag.h>
#include <cnc/internal/step_launcher.h>
#include <cnc/internal/scheduler_i.h>

#include <cnc/internal/tbbcompat.h>
#include <tbb/scalable_allocator.h>
#include <tbb/atomic.h>

#include <cnc/internal/scalable_vector.h>
#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    namespace Internal {

        /// A range_step_instance is a wrapper which can execute a bunch
        /// (set) of steps in one go.  Using ranges avoids extra scheduling
        /// and extra virtual function calls for each individual step
        /// execution (tag).  A range_step_instance is a "normal"
        /// tagged_step_instance, e.g. it is scheduled and invoked through its
        /// virtual execute method.  In execute, it launches the range_step (no
        /// virtual function call).
        /// \note like with step and step_instance, range_steps define the step-body,
        ///       and range_step_instance takes care of the exeuction machanics
        ///
        /// The range_step must iterate through all
        /// tags in the range.  Already executed/prepared steps must not be
        /// re-executed/re-prepared in range_step. If at least one step/tag
        /// did not complete, range_step must throw exception data_not_ready.
        ///
        /// We need the extra template argument Range to provide an 'empty' template specialization for no_range.
        ///
        /// The tuner interface provides ways to further optimize performance.
        /// StepTuner::check_deps_in_ranges can be used to bypass dependency checks.
        /// \see range_step
        /// \see tagged_step_instance
        /// \see default_tuner
        /// \see range_step
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        class range_step_instance : public tagged_step_instance< Range >
        {
            typedef step_launcher< Tag, Step, Arg, TagTuner, StepTuner > step_launcher_type;
            typedef typename TagTuner::partitioner_type partitioner_type;
        public:
            range_step_instance( const Range & range, const StepTuner & steptuner, context_base & ctxt,
                                 const step_launcher_type & sl, scheduler_i & sch, bool pre_split = false );
            range_step_instance( const range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range > & org, const Range & range );
            ~range_step_instance();
            virtual int affinity() const;
            virtual CnC::Internal::StepReturnValue_t execute();
            virtual char prepare( step_delayer &, int &, const schedulable * );
            void compute_on( int target );
            virtual const step_launcher_base< Range, no_range > * get_step_launcher() const { return NULL; }
            
            // now the stuff needed for partitioner
            void originate_range( const Range & ) const;
            void originate( const Tag & ) const;

            virtual std::ostream & format( std::ostream & os ) const;
            
        private:
            typedef step_args< Tag, Range, step_launcher_type, range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range > > step_args_type;
            typedef range_step< Tag, Range, step_launcher_type, range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >,
                                typename partitioner_type::split_type, StepTuner::check_deps_in_ranges > step_type;
        
            const step_launcher_type & m_stepLauncher;
            char                     * m_statuses;
            const StepTuner          & m_stepTuner;
            partitioner_type           m_partitioner;
            const step_type            m_step;
            mutable bool               m_inited;
            //            static std::string         s_timingName;
            static std::string         s_tracingName;
        };

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        class range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, no_range >
        {
        };

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        std::string range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::s_tracingName = "";

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
		
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
		int range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::affinity() const
		{
				return m_stepLauncher.get_step_tuner().affinity( this->m_tag.Value(), m_stepLauncher.m_arg );
		}
		
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // now the stuff needed for partitioner
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        void range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::originate_range( const Range & range ) const
        {
            step_instance_base * _rsi = new range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >( *this, range );
            CNC_ASSERT( &this->scheduler() == &_rsi->scheduler() );
            if( ! _rsi->scheduler().prepare( _rsi ) ) delete _rsi;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        void range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::originate( const Tag & tag ) const
        {
            //inited = false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::range_step_instance( const Range & range,
                                                                                                const StepTuner & steptuner, context_base & ctxt,
                                                                                                const step_launcher_type & sl, scheduler_i & sch, bool pre_split )
            : tagged_step_instance< Range >( range, ctxt, 1 /* FIXME highest priority? */, sch ),
              m_stepLauncher( sl ),
              m_statuses( NULL ),
              m_stepTuner( steptuner ),
              m_partitioner( sl.m_tagTuner.partitioner() ),
              m_step(),
              m_inited( false )
        {
            static tbb::atomic< bool > _inited;
            if( _inited.fetch_and_store( true ) == false ) {
                if( s_tracingName.empty() ) {
                    s_tracingName = "Range<";
                    s_tracingName.append( m_stepLauncher.m_stepColl.name() );
                    s_tracingName.append( ">" );
                }
            }
            if( m_stepLauncher.timing() ) this->enable_timing();
            if( pre_split ) {
                m_partitioner.divide_and_originate( this->m_tag.Value(), *this );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::range_step_instance( const range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range > & org, const Range & range )
            : tagged_step_instance< Range >( range, org.m_context, 1 /* FIXME highest priority? */, org.scheduler() ),
              m_stepLauncher( org.m_stepLauncher ),
              m_statuses( NULL ),
              m_stepTuner( org.m_stepTuner ),
              m_partitioner( org.m_partitioner ),
              m_step(),
              m_inited( false )
        {
            if( m_stepLauncher.timing() ) this->enable_timing();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::~range_step_instance()
        {
            if( m_statuses ) scalable_free( m_statuses );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        CnC::Internal::StepReturnValue_t range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::execute()
        { 
            Range & _range = this->m_tag.Value();
            bool _firstTime = m_statuses == NULL;
            step_args_type args( m_statuses, m_stepLauncher, m_inited, this );
            if( _firstTime ) {
                // scheduler_i made sure we have an additional dependency, so no concurrent put will accidently schedule this step
                // -> not needed:  this->suspend();
                // split range until no done splitting
                while( m_partitioner.divide_and_originate( _range, *this ) ) {}
                // has been prepared before, even though possibly when it was still a larger range
                // but we need to prepare the individual steps per tag
                m_step.prepare( _range, args );
                if( this->num_suspends() > 1 ) {
                    return CNC_Unfinished;
                }
            }
            CnC::Internal::StepReturnValue_t _res 
                = tagged_step_instance< Range >::do_execute( this,
                                                             m_step,
                                                             _range,
                                                             args,
                                                             this->context(),
                                                             false,
                                                             m_stepLauncher.timing(),
                                                             m_stepLauncher.m_stepColl.trace_level() > 0,
                                                             s_tracingName,
                                                             m_stepLauncher.itacid() );
            // This assertion can legaly fail if another thread put the last unavailable item after our own unsuspend
            // CNC_ASSERT( _res == CNC_Success || this->suspended() );
            // FIXME: grmpf marks as unfinished if first exe was unsuccessful; otherwise it is deleted and the
            //              subsequent schedule from scheduler_i::wait_all (pending) operates on freed memory
            //  return _firstTime ? _res : CNC_Unfinished;
            
            return this->is_sequentialized() ? CNC_NeedsSequentialize : ( this->is_pending() ? CNC_Unfinished : _res );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        char range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::prepare( step_delayer & sD, int & passOnTo, const schedulable * )
        {
            // no other thread can know about this step before this method has started
            CNC_ASSERT( ! this->prepared() );
            // let's make sure we have an additional dependency, so no concurrent put will accidently schedule this step
            this->suspend();
            // no need to reset_was_suspended()
            CNC_ASSERT( this->m_nSuspenders == 1 );

            const StepTuner & _tuner = this->m_stepLauncher.get_step_tuner();
            Arg &  _arg  = this->m_stepLauncher.m_arg;

            if( ! this->prepare_compute_on( this->m_tag.Value(), passOnTo, _arg, _tuner ) ) {
                this->unsuspend();
                return CNC_Unfinished;
            }
            this->m_status = CNC_Prepared;
            
            _UNUSED_ bool _ret = this->unsuspend();
            CNC_ASSERT( _ret );
            return CNC_Prepared;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        void range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::compute_on( int target )
        {
            this->m_stepLauncher.compute_on_range( this->m_tag.Value(), target );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range >
        std::ostream & range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, Range >::format( std::ostream & os ) const
        {
            os << "Range<" << m_stepLauncher.name() << ">(";
            return cnc_format( os, this->m_tag.Value() ) << ")";
        }

    } //   namespace Internal
} // namespace CnC

#endif //RANGE_STEP_INSTANCE_H_ALREADY_INCLUDED
