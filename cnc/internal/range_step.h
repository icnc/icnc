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

#ifndef RANGE_STEP_H_ALREADY_INCLUDED
#define RANGE_STEP_H_ALREADY_INCLUDED

#include <cnc/internal/schedulable.h>
#include <cnc/default_partitioner.h>
#include <cnc/itac.h>
#include <cnc/internal/data_not_ready.h>
#include <cnc/internal/step_delayer.h>

#include <sstream>
#include <climits>

namespace CnC {
    namespace Internal {

        static const char UNPREPARED = 0, PREPARED = 1, DONE = 2;

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Range, class StepLauncher, class RangeStepI >
        struct step_args
        {
            step_args( char *& status, const StepLauncher & sc, bool & inited, /* bool & hasdeps,*/ RangeStepI * rsi )
                : m_statuses( status ), m_stepLauncher( sc ), m_inited( inited ), /*m_hasDeps( hasdeps ), */ m_rangeStepI( rsi ), m_last( INT_MAX ) {}
            char              *& m_statuses;
            const StepLauncher & m_stepLauncher;
            bool               & m_inited;
            //            bool               & n_hasDeps;
            RangeStepI        *  m_rangeStepI;
            int                  m_last;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// range_step basically is a CnC step. It wraps a normal user-step to
        /// execute more than one such user-step through a range of
        /// tags. Hence, its input tag for execute is a range. It iterates
        /// once through its sub-steps performing to things:
        /// 1. preparing
        /// 2. execution.
        ///
        /// If at least one step did not succeed, it will throw data_not_ready
        /// but it will always visit every tag/instance when executed (duplicate 
        /// preparation and/or execution is prevented, of course).
        ///
        /// Dependences within the step-instances of a range-step are handled
        /// by re-executing the range_step_instance until all deps are met.
        /// We return CNC_NeedsExtraWake to signal this.
        ///
        /// A range that is to be sent to another process (compute_on) whould not
        /// go through any of this code (only where it is actually executed). The
        /// range_step_instance must take care of this.
        ///
        /// The corresponding range_step_instance will "collect" all 
        /// potential item-depdencies, e.g. the range_step_instance will
        /// be suspended (there are no individual step_instances). Hence
        /// a range_step must only be executed from a "active" range_step_instance.
        ///
        /// range_step comes in 2 explicit specializations:
        /// 1. for ranges of tags, executing steps on a per-tag basis
        /// 2. for ranges of ranges, e.g. if the range is also the tag;
        ///    in this case the step is called only once without iterating
        ///    through the given range.
        /// Differentiation is achieved through Partitioner::split_type.
        /// The specialization is *required* because otherwise the user step
        /// would need to accept a range as well as the tag. If separated
        /// in specializations, only the desired code path is compiled.
        ///
        /// Template parameter deps enables/disables dependency checks
        /// (\see default_tuner). If enabled, each step execution must
        /// go through tagged_step_instance::do_execute to catch potentially
        /// unavailable items. It must also update a status-array to avoid
        /// duplicate preparation/execution of steps.
        ///
        /// if deps == true execute_from_range must go through
        /// tagged_step_instance::do_execute if deps == false we call the
        /// user-step right away FIXME we might want 2 more explicit
        /// specializations for dep==true/false (make it type not bool)
        template< class Tag, class Range, class StepLauncher, class RangeStepI, class TIR, bool deps >
        struct range_step
        {
        public:
            CnC::Internal::StepReturnValue_t execute( const Range & range, step_args< Tag, Range, StepLauncher, RangeStepI > & inp ) const;
            void prepare( const Range & range, step_args< Tag, Range, StepLauncher, RangeStepI > & inp ) const;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        // if deps == true execute_from_range must go through tagged_step_instance::do_execute
        // if deps == false we call the user-step right away
        // seems we need 2 explicit specializations for dep==true/false (make it type not bool)
        // need to include the global lock

