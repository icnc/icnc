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

#ifndef _CnC_TAG_COLLECTION_BASE_H_
#define _CnC_TAG_COLLECTION_BASE_H_

#include <cnc/internal/tag_collection_i.h>
#include <cnc/internal/typed_tag.h>
#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/no_tag_table.h>
#include <tbb/concurrent_vector.h>
#include <sstream>

namespace CnC {
    namespace Internal {

        template< typename Tuner > const Tuner & get_default_tuner();
        template< class Tag, class Range > class step_launcher_base;

        /// The actual implementation of tag-collections.
        /// For a tag collection, we maintain a list of prescribed step collections.
        /// When a tag is put (Put()), we schedule a step instance from each of
        /// the prescribed tag collections.
        template< class Tag, class Tuner >
        class tag_collection_base : public tag_collection_i
        {
        public:
            typedef typename Tuner::tag_table_type TagTable_t;

            tag_collection_base( context_base & g, const std::string & name );
            tag_collection_base( context_base & g, const std::string & name, const Tuner & tnr );
            ~tag_collection_base();
            /// prescribe a step-collection, represented by a step_launcher
            int addPrescribedCollection( step_launcher_base< Tag, typename Tuner::range_type > * step_launcher );
            virtual void unsafe_reset( bool );
            /// \return the size of a collection.
            size_t size() const;
            /// \return a bool indicating if the Tag collection is empty or not
            bool empty() const;
            void Put( const Tag & user_tag, const int mask = -1 );
            /// when a tag_range is put, we create a "super-step-instance"
            void create_range_instances( const typename Tuner::range_type & r );
            /// needed by distCNC (distributable)
            virtual void recv_msg( serializer * );
            const Tuner & tuner() const { return m_tuner; }

            typedef typename TagTable_t::const_iterator const_iterator;
            const_iterator begin() const { return m_tagTable.begin(); }
            const_iterator end() const { return m_tagTable.end(); }

            /// the callback type for writing re-usable graphs (CnC::graph)
            typedef struct _callback {
                virtual void on_put( const Tag & ) = 0;
                virtual ~_callback() {}; 
            } callback_type;
            /// register a callback, called whe a tag is put
            /// not thread safe
            void on_put( callback_type * cb );

        private:
            typedef tbb::concurrent_vector< step_launcher_base< Tag, typename Tuner::range_type > * >  PrescribedStepCollections_t;
            PrescribedStepCollections_t m_prescribedStepCollections;
            void do_reset();

            // the set of tags in the TagCollection
            const Tuner & m_tuner;
            TagTable_t    m_tagTable;
            typedef std::vector< callback_type * > callback_vec;
            callback_vec m_onPuts;
            int m_allMask;
        }; // class tag_collection_base

    } // namespace Internal
} // namespace CnC

#include <cnc/internal/step_launcher.h>

