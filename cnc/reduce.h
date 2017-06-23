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
  Reductions for CnC, provided as a re-usable graph.
*/

#ifndef _CnC_REDUCE_H_
#define _CnC_REDUCE_H_

#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/combinable.h>
#include <tbb/atomic.h>
#include <tbb/spin_rw_mutex.h>
#include <functional>

namespace CnC
{

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select > class reduction;

    /// \defgroup reductions Asynchronous Reductions
    /// @{

    /// Creates a graph for asynchronous reductions.
    ///
    /// Takes an input collection and reduces its content with a given
    /// operation and selection mechanism.  The computation is done
    /// while new items arrive. Not all items need to be available to
    /// start or make progress.  Data input is provided by normal puts
    /// into the input collection. The final reduced value for a
    /// reduction is put into the output collection.
    ///
    /// Supports multiple concurrent reductions (with the same
    /// operation) identified by a reduction id.  For this, a selector
    /// functor can be provided to tell which data-item goes to which
    /// reduction (maps a data-tag to a reduction-id).
    ///
    /// The number reduced items per reduction-id needs to be provided
    /// through a second input collection. You can signal no more
    /// incoming values by putting a count < 0. Providing counts
    /// late reduces communication and potentially improves performance.
    ///
    /// Each reduction is independent of other reductions and can
    /// finish independently while others are still processing.
    /// Connected graphs can get the reduced values with a normal
    /// get-calls (using the desired reduction-id as the tag).
    ///
    /// The implementation is virtually lock-free. On distributed memory
    /// the additional communication is also largely asynchronous.
    ///
    /// See also \ref reuse
    ///
    /// \param ctxt the context to which the graph belongs
    /// \param name the name of this reduction graph instance
    /// \param in   input collection, every item that's put here is
    ///             applied to sel and potentially takes part in a reduction
    /// \param cnt  input collection; number of items for each reduction
    ///             expected to be put here (tag is reduction-id, value is count)
    /// \param out  output collection, reduced results are put here with tags as returned by sel
    /// \param op   the reduction operation:\n
    ///             IType (*)(const IType&, const IType&) const\n
    ///             usually a functor
    /// \param idty the identity/neutral element for the given operation
    /// \param sel  functor, called once for every item put into "in":\n
    ///             bool (*)( const ITag & itag, OTag & otag ) const\n
    ///             must return true if given element should be used for a reduction, otherwise false;\n
    ///             if true, it must set otag to the tag of the reduction it participates in
    template< typename Ctxt, typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    graph * make_reduce_graph( CnC::context< Ctxt > & ctxt, const std::string & name, 
                               CnC::item_collection< ITag, IType, ITuner > & in,
                               CnC::item_collection< OTag, CType, CTuner > & cnt,
                               CnC::item_collection< OTag, IType, OTuner > & out,
                               const ReduceOp & op,
                               const IType & idty,
                               const Select & sel )
    {
        return new reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >( ctxt, name, in, cnt, out, op, idty, sel );
    }


    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // The below is the implementation, normaly users shouldn't need to read it
    // However, you might want to use this a template for writring your own reduction.
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    
#ifdef _DIST_CNC_
    // message tags for dist CnC
    namespace DISTRED {
        static const char BCASTCOUNT  = 93;
        static const char GATHERCOUNT = 94;
        static const char DONE        = 95;
        static const char ALLDONE     = 96;
        static const char VALUE       = 97;
        static const char ALLVALUES   = 98;
    }
#endif

    // status of each reduction
    static const int LOCAL         = 0;
    static const int CNT_AVAILABLE = 1;
    static const int BCAST_DONE    = 2;
    static const int FINISH        = 3;
    static const int DONE          = 4;

