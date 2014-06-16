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

#ifndef STEP_LAUNCHER_HH_ALREADY_INCLUDED
#define STEP_LAUNCHER_HH_ALREADY_INCLUDED

#include <cnc/serializer.h>
#include <cnc/internal/tag_collection_base.h>
#include <cnc/internal/step_launcher_base.h>
#include <cnc/default_tuner.h>
#include <cnc/itac.h>
#include <tbb/atomic.h>
#include <sstream>

namespace CnC {

    template< typename UserStep, typename StepTuner >  class step_collection;

    namespace Internal {

        template< class StepLauncher > class step_instance;
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner, class Range > class range_step_instance;
        class distributor;
        template< class T > class creator;
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// The step_launcher has most of the functionality that one would attach to a step_collection.
        /// Most of the step-collection's functionality depends on the tag-collection that prescribes it,
        /// When creating a step-collection this information is not know yet.
        /// Only when the step-collection gets prescribed, we have the necessary data.
        /// Hence, the step-collection deals only as a proxy, the actual functionality is in the launcher.
        /// The launcher takes care of preparing and launching step-instances.
        /// Preparing a step-instance calls the different tuning methods, like depends and distribution stuff.
        /// It sends step-instances to other processes and receives instances (recv_msg).
        /// Executing range-steps requires special handling, so we have specific methods for those.
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        class step_launcher : public step_launcher_base< Tag, typename TagTuner::range_type >
        {
        public:
            typedef Tag tag_type;
            typedef Step step_type;
            typedef Arg arg_type;
            typedef TagTuner tag_tuner_type;
            typedef StepTuner step_tuner_type;
            typedef step_instance< step_launcher< Tag, Step, Arg, TagTuner, StepTuner > > step_instance_type;
            typedef step_collection< Step, StepTuner > step_coll_type;
            typedef typename TagTuner::range_type range_type;
            typedef tag_collection_base< Tag, TagTuner > tag_coll_type;

            step_launcher( context_base & ctxt, Arg & arg, const step_collection< Step, StepTuner > & sc, const TagTuner & tt, scheduler_i & sched );
            step_launcher( tag_coll_type * tc, Arg & arg, const step_collection< Step, StepTuner > & sc ) ;
            ~step_launcher();
            virtual tagged_step_instance< tag_type > * create_step_instance( const Tag & tag, context_base & ctxt, bool compute_on ) const;
            virtual tagged_step_instance< range_type > * create_range_step_instance( const range_type & range, context_base & ctxt ) const;
            virtual bool timing() const;
            void compute_on( const Tag & user_tag, int target ) const;
            void compute_on_range( const range_type & range, int target ) const;
            virtual void recv_msg( serializer * ser );

            bool prepare_from_range( tagged_step_instance< range_type > * rsi, const Tag & tag, step_delayer & sD, int & passOnTo ) const;
            CnC::Internal::StepReturnValue_t execute_from_range( tagged_step_instance< range_type > * rsi, const Tag & tag ) const;
            CnC::Internal::StepReturnValue_t execute_step( const Tag & tag ) const { return get_step().execute( tag, m_arg ) ? CNC_Failure : CNC_Success; }

            const tag_tuner_type & get_tag_tuner() const { return m_tagTuner; }

            // from traceable
            virtual void set_tracing( int level );

            int itacid() const
#ifdef CNC_WITH_ITAC
            { return m_itacid; }
#else
            { return 0; }
#endif

        private:
            const step_tuner_type & get_step_tuner() const { return m_stepColl.m_tuner; }
            const Step  & get_step()  const { return m_stepColl.m_userStep; }

            const Tag * get_tag_type() const { return NULL; } // need something to help the compiler deduce template argument type
            Arg                                        & m_arg;
            mutable tbb::atomic< step_instance_type * >  m_stepInstance; // use pointer & lazy init avoid default constructor for tags
            const step_coll_type                       & m_stepColl;
            const TagTuner                             & m_tagTuner;
            scheduler_i                                & m_scheduler;
            tag_coll_type                              * m_tagColl;
            friend class step_instance< step_launcher< Tag, Step, Arg, TagTuner, StepTuner > >;
            friend class range_step_instance< Tag, Step, Arg, TagTuner, StepTuner, typename TagTuner::range_type >;
#ifdef CNC_WITH_ITAC
            int m_itacid;
#endif
        };

    } //   namespace Internal
} // namespace CnC

