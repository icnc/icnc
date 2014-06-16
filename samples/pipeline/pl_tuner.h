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

#ifndef _H_PIPELINE_TUNER_INCLUDED_H_
#define _H_PIPELINE_TUNER_INCLUDED_H_

pl_stage::pl_stage( int id, int nStages, int nStreams, part_type p )
    : m_id( id ),
      m_stages( nStages ),
      m_streams( nStreams ),
      m_part( p ),
      m_nx( numProcs() ),
      m_ny( 1 )
{
    if( m_part == BLOCK ) {
        while( m_nx % 2 == 0 && m_nx > m_ny ) {
            m_nx /= 2;
            m_ny *= 2;
        }
        assert( m_nx * m_ny == numProcs() );
    }
}

int pl_stage::get_count( const item_tag_type & tag ) const
{
    return tag.first < m_stages ? 1 : CnC::NO_GETCOUNT;
}

int pl_stage::get_count( const id_type & tag ) const
{
    return m_streams;
}

int pl_stage::compute_on( const id_type & stage, const id_type & stream ) const
{
    if( m_part == STREAM ) return stream % numProcs(); //(stream-1) * numProcs() / m_streams;
    if( m_part == STAGE ) return stage % numProcs(); //(stage-1) * numProcs() / m_stages;
    return (stream-1) * m_nx / m_streams + ( (stage-1) * m_ny / m_stages ) * m_nx;
}

int pl_stage::compute_on( const id_type & tag, pl_context & ) const
{
    return compute_on( m_id, tag );
}

int pl_stage::consumed_on( const item_tag_type & tag ) const
{
    if( tag.first == m_stages ) return 0; // "result" for env 
    return compute_on( tag.first + 1, tag.second );
}

int pl_stage::produced_on( const id_type & /*tag*/ ) const
{
    return 0;
}

template< typename T >
void pl_stage::depends( const id_type & tag, pl_context & c, T & dc ) const
{
    if( m_id != 1 ) {
        dc.depends( c.m_streamData, item_tag_type( m_id-1, tag ) );
    }
    if( numProcs() > 1 ) {
        dc.depends( c.m_stageData, m_id );
    }
}

#endif // _H_PIPELINE_TUNER_INCLUDED_H_
