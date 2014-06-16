//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

#ifndef _H_PIPELINE_INCLUDED_H_
#define _H_PIPELINE_INCLUDED_H_

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>

#include <cassert>
#include <cstdio> // sprintf
#include <vector>

// for the sake of simplicity we make all identifiers ints
typedef int id_type;

// input and output data needs to be tagged by the pipeline/stream and the producing pipeline stage 
typedef std::pair< id_type, id_type > item_tag_type;

// type of items, might be anything
typedef std::shared_ptr< const std::vector< double > > item_type;

// forward declare the context
struct pl_context;

// a generic stage of a pipeline
// you can create as many of those as you want, just make sure each of them get's a unique id
// to keep it siple, stage i will for every tag x consume (i-1,x) and produce (i,x)
struct pl_stage : public CnC::hashmap_tuner, public CnC::step_tuner<>
{
    typedef enum { STREAM, STAGE, BLOCK } part_type;
    pl_stage( int id = -1, int nStages = 0, int nStreams = 0, part_type p = STREAM );

    int execute( const id_type & tag, pl_context & c ) const;
    int m_id;
    // tuning
    int get_count( const item_tag_type & ) const;
    int get_count( const id_type & ) const;
    int compute_on( const id_type & stream, const id_type & stage ) const;
    int compute_on( const id_type & /*tag*/, pl_context & /*arg*/ ) const;
    int consumed_on( const item_tag_type & /*tag*/ ) const;
    int consumed_on( const id_type & /*tag*/ ) const { return CnC::CONSUMER_UNKNOWN; }
    int produced_on( const id_type & /*tag*/ ) const;
    int produced_on( const item_tag_type & /*tag*/ ) const { assert( 0 ); return CnC::PRODUCER_UNKNOWN; }
    template< typename T >
    void depends( const id_type & tag, pl_context & c, T & dc ) const;

    int m_stages;
    int m_streams;
    int m_nx, m_ny;
    part_type m_part;
};
CNC_BITWISE_SERIALIZABLE( pl_stage );

struct pl_context : public CnC::context< pl_context >
{
    typedef CnC::step_collection< pl_stage, pl_stage > step_coll_type;

    // constructing a pipeline requries the number of stages
    pl_context( int nstages = 0, int nstreams = 0, int n = 0, pl_stage::part_type pt = pl_stage::STREAM )
        : m_nStages( nstages ),
          m_nStreams( nstreams ),
          m_n( n ),
          m_tuner( -1, nstages, nstreams, pt ),
          m_stages(),
          m_stepColls(),
          m_tagColl( *this ),
          m_streamData( *this, m_tuner ),
          m_stageData( *this, m_tuner )
    {
        if( m_nStages > 0 ) {
            init();
        }
    }

    ~pl_context()
    {
        while( ! m_stepColls.empty() ) {
            delete m_stepColls.back();
            m_stepColls.pop_back();
        }
    }

    void init()
    {
        assert( m_nStages && m_nStages );
        m_stages.reserve( m_nStages ); // needed as we require the reference of its entries
        m_stepColls.reserve( m_nStages );
        char _buff[32];
        // now create the pipeline
        // first id is 1, 0 means env
        for( int i = 1; i <= m_nStages; ++i ) {
            m_stages.push_back( pl_stage( i, m_nStages, m_nStreams, m_tuner.m_part ) );
            sprintf( _buff, "stage_%d", i );
            step_coll_type * _sc = new step_coll_type( *this, _buff, m_stages.back(), m_stages.back() );
            m_tagColl.prescribes( *_sc, *this );
            _sc->consumes( m_streamData );
            _sc->consumes( m_stageData );
            _sc->produces( m_streamData );
            m_stepColls.push_back( _sc );
        }
//         if( CnC::Internal::distributor::myPid() == 2 ) {
//             CnC::debug::trace( m_stageData );
//             CnC::debug::trace( m_tagColl );
//         }
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_nStages & m_nStreams & m_n & m_tuner;
        if( ser.is_unpacking() ) init();
    }
#endif

    int                                               m_nStages;
    int                                               m_nStreams;
    int                                               m_n;
    pl_stage                                          m_tuner;
    std::vector< pl_stage >                           m_stages;
    std::vector< step_coll_type * >                   m_stepColls;
    CnC::tag_collection< id_type >                    m_tagColl;
    CnC::item_collection< item_tag_type, item_type, pl_stage >  m_streamData;
    CnC::item_collection< id_type, item_type, pl_stage >        m_stageData;
}; 

#include "pl_tuner.h"

#endif // _H_PIPELINE_INCLUDED_H_