#include <cnc/internal/step_instance.h>
#include <cnc/internal/range_step_instance.h>

namespace CnC {
    namespace Internal {

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::step_launcher( context_base & ctxt, Arg & arg,
                                                                             const step_collection< Step, StepTuner > & sc,
                                                                             const TagTuner & tt, scheduler_i & sched ) 
            : step_launcher_base< Tag, range_type >( ctxt ),
              m_arg( arg ),
              m_stepInstance(),
              m_stepColl( sc ),
              m_tagTuner( tt ),
              m_scheduler( sched ),
              m_tagColl( NULL )
        {
            m_stepInstance = NULL;
#ifdef CNC_WITH_ITAC
            int _c;
            std::string _n = sc.name();
            if( _n == "" ) {
                std::stringstream _ss;
                _ss << "Step_" << this->gid();
                _n = _ss.str();
            }
            VT_classdef( _n.c_str(), &_c );
            VT_funcdef( _n.c_str(), _c, &m_itacid );
#endif
                  //            VT_SYMDEF( distributable::gid(), sc.name().c_str() );
        }

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::step_launcher( tag_coll_type * tc, Arg & arg, const step_collection< Step, StepTuner > & sc ) 
            : step_launcher_base< Tag, range_type >( tc->get_context() ),
              m_arg( arg ),
              m_stepInstance(),
              m_stepColl( sc ),
              m_tagTuner( tc->tuner() ),
              m_scheduler( tc->get_context().scheduler() ),
              m_tagColl( tc ) 
        {
            m_stepInstance = NULL;
#ifdef CNC_WITH_ITAC
            int _c;
            std::string _n = sc.name();
            if( _n == "" ) {
                std::stringstream _ss;
                _ss << "Step_" << this->gid();
                _n = _ss.str();
            }
            VT_classdef( _n.c_str(), &_c );
            VT_funcdef( _n.c_str(), _c, &m_itacid );
#endif
                  //            VT_SYMDEF( distributable::gid(), sc.name().c_str() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::~step_launcher( )
        { 
            delete m_stepInstance;
            /* delete m_step; */ 
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        tagged_step_instance< typename step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::tag_type > * 
        //typename step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::step_instance_type * 
        step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::create_step_instance( const Tag & tag, context_base & ctxt, bool compute_on ) const
        {
            step_instance_type * _si = new step_instance_type( tag, ctxt, this );
            if( m_scheduler.prepare( _si, compute_on ) ) return _si;
            delete _si;
            return NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename range_type >
        struct tag_for_range
        {
            static Tag get( const range_is_tag_type &, const range_type & r  )
            {
                return r;
            }

            static Tag get( const range_is_range_type &, const range_type & r  )
            {
                return r.begin();
            }
            template< typename Partitioner >
            static Tag get( const range_type & r, const Partitioner & )
            {
                return get( typename Partitioner::split_type(), r );
            }
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class range_type, class step_instance_type, class StepTuner, class StepLauncher >
        tagged_step_instance< range_type > * create_rsi( const Tag *, const range_type & range, const StepLauncher * sc, context_base & ctxt, scheduler_i & sch,
                                                         const StepTuner & tuner, tbb::atomic< step_instance_type * > & stepInstance, bool compute_on = true )
        {
            // avoid default constructor for tags
            if( stepInstance == NULL ) {
                step_instance_type * _tmp = new step_instance_type( tag_for_range< Tag, range_type >::get( range, sc->get_tag_tuner().partitioner() ),
                                                                    ctxt, sc );
                if( stepInstance.compare_and_swap( _tmp, NULL ) != NULL ) delete _tmp;
            }
#ifndef CNC_PRE_SPLIT
# define CNC_PRE_SPLIT false
#endif
            typedef range_step_instance< Tag, typename StepLauncher::step_type, typename StepLauncher::arg_type, typename StepLauncher::tag_tuner_type, StepTuner, range_type > rsi_type;

            tagged_step_instance< range_type > * _si = new rsi_type( range, tuner, ctxt, *sc, sch, CNC_PRE_SPLIT );
            if( sch.prepare( _si, compute_on ) ) return _si;
            delete _si;
            return NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class step_instance_type, class StepTuner, class StepLauncher >
        tagged_step_instance< no_range > * create_rsi( const Tag *, const no_range & range, const StepLauncher * sc, context_base & ctxt, scheduler_i & sch, const StepTuner & tuner, tbb::atomic< step_instance_type * > & stepInstance, bool = true )
        {
            CNC_ABORT( "You need to specify a range type to put a range" );
            return NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        tagged_step_instance< typename step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::range_type > *
        step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::create_range_step_instance( const typename step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::range_type & range, context_base & ctxt ) const
        {
            return create_rsi( get_tag_type(), range, this, ctxt, m_scheduler, get_step_tuner(), m_stepInstance );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        bool step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::prepare_from_range( tagged_step_instance< range_type > * rsi, const Tag & tag, step_delayer & sD, int & passOnTo ) const
        {
            return m_stepInstance->prepare_from_range( rsi, tag, sD );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        CnC::Internal::StepReturnValue_t step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::execute_from_range( tagged_step_instance< range_type > * rsi, const Tag & tag ) const
        {
            return m_stepInstance->execute_from_range( rsi, tag );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        bool step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::timing() const
        {
            return m_stepColl.timing();
        }
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        void step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::set_tracing( int level )
        {
            traceable::set_tracing( level );
            const_cast< step_collection< Step, StepTuner > & >( m_stepColl ).set_tracing( level );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            
        namespace SC {
            static const char TAG   = 0; // message contains tag
            static const char RANGE = 1; // message contains range
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        void step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::compute_on( const Tag & user_tag, int target ) const
        {
#ifdef _DIST_CNC_
            VT_FUNC( "Dist::step_coll::compute_on" );
            if( m_stepColl.trace_level() > 2 ) {
                Speaker oss;
                oss << "Send step " << m_stepColl.name() << "(";
                cnc_format( oss, user_tag ) << ") to " << target;
            }
            serializer * _ser = this->context().new_serializer( this );
            (*_ser) & SC::TAG & user_tag;

            if ( target == COMPUTE_ON_ALL || target == COMPUTE_ON_ALL_OTHERS ) {
                this->context().bcast_msg( _ser );
            } else {
                this->context().send_msg( _ser, target );
            }
#endif
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            
        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        void step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::compute_on_range( const range_type & range, int target ) const
        {
#ifdef _DIST_CNC_
            VT_FUNC( "Dist::step_coll::compute_on" );
            serializer * _ser = this->context().new_serializer( this );
            (*_ser) & SC::RANGE & range;
            this->context().send_msg( _ser, target );
#endif
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Step, class Arg, class TagTuner, class StepTuner >
        void step_launcher< Tag, Step, Arg, TagTuner, StepTuner >::recv_msg( serializer * ser )
        {
#ifdef _DIST_CNC_
            VT_FUNC( "Dist::step_coll::recv_msg" );
            char _msg;

            (*ser) & _msg;
            if( _msg == SC::TAG ) {
                Tag user_tag;
                (*ser) & user_tag;
                if( m_stepColl.trace_level() > 2 ) {
                    Speaker oss;
                    oss << "Receive step " << m_stepColl.name() << "(";
                    cnc_format( oss, user_tag ) << ")";
                }
                if( m_tagColl == NULL ) {
                    tagged_step_instance< Tag > * _si = create_step_instance( user_tag, this->context(), false );
                } else {
                    m_tagColl->Put( user_tag, this->id() );
                }
                //                m_scheduler->prepare( _si, false );
                serializer _clser;
                _clser.set_mode_cleanup();
                _clser.cleanup( user_tag );
            } else {
                CNC_ASSERT( _msg == SC::RANGE );
                range_type range;
                (*ser) & range;
                tagged_step_instance< range_type > * _si = create_rsi( get_tag_type(), range, this, this->context(), m_scheduler, get_step_tuner(), m_stepInstance, false );
                if( m_stepColl.trace_level() > 2 ) {
                    Speaker oss;
                    oss << "Receive range-step " << m_stepColl.name() << "(";
                    cnc_format( oss, range ) << ") @" << _si;
                }
                //                m_scheduler->prepare( _si, false );
                serializer _clser;
                _clser.set_mode_cleanup();
                _clser.cleanup( range );
            }
#endif
        }

    } // namespace Internal
} // end namespace CnC


#endif //STEP_LAUNCHER_HH_ALREADY_INCLUDED
