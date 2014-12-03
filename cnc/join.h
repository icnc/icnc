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
  Joins/corss products for CnC, provided as a re-usable graph.
*/

#ifndef _CnC_JOIN_H_
#define _CnC_JOIN_H_

#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/queuing_mutex.h>
#include <set>

namespace CnC
{

    template< typename TagA, typename TunerA, typename TagB, typename TunerB, typename TagC, typename TunerC > class join;

    /// Returns a graph that joins 2 tag-collections into a third one.
    /// Continuously produces the join product of the 2 input tag-collections
    /// and puts it into the output tag-collection.
    ///
    /// Accepts any types; only requires that a an output tag-type
    /// provides a constructor which accepts (taga, tagb) to construct
    /// a joined tag from tag a and b.
    ///
    /// On distributed memory duplicate output tags might be produced.
    /// To avoid duplicate step execution use tag-preservation on the
    /// output tag-collection (CnC::preserve_tuner) and suitable
    /// distribution functions on the prescribed step-collection
    /// (CnC::step_tuner).
    ///
    /// \param ctxt  the context to which the graph belongs
    /// \param name  the name of this join graph instance
    /// \param a     first input collection
    /// \param b     second input collection
    /// \param c     output collection
    template< typename Ctxt, typename TagA, typename TunerA, typename TagB, typename TunerB, typename TagC, typename TunerC >
    graph * make_join_graph( CnC::context< Ctxt > & ctxt, const std::string & name, 
                             CnC::tag_collection< TagA, TunerA > & a,
                             CnC::tag_collection< TagB, TunerB > & b,
                             CnC::tag_collection< TagC, TunerC > & c )
    {
        return new join< TagA, TunerA, TagB, TunerB, TagC, TunerC >( ctxt, name, a, b, c );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // The actual join graph. A complex template construct; use
    // make_join_graph to create a join graph. Only the constructor is
    // public, everything else is private ("hidden").
    //
    // When ever a new tag arrives, it gets joined with all existing
    // tags in the other collection.  The actual join is protected by a
    // mutex. This makes it safe on a single process.
    //
    // We cannot rely on the collection to keep tags because we
    // currently have no thread-safe iteration facility. Hence we use
    // std::set to keep them privately in the graph.
    //
    // We need explicit handling of distributed memory. We keep the
    // tags locally where they are put and just send a bcast message
    // when a new tag arrives. This can produce duplicate output tags.
    template< typename TagA, typename TunerA, typename TagB, typename TunerB, typename TagC, typename TunerC >
    class join : public CnC::graph, CnC::Internal::no_copy
    {
        typedef tbb::queuing_mutex mutex_type;
        typedef std::set< TagA > seta_type;
        typedef std::set< TagB > setb_type;
        typedef CnC::tag_collection< TagA, TunerA > colla_type;
        typedef CnC::tag_collection< TagB, TunerB > collb_type;
        typedef CnC::tag_collection< TagC, TunerC > collc_type;
        typedef join< TagA, TunerA, TagB, TunerB, TagC, TunerC > join_type;
        enum { JOINA = 'A', JOINB = 'B' };
        friend struct on_a_put;
        friend struct on_b_put;

    public:
        template< typename Ctxt >
        join( CnC::context< Ctxt > & ctxt, const std::string & name, colla_type & a, collb_type & b, collc_type & c )
            : CnC::graph( ctxt, name ),
              m_a( a ),
              m_b( b ),
              m_c( c ),
              m_seta(),
              m_setb(),
              m_mutex()
        {
            // callback objects must persist the lifetime of collections
            m_a.on_put( new on_a_put( this ) );
            m_b.on_put( new on_b_put( this ) );
        }

        // the implementation is "hidden"
	private:
        // join a given tag with all existing tags in the other set
        template< typename Tag, typename Set, typename joiner >
        void join_one( const Tag & t, const Set & set, const joiner & j )
        {
            // in a more advanced version we can go parallel by using parallel_for or a range
            for( typename Set::iterator i=set.begin(); i!=set.end(); ++i ) {
                m_c.put( j( t, *i ) );
            }
        }

        // join a b-tag with a a-tag
        struct join_ba
        { 
            TagC operator()( const TagB & b, const TagA & a ) const {
				return TagC( a, b );
            }
        };
        // join a a-tag with a b-tag
        struct join_ab
        { 
            TagC operator()( const TagA & a, const TagB & b ) const {
				return TagC( a, b );
            }
        };
        typedef join_ab join_ab_type;
        typedef join_ba join_ba_type;

        template< typename Tag >
        void send_tag( const Tag & tag, const char op )
        {
#ifdef _DIST_CNC_
            if( Internal::distributor::active() ) {
                if( this->trace_level() > 2 ) {
                    Internal::Speaker oss(std::cout);
                    oss << this->name() << "::send_tag JOIN" << (op==JOINA?"A":"B") << " [";
                    cnc_format( oss, tag );
                    oss << "]";
                }
                CnC::serializer * ser = this->new_serializer();
                (*ser) & op & tag;
                this->bcast_msg( ser );
            }
#endif
        }

        // callback for collection a
        struct on_a_put : public colla_type::callback_type
        {
            on_a_put( join_type * j  ) : m_join( j ) {}

            void on_put( const TagA & tag )
            {
                if( m_join->trace_level() > 0 ) {
                    Internal::Speaker oss(std::cout);
                    oss << m_join->name() << "::on_put_a <";
                    cnc_format( oss, tag );
                    oss << ">";
                }
                m_join->send_tag( tag, JOINA );
                mutex_type::scoped_lock _lock( m_join->m_mutex );
                m_join->join_one( tag, m_join->m_setb, join_ab_type() );
                m_join->m_seta.insert( tag );
            }
        private:
            join_type * m_join;
        };

        // callback for collection b
        struct on_b_put : public collb_type::callback_type
        {
            on_b_put( join_type * j  ) : m_join( j ) {}

            void on_put( const TagB & tag )
            {
                if( m_join->trace_level() > 0 ) {
                    Internal::Speaker oss(std::cout);
                    oss << m_join->name() << "::on_put_b <";
                    cnc_format( oss, tag );
                    oss << ">";
                }
                m_join->send_tag( tag, JOINB );
                mutex_type::scoped_lock _lock( m_join->m_mutex );
                m_join->join_one( tag, m_join->m_seta, join_ba_type() );
                m_join->m_setb.insert( tag );
            }
        private:
            join_type * m_join;
        };
		
#ifdef _DIST_CNC_
        virtual void recv_msg( serializer * ser )
        {
            char _op;
            (*ser) & _op;
            switch( _op ) {
            case JOINA : {
                TagA _tag;
                (*ser) & _tag;
                if( this->trace_level() > 2 ) {
                    Internal::Speaker oss(std::cout);
                    oss << this->name() << "::recv_msg JOINA <";
                    cnc_format( oss, _tag );
                    oss << ">";
                }
                mutex_type::scoped_lock _lock( m_mutex );
                join_one( _tag, m_setb, join_ab_type() );
                break;
            }
            case JOINB : {
                CNC_ASSERT_MSG( _op == JOINB, "Unexpected message tag" );
                TagB _tag;
                (*ser) & _tag;
                if( this->trace_level() > 2 ) {
                    Internal::Speaker oss(std::cout);
                    oss << this->name() << "::recv_msg JOINB <";
                    cnc_format( oss, _tag );
                    oss << ">";
                }
                mutex_type::scoped_lock _lock( m_mutex );
                join_one( _tag, m_seta, join_ba_type() );
                break;
            }
            default :
                CNC_ASSERT_MSG( false, "Unexpected message tag in JOIN" );
            }
        }
#endif

        colla_type & m_a;
        collb_type & m_b;
        collc_type & m_c;
        mutex_type   m_mutex;
        seta_type    m_seta;
        setb_type    m_setb;
    };


} // namespace CnC

#endif //_CnC_JOIN_H_