        template< class Tag, class Range, class StepLauncher, class RangeStepI, bool deps >
        struct range_step< Tag, Range, StepLauncher, RangeStepI, range_is_range_type, deps >
        {
            void prepare( const Range & range, step_args< Tag, Range, StepLauncher, RangeStepI > & inp ) const
            {
                // init executed flag at first execution
                if( deps && inp.m_statuses == NULL ) {
                    inp.m_statuses = static_cast< char* >( scalable_calloc( range.size(), sizeof( *inp.m_statuses ) ) );
                    typename Range::const_iterator e = range.end();
                    int p = 0;
                    for( typename Range::const_iterator i = range.begin(); i != e && p <= inp.m_last; ++i, ++p ) {
                        CNC_ASSERT( inp.m_statuses[p] == UNPREPARED );
                        step_delayer _sD;
                        int _target = NO_COMPUTE_ON;
                        bool _ret = inp.m_stepLauncher.prepare_from_range( inp.m_rangeStepI, i, _sD, _target );
                        inp.m_statuses[p] = _ret ? PREPARED : DONE;
                    }
                    //                    inp.hasDeps = inp.m_rangeStepI->num_suspends() > 1;
                }
            }
                                
            CnC::Internal::StepReturnValue_t execute( const Range & range, step_args< Tag, Range, StepLauncher, RangeStepI > & inp ) const
            {
                // init executed flag at first execution
                CNC_ASSERT( ! deps || inp.m_statuses != NULL );
                int _numsus1 =  inp.m_rangeStepI->num_suspends();
                int _numsus2 = _numsus1;
                int _numreplay = 0;
                int _last = INT_MAX;
                bool _nseq = false;
                typename Range::const_iterator e = range.end();
                do {
                    int p = 0;
                    _numreplay = 0;
                    for( typename Range::const_iterator i = range.begin(); i != e && p <= inp.m_last; ++i, ++p ) {
                        if( deps ) {
                            if( inp.m_statuses[p] != DONE ) {
                                StepReturnValue_t rv = inp.m_stepLauncher.execute_from_range( inp.m_rangeStepI, i );
                                if( rv == CNC_Unfinished ) {
                                    ++_numreplay;
                                    _last = p;
                                } else if( rv == CNC_NeedsSequentialize ) {
                                    ++_numreplay;
                                    _nseq = true;
                                } else {
                                    inp.m_statuses[p] = DONE;
                                }
                            }
                        } else {
#ifndef NDEBUG
                            try {
                                inp.m_stepLauncher.execute_step( i );
                            }
                            catch( data_not_ready ) {
                                std::cerr << "no replay support in fast-call mode\n";
                            }
#else
                            inp.m_stepLauncher.execute_step( i );
#endif
                        }
                    }
                    _numsus2 = _numsus1;
                    if( _last < inp.m_last )  { inp.m_last = _last; }
                } while( _nseq == false && _numsus2 > ( _numsus1 = inp.m_rangeStepI->num_suspends() ) - _numreplay );
                if( deps && _numreplay > 0 )
                    return  _nseq ? CNC_NeedsSequentialize : CNC_NeedsExtraWake;
                return CNC_Success;
            }
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// specialization for self-dividing ranges (range_is_ag_type)
        /// no iteration needed, the range is derictly passed to the step as its (one) tag
        template< class Tag, class Range, class StepLauncher, class RangeStepI, bool deps >
        struct range_step< Tag, Range, StepLauncher, RangeStepI, range_is_tag_type, deps >
        {
            void prepare( const Range &, step_args< Tag, Range, StepLauncher, RangeStepI > & ) const
            {
                // nothing to be done, it needs to reside in execute, as we might throw an exception
            }

            CnC::Internal::StepReturnValue_t execute( const Range & range, step_args< Tag, Range, StepLauncher, RangeStepI > & inp ) const
            {
                if( deps ) {
                    if( ! inp.m_inited ) {
                        step_delayer _sD;
                        int _target = NO_COMPUTE_ON;
                        bool _ret = inp.m_stepLauncher.prepare_from_range( inp.m_rangeStepI, range, _sD, _target );
                        inp.m_inited = true;
                        if( ! _ret ) throw( DATA_NOT_READY );
                    }
                    if( inp.m_stepLauncher.execute_from_range( inp.m_rangeStepI, range ) == CNC_Unfinished ) {
                        throw( DATA_NOT_READY );
                    }
                } else {
#ifndef NDEBUG
                    try {
                        inp.m_stepLauncher.execute_step( range );
                    }
                    catch( data_not_ready ) {
                        std::cerr << "no replay support fast-call mode\n";
                    }
#else
                    inp.m_stepLauncher.execute_step( range );
#endif
                }
                return CNC_Success;
            }
        };

    } //   namespace Internal
} // namespace CnC

#endif //RANGE_STEP_H_ALREADY_INCLUDED