namespace CnC {
    namespace Internal {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        namespace TC {
            const char RESET = 11; // request reset
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        tag_collection_base< Tag, Tuner >::tag_collection_base( context_base & g, const std::string & name )
            : tag_collection_i( g, name ),
              m_prescribedStepCollections(),
              m_tuner( get_default_tuner< Tuner >() ),
              m_tagTable(),
              m_onPuts(),
              m_allMask( 0 )
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        tag_collection_base< Tag, Tuner >::tag_collection_base( context_base & g, const std::string & name, const Tuner & tnr )
            : tag_collection_i( g, name ),
              m_prescribedStepCollections(),
              m_tuner( tnr ),
              m_tagTable(),
              m_onPuts(),
              m_allMask( 0 )
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        tag_collection_base< Tag, Tuner >::~tag_collection_base()
        {
            for( typename PrescribedStepCollections_t::const_iterator ci = this->m_prescribedStepCollections.begin();
                 ci != this->m_prescribedStepCollections.end();
                 ++ci ) {
                // FIXME: StepCollections need revivision
                delete *ci;
            }
            for( typename callback_vec::iterator i = m_onPuts.begin(); i != m_onPuts.end(); ++i ) {
                delete (*i);
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        int tag_collection_base< Tag, Tuner >::addPrescribedCollection( step_launcher_base< Tag, typename Tuner::range_type > * step_launcher )
        {
            int id = ( 1 << m_prescribedStepCollections.size() );
            step_launcher->set_id( id ),
            m_prescribedStepCollections.push_back( step_launcher );
            m_allMask |= id;
            return id;
        }
    
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        size_t tag_collection_base< Tag, Tuner >::size() const
        { // Return the size of a collection.
            return m_tagTable.size();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        void tag_collection_base< Tag, Tuner >::unsafe_reset( bool dist )
        {
#ifdef _DIST_CNC_
            if( dist ) {
                serializer * _ser = get_context().new_serializer( this );
                (*_ser) & TC::RESET;
                get_context().bcast_msg( _ser ); 
            }
#endif
            do_reset();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        void tag_collection_base< Tag, Tuner >::do_reset()
        {
            m_tagTable.clear();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        bool tag_collection_base< Tag, Tuner >::empty() const
        { // Return a bool indicating if the Tag collection is empty or not
            return m_tagTable.empty();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Tuner >
        void tag_collection_base< Tag, Tuner >::Put( const Tag & user_tag, const int mask )
        {
            int _lmask = mask & m_allMask;

            // store tag if requested
            int _found = m_tagTable.insert( user_tag, mask );

            // walk the list of prescribed steps, and insert an
            // instance for each one if tag was not inserted yet.
            if( _found != _lmask || m_allMask == 0 ) {
                if ( trace_level() > 0 ) {
                    Speaker oss;
                    oss << "Put tag " << name() << "<";
                    cnc_format( oss, user_tag ) << "> mask " << mask << " " << _lmask << " _found " << _found;
                }
                for( typename PrescribedStepCollections_t::const_iterator ci = this->m_prescribedStepCollections.begin();
                     ci != this->m_prescribedStepCollections.end();
                     ++ci ) {
                    
                    step_launcher_base< Tag, typename Tuner::range_type > *_stepLauncher = *ci;
                    if( ( _lmask & _stepLauncher->id() ) && ! ( _found & _stepLauncher->id() ) ) {
                        
                        // create new step_instance_base and schedule it
                        _stepLauncher->create_step_instance( user_tag, get_context(), mask == -1 );
                        
                    } else if( trace_level() > 0 ) { Speaker oss; oss << "\t{" << _stepLauncher->id() << " memoized}"; }
                }
                if( _found == 0 ) {
                    for( typename callback_vec::iterator i = m_onPuts.begin(); i != m_onPuts.end(); ++i ) {
                        (*i)->on_put( user_tag );
                    }
                }
            } else if ( trace_level() > 0 ) {
                Speaker oss;
                oss << "Put tag " << name() << "<";
                cnc_format( oss, user_tag ) << ">  {memoized} mask " << mask << " " << _lmask << " _found " << _found;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename Tuner >
        void tag_collection_base< Tag, Tuner >::create_range_instances( const typename Tuner::range_type & r )
        {
            if ( trace_level() > 0 ) {
                Speaker oss;
                oss << "Put range " << name() << "<";
                cnc_format( oss, r ) << ">";
            }

            // walk the list of prescribed steps, and insert an
            // instance for each one if tag was not inserted yet.
            for( typename PrescribedStepCollections_t::const_iterator ci = this->m_prescribedStepCollections.begin();
                 ci != this->m_prescribedStepCollections.end();
                 ++ci ) 
            {
                step_launcher_base< Tag, typename Tuner::range_type > *_stepLauncher = *ci;
#ifndef CNC_PRE_SPLIT
# define CNC_PRE_SPLIT false
#endif
                typename Tuner::range_type _range( r );
                // create new step_instance and schedule it
                // tagged_step_instance< typename Tuner::range_type > * _si = 
                (void) _stepLauncher->create_range_step_instance( r, get_context() );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename Tuner >
        void tag_collection_base< Tag, Tuner >::on_put( callback_type * cb )
        {
            // not thread-safe!
            m_onPuts.push_back( cb );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename Tuner >
        void tag_collection_base< Tag, Tuner >::recv_msg( serializer * ser )
        {
#ifdef _DIST_CNC_
            CNC_ASSERT( distributor::active() );
            char _msg;
            (*ser) & _msg;
            switch( _msg ) {
            case TC::RESET :
                {
                    do_reset();
                    break;
                }
            default :
                CNC_ABORT( "Protocol error: enexpected message tag." );
            }
            // Don't forget Cleanup
#endif
        }

    } // namespace Internal
} // namespace CnC

#endif // _CnC_TAG_COLLECTION_BASE_H_