    // shortcut macro for ITAC instrumentation
#ifdef CNC_WITH_ITAC
# define TRACE( _m ) static std::string _t_n_( m_reduce->name() + _m ); VT_FUNC( _t_n_.c_str() );
#else
# define TRACE( _m )
#endif

#define M1 static_cast< CType >( -1 )

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // The actual reduction graph. A complex template construct; use
    // make_reduce_graph to create a reduction graph. Only the constructor is
    // public, everything else is private ("hidden").
    //
    // We use tbb::combinable for the local (shard memory) reduction part.
    //
    // On distributed memory we implement the following asyncrhonous protocol
    //   - As long as the count of a given reduction is unkown, we proceed as everything was local.
    //   - As soon as the count arrives, we do the following
    //     0. assign ownership of the reduction to the count-providing process
    //        The owner controls gathering the distributed values when the count
    //     1a. if count is a real count, it bcasts the count
    //     1a if it's a done-flag (-1), we immediately move to 3.
    //     2a. immediately followed by a gather of the counts (but not values)
    //     2b. processes which are not the owners send a message for every additional item that was not gathered in 2a
    //     3. once the onwer sees that all items for the reduction have arrived it bcast DONE
    //     4. immediately followed by a gather of the values
    //     5. when the owner collected all values, it use tbb::combinable and puts the final value
    // As almost everything can happen at the same time, we use transactional-like 
    // operations implemented with atomic variables which guide each reduction through its states
    // We implement our own bcast/gather tree individually for each owner (as its root)
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    class reduction : public CnC::graph//, CnC::Internal::no_copy
    {
    public:
        typedef CnC::item_collection< ITag, IType, ITuner > icoll_type;
        typedef CnC::item_collection< OTag, CType, CTuner > ccoll_type;
        typedef CnC::item_collection< OTag, IType, OTuner > ocoll_type;

        template< typename Ctxt >
        reduction( CnC::context< Ctxt > & ctxt, const std::string & name, icoll_type & in, ccoll_type & c, ocoll_type & out, const ReduceOp & red, const IType & identity, const Select & sel );
        ~reduction();
        
        // sometimes you can't tell number of reduced items until all computation is done.
        // This call finalizes all reductions, no matter if a count was given or not.
        void flush();
        
        // the implementation is "hidden"
    private:
        typedef reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select > reduce_type;
        typedef tbb::spin_rw_mutex mutex_type;

        // thread-local storage per reduction
        struct red_tls {
            tbb::combinable< IType > val;
            tbb::atomic< CType > nreduced;
            tbb::atomic< CType > n;
            mutex_type mtx;
#ifdef _DIST_CNC_
            tbb::atomic< int > nCounts;
            tbb::atomic< int > nValues;
            int owner;
            
#endif
            tbb::atomic< int > status;
            red_tls();
            red_tls( const IType & v );
            red_tls( const red_tls & rt );
        private:
            void operator=( const red_tls & rt );
        };
        typedef tbb::concurrent_unordered_map< OTag, red_tls > tls_map_type;

        // callback for collection a
        struct on_item_put : public icoll_type::callback_type
        {
            on_item_put( reduce_type * r );
            void on_put( const ITag & tag, const IType & val );
#ifdef _DIST_CNC_
            void on_value( const OTag & otag, const typename tls_map_type::iterator & i, const IType & val );
#endif
            void add_value( const typename tls_map_type::iterator & i, const IType & val ) const;
        private:

            reduce_type * m_reduce;
        };
        friend struct on_item_put;

        // callback for count collection
        struct on_count_put : public ccoll_type::callback_type
        {
            on_count_put( reduce_type * r );
            void on_put( const OTag & otag, const CType & cnt );
#ifdef _DIST_CNC_
            void on_done( const OTag & otag, const typename tls_map_type::iterator & i, const int owner );
            void on_bcastCount( const OTag & otag, const typename tls_map_type::iterator & i, const CType & cnt, const int owner );
            void on_gatherCount( const OTag & otag, const typename tls_map_type::iterator & i, const CType & cnt );
#endif
        private:
            reduce_type * m_reduce;
        };
        friend struct on_count_put;

        typename tls_map_type::iterator get( const OTag & tag );
        bool try_put_value( const OTag & otag, const typename tls_map_type::iterator & i );
#ifdef _DIST_CNC_
        bool send_count( const OTag & otag, const typename tls_map_type::iterator & i, const int to, const bool always );
        static int my_parent_for_root( const int root );
        void try_send_or_put_value( const OTag & otag, const typename tls_map_type::iterator & i );
        void try_send_or_put_all();
        // home-grown bcast that uses a tree for each root
        // at some point something like this should go into the CnC runtime
        // returns the number of messages sent (0, 1 or 2)
        int bcast( CnC::serializer * ser, int root );
        int bcast_count( const OTag & tag, const CType & val, const int root );
        virtual void recv_msg( serializer * ser );
#endif
        icoll_type & m_in;
        ccoll_type & m_cnt;
        ocoll_type & m_out;
        on_item_put   * m_ondata;
        on_count_put   * m_oncount;
        ReduceOp     m_op;
        Select       m_sel;
        tls_map_type m_reductions;
        const IType  m_identity;
#ifdef _DIST_CNC_
        tbb::atomic< int > m_nDones;
        bool         m_alldone;
#endif
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::red_tls::red_tls()
    {
        nreduced = 0;
        n = M1;
        status = LOCAL;
#ifdef _DIST_CNC_
        nCounts = -1;
        nValues = -1;
        owner = -1;
#endif
    } 

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::red_tls::red_tls( const IType & v )
    { 
        val = v;
        nreduced = 0;
        n = M1;
        status = LOCAL;
#ifdef _DIST_CNC_
        nCounts = -1;
        nValues = -1;
        owner = -1;
#endif
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::red_tls::red_tls( const red_tls & rt )
        : val( rt.val ),
          nreduced( rt.nreduced ),
          n( rt.n ),
          mtx(),
#ifdef _DIST_CNC_
          nCounts( rt.nCounts ),
          nValues( rt.nValues ),
          owner( rt.owner ),
#endif
          status( rt.status )
    {} 

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    template< typename Ctxt >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::reduction( CnC::context< Ctxt > & ctxt, const std::string & name,
                                                                                                icoll_type & in, ccoll_type & c, ocoll_type & out,
                                                                                                const ReduceOp & red, const IType & identity, const Select & sel )
        : CnC::graph( ctxt, name ),
          m_in( in ),
          m_cnt( c ),
          m_out( out ),
          m_ondata( new on_item_put( this ) ),
          m_oncount( new on_count_put( this ) ),
          m_op( red ),
          m_sel( sel ),
          m_reductions(),
          m_identity( identity )
    {
        // callback objects must persist the lifetime of collections
        m_in.on_put( m_ondata );
        m_cnt.on_put( m_oncount );
#ifdef _DIST_CNC_
        m_alldone = false;
        m_nDones = -1;
#endif
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::~reduction() 
    {
        // delete m_ondata;
        // delete m_oncount;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // sometimes you can't tell number of reduced items until all computation is done.
    // This call finalizes all reductions, no matter if a count was given or not.
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::flush()
    {
#ifdef _DIST_CNC_
        if( Internal::distributor::active() ) {
            if( trace_level() > 0 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " flush: bcast ALLDONE";
            }
            CNC_ASSERT( CnC::tuner_base::myPid() == 0 );
            m_alldone = true;
            CnC::serializer * ser = this->new_serializer();
            (*ser) & DISTRED::ALLDONE;
            m_nDones = 1000; // protect from current messages
            m_nDones += bcast( ser, 0 );
            m_nDones -= 999;
            try_send_or_put_all();
        } else
#endif
        for( typename tls_map_type::iterator i = m_reductions.begin(); i != m_reductions.end(); ++i ) {
            m_out.put( i->first, i->second.val.combine( m_op ) );
        }
    }
    
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    typename reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::tls_map_type::iterator
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::get( const OTag & tag )
    {
        typename tls_map_type::iterator i = m_reductions.find( tag );
        if( i == m_reductions.end() ) {
            i = m_reductions.insert( typename tls_map_type::value_type( tag, red_tls() ) ).first;
        }
        return i;
    }
    
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    bool reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::try_put_value( const OTag & otag, const typename tls_map_type::iterator & i )
    {
        if( trace_level() > 2 ) {
            Internal::Speaker oss(std::cout);
            oss << this->name() << " try_put_value [";
            cnc_format( oss, otag ) << "]"
#ifdef _DIST_CNC_
                << " nValues " << i->second.nValues << " status " << i->second.status
#endif
                ;
        }
        if( i->second.nreduced == i->second.n ) {
            // setting n could go in parallel, it sets new n and then compares
            mutex_type::scoped_lock _lock( i->second.mtx );
            if( i->second.status != DONE ) {
                this->m_out.put( otag, i->second.val.combine( this->m_op ) );
                i->second.status = DONE;
                return true;
            }
        }
        return false;
    }
    
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef _DIST_CNC_
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    bool reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::send_count( const OTag & otag, const typename tls_map_type::iterator & i,
                                                                                                      const int to, const bool always )
    {
        // we might have a count-put and the last value-put concurrently and all local
        // only then the owner/to can be <0 or myself
        if( to < 0 || to == CnC::tuner_base::myPid() ) return false;
        CType _cnt = i->second.nreduced.fetch_and_store( 0 );
        if( always || _cnt > 0 ) { // someone else might have transmitted the combined count already
            CnC::serializer * ser = this->new_serializer();
            (*ser) & DISTRED::GATHERCOUNT & otag & _cnt;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " send GATHERCOUNT [";
                cnc_format( oss, otag ) << "] " << _cnt << " to " << to;
            }
            this->send_msg( ser, to );
            return true;
        }
        return false;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    int reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::my_parent_for_root( const int root )
    {
        const int mpid = CnC::tuner_base::myPid();
        const int nps  = CnC::tuner_base::numProcs();
        CNC_ASSERT( root != mpid );
        int _p = ( ( ( mpid >= root ? ( mpid - root ) : ( mpid + nps - root ) ) - 1 ) / 2 ) + root;
        return _p % nps;
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::try_send_or_put_value( const OTag & otag, const typename tls_map_type::iterator & i )
    {
        if( trace_level() > 2 ) {
            Internal::Speaker oss(std::cout);
            oss << this->name() << " try_send_or_put_value [";
            cnc_format( oss, otag ) << "] nValues " << i->second.nValues << " status " << i->second.status;
        }
        if( i->second.nValues.fetch_and_decrement() == 1 ) {
            if( i->second.owner == CnC::tuner_base::myPid() ) {
                if( i->second.status == FINISH ) {
                    CNC_ASSERT( i->second.nreduced == i->second.n || i->second.n == M1 );
                    CNC_ASSERT( i->second.nValues == 0 && i->second.status == FINISH );
                    i->second.nreduced = i->second.n;
                    try_put_value( otag, i );
                }
            } else {
                CnC::serializer * ser = this->new_serializer();
                IType _val( i->second.val.combine( this->m_op ) );
                (*ser) & DISTRED::VALUE & otag & _val;
                const int to = my_parent_for_root( i->second.owner );
                if( trace_level() > 2 ) {
                    Internal::Speaker oss(std::cout);
                    oss << this->name() << " send VALUE [";
                    cnc_format( oss, otag ) << "] ";
                    cnc_format( oss, _val ) << " to " << to;
                }
                this->send_msg( ser, to );
                i->second.status = DONE;
            }
        }
    }
        
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    int reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::bcast( CnC::serializer * ser, int root ) 
    {
        const int mpid = CnC::tuner_base::myPid();
        const int nps  = CnC::tuner_base::numProcs();
        int _r1 = ( ( mpid >= root ? ( mpid - root ) : ( mpid + nps - root ) ) + 1 ) * 2 - 1;
        if( _r1 < nps ) {
            if( _r1 < nps-1 ) {
                // we have 2 children
                _r1 = (root + _r1) % nps;
                int _recvrs[2] = { _r1, (_r1+1)%nps };
                this->bcast_msg( ser, _recvrs, 2 );
                return 2;
            } else {
                // we have only a single child
                _r1 = (root + _r1) % nps;
                this->send_msg( ser, _r1 );
                return 1;
            }
        }
        delete ser;
        // we are a leaf, nothing to be sent
        return 0;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // home-grown bcast that uses a tree for each root
    // at some point something like this should go into the CnC runtime
    // returns the number of messages sent (0, 1 or 2)
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    int reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::bcast_count( const OTag & tag, const CType & val, const int root )
    {
        CnC::serializer * ser = this->new_serializer();
        (*ser) & ( val != M1 ? DISTRED::BCASTCOUNT : DISTRED::DONE ) & tag & root;
        if( val != M1 ) (*ser) & val;
        int _c = bcast( ser, root );
        if( _c && trace_level() > 2 ) {
            Internal::Speaker oss(std::cout);
            oss << this->name() << " bcast " << (val != M1 ? " BCASTCOUNT [" : " DONE [");
            cnc_format( oss, tag ) << "]";
        }
        return _c;
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::try_send_or_put_all()
    {
        CNC_ASSERT( m_alldone );
        if( trace_level() > 2 ) {
            Internal::Speaker oss(std::cout);
            oss << this->name() << " try_send_or_put_all " << m_nDones;
        }
        if( --m_nDones == 0 ) {
            if( CnC::tuner_base::myPid() == 0 ) {
                for( typename tls_map_type::iterator i = m_reductions.begin(); i != m_reductions.end(); ++i ) {
                    m_out.put( i->first, i->second.val.combine( m_op ) );
                }
            } else {
                int _n = m_reductions.size();
                CnC::serializer * ser = this->new_serializer();
                (*ser) & DISTRED::ALLVALUES & _n;
                for( typename tls_map_type::iterator i = m_reductions.begin(); i != m_reductions.end(); ++i ) {
                    IType _val( i->second.val.combine( this->m_op ) );
                    (*ser) & i->first & _val;
                }
                const int to = my_parent_for_root( 0 );
                if( trace_level() > 2 ) {
                    Internal::Speaker oss(std::cout);
                    oss << this->name() << " send ALLVALUES to " << to;
                }
                this->send_msg( ser, to );
            }
        }
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::recv_msg( serializer * ser )
    {
        char _op;
        (*ser) & _op;

        switch( _op ) {
        case DISTRED::GATHERCOUNT : {
            OTag _tag;
            CType _cnt;
            (*ser) & _tag & _cnt;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd GATHERCOUNT [";
                cnc_format( oss, _tag ) << "] " << _cnt;
            }
            m_oncount->on_gatherCount( _tag, get( _tag ), _cnt );
            break;
        }
        case DISTRED::BCASTCOUNT : {
            OTag _tag;
            CType _cnt;
            int _owner;
            (*ser) & _tag & _owner & _cnt ;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd BCASTCOUNT [";
                cnc_format( oss, _tag ) << "] " << _cnt << " " << _owner;
            }
            m_oncount->on_bcastCount( _tag, get( _tag ), _cnt, _owner );
            break;
        }
        case DISTRED::VALUE : {
            OTag _tag;
            IType _val;
            (*ser) & _tag & _val;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd VALUE [";
                cnc_format( oss, _tag ) << "] ";
                cnc_format( oss, _val );
            }
            m_ondata->on_value( _tag, get( _tag ), _val );
            break;
        }
        case DISTRED::DONE : {
            OTag _tag;
            int _owner;
            (*ser) & _tag & _owner;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd DONE [";
                cnc_format( oss, _tag ) << "] " << _owner;
            }
            m_oncount->on_done( _tag, get( _tag ), _owner );
            break;
        }
        case DISTRED::ALLDONE : {
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd ALLDONE";
            }
            m_alldone = true;
            CnC::serializer * ser = this->new_serializer();
            (*ser) & DISTRED::ALLDONE;
            m_nDones = 1000; // protect from current messages
            m_nDones += bcast( ser, 0 );
            m_nDones -= 999;
            try_send_or_put_all();
            break;
        }
        default:
        case DISTRED::ALLVALUES : {
            CNC_ASSERT( m_alldone = true );
            int _n;
            (*ser) & _n;
            if( trace_level() > 2 ) {
                Internal::Speaker oss(std::cout);
                oss << this->name() << " recvd ALLVALUES " << _n;
            }
            while( _n-- ) {
                OTag _tag;
                IType _val;
                (*ser) & _tag & _val;
                const typename tls_map_type::iterator i = get( _tag );
                m_ondata->add_value( i, _val );
            }
            try_send_or_put_all();
            break;
        }
            CNC_ASSERT_MSG( false, "Unexpected message tag in JOIN" );
        }
    }

#endif // _DIST_CNC_
        
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_item_put::on_item_put( reduce_type * r )
        : m_reduce( r )
    {}
    
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_item_put::add_value( const typename tls_map_type::iterator & i, const IType & val ) const
    {
        bool _exists;
        IType & _rval = i->second.val.local( _exists );
        _rval = m_reduce->m_op( _exists ? _rval : m_reduce->m_identity, val );
    }
 
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef _DIST_CNC_
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_item_put::on_value( const OTag & otag, const typename tls_map_type::iterator & i, const IType & val )
    {
        TRACE( "::on_value" );
        add_value( i, val );
        m_reduce->try_send_or_put_value( otag, i );
    }
#endif

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_item_put::on_put( const ITag & tag, const IType & val )
    {
        TRACE( "::on_data" );
        OTag otag;
        if( m_reduce->m_sel( tag, otag ) ) {
            typename tls_map_type::iterator i = m_reduce->get( otag );
            add_value( i, val );
            CType _n = ++i->second.nreduced;
            if( m_reduce->trace_level() > 0 ) { 
                Internal::Speaker oss(std::cout);
                oss << m_reduce->name() << " on_put [";
                cnc_format( oss, tag ) << "] for [";
                cnc_format( oss, otag ) << "] ";
                cnc_format( oss, val );
                oss << " nred now " << _n << "/" << i->second.n
#ifdef _DIST_CNC_
                    << " owner " << i->second.owner << " status " << i->second.status << " nCounts " << i->second.nCounts
#endif
                    ;
            }
#ifdef _DIST_CNC_
            // not yet done, might need some communication
            if( Internal::distributor::active() ) {
                // just in case, test if all was local
                // 2 cases
                // 1.: local phase or we are the owner
                // 2.: we know the red-count, but the reduction is not yet done.
                if( i->second.status >= CNT_AVAILABLE ) {
                    if( CnC::tuner_base::myPid() == i->second.owner ) {
                        // the owner's put was the final put, let's trigger the done propagation
                        if( _n == i->second.n && i->second.nCounts <= 0 ) m_reduce->m_oncount->on_done( otag, i, CnC::tuner_base::myPid() );
                    } else {
                        // 2nd case
                        // We have to report the new count (not the value)
                        // we use an atomic variable to count, so we do not lose contributions
                        // we don't report the value, so nothing gets inconsistent
                        // the value is transmitted only when we know nothing more comes in (elsewhere)
                        m_reduce->send_count( otag, i, i->second.owner, false );
                    }
                } // else means we are still collecting locally (case 1)
                //                        }
            } else
#endif
                m_reduce->try_put_value( otag, i );
        } else if( m_reduce->trace_level() > 0 ) { 
            Internal::Speaker oss(std::cout);
            oss << m_reduce->name() << " [";
            cnc_format( oss, tag ) << "] was not selected";
        }
    }
            
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_count_put::on_count_put( reduce_type * r )
        : m_reduce( r )
    {}

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef _DIST_CNC_
    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_count_put::on_done( const OTag & otag, const typename tls_map_type::iterator & i, const int owner )
    {
        TRACE( "::on_done" );
        if( m_reduce->trace_level() > 2 ) { 
            Internal::Speaker oss(std::cout);
            oss << m_reduce->name() << " on_done for [";
            cnc_format( oss, otag ) << "] nred now " << i->second.nreduced << "/" << i->second.n
                                    << " owner " << i->second.owner << " status " << i->second.status;
        }
        i->second.owner = owner;
        if( owner != CnC::tuner_base::myPid() || i->second.status.compare_and_swap( BCAST_DONE, CNT_AVAILABLE ) == CNT_AVAILABLE ) {
            // "forward" through bcast-tree (we are using our home-grown bcast!)
            i->second.nValues = 1000;
            i->second.nValues += m_reduce->bcast_count( otag, M1, owner );
            int _tmp = i->second.status.compare_and_swap( FINISH, BCAST_DONE );
            CNC_ASSERT( owner != CnC::tuner_base::myPid() || _tmp == BCAST_DONE );
            // we leave one more for ourself (no contribution to value)
			i->second.nValues -= 999;
            // we might be a leaf or all the values came back between the bcast and now
            m_reduce->try_send_or_put_value( otag, i );
        }
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_count_put::on_bcastCount( const OTag & otag, const typename tls_map_type::iterator & i, const CType & cnt, const int owner )
    {
        TRACE( "::on_bcast" );
        CNC_ASSERT( cnt != M1 );
        i->second.owner = owner;
        i->second.n = cnt;

        int _tmp = i->second.status.compare_and_swap( CNT_AVAILABLE, LOCAL );
        CNC_ASSERT( _tmp == LOCAL );
        // "forward" through bcast-tree (we are using our home-grown bcast!)
        i->second.nCounts = 1;
        i->second.nCounts += m_reduce->bcast_count( otag, cnt, owner );
        // if we are a leaf -> trigger gather to root
        if( --i->second.nCounts == 0 && owner != CnC::tuner_base::myPid() ) {
            m_reduce->send_count( otag, i, my_parent_for_root( owner ), true );
        }
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_count_put::on_gatherCount( const OTag & otag, const typename tls_map_type::iterator & i, const CType & cnt )
    {
        TRACE( "::on_gather" );
        CNC_ASSERT( cnt != M1 );
        i->second.nreduced += cnt;
        if( m_reduce->trace_level() > 2 ) { 
            Internal::Speaker oss(std::cout);
            oss << m_reduce->name() << " on_gatherCount [";
            cnc_format( oss, otag ) << "] now " << i->second.nreduced << "/" << i->second.n << " nCounts " << i->second.nCounts;
        }
        // at root, we might need to trigger done phase
        if( --i->second.nCounts <= 0 ) { // there might be extra counts
            if( i->second.owner == CnC::tuner_base::myPid() ) {
                if( i->second.n == i->second.nreduced ) {
                    on_done( otag, i, CnC::tuner_base::myPid() );
                }
            } else {
                CNC_ASSERT( i->second.nCounts == 0 ); // extra counts occur only on root
                m_reduce->send_count( otag, i, my_parent_for_root( i->second.owner ), true );
            }
        }
    }
#endif // _DIST_CNC_

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename ITag, typename IType, typename ITuner, typename CType, typename CTuner, typename OTag, typename OTuner, typename ReduceOp, typename Select >
    void reduction< ITag, IType, ITuner, CType, CTuner, OTag, OTuner, ReduceOp, Select >::on_count_put::on_put( const OTag & otag, const CType & cnt )
    {
        TRACE( "::on_cnt" );
        typename tls_map_type::iterator i = m_reduce->get( otag );
                
#ifdef _DIST_CNC_
        if( Internal::distributor::active() ) {
            if( cnt != M1 ) { //is this a normal count?
                i->second.n = cnt;
                // just in case all was local, we try to finish
                if( ! m_reduce->try_put_value( otag, i ) ) {
                    on_bcastCount( otag, i, cnt, CnC::tuner_base::myPid() );
                }
            } else { // this is the done flag
                // we have no idea what was put remotely
                // -> we trigger the final gather phase
                int _tmp = i->second.status.compare_and_swap( CNT_AVAILABLE, LOCAL );
                CNC_ASSERT( _tmp == LOCAL );
                i->second.nCounts = 0;
                on_done( otag, i, CnC::tuner_base::myPid() );
            }
        } else
#endif
            {
                if( cnt != M1 ) i->second.n = cnt;
                else i->second.n = i->second.nreduced;
                m_reduce->try_put_value( otag, i );
            }
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

} // namespace CnC

/// @}

#endif //_CnC_REDUCE_H_
