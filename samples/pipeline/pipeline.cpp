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

#include "pipeline.h"
#include <cnc/debug.h>
#include <cstdlib> // atoi
#include <iostream>
#include <tbb/tick_count.h>

// each stage of the pipeline reads input data, computes and the writes the output
// we fake computation for now
int pl_stage::execute( const id_type & tag, pl_context & c ) const
{
    item_type _item1, _item2;
    c.m_stageData.get( m_id, _item1 );
    if( m_id == 1 ) {
        _item2 = std::make_shared< std::vector< double > >( c.m_n );
    } else {
        c.m_streamData.get( item_tag_type( m_id-1, tag ), _item2 );
    }
    // dummy computation
    double _tmp = tag + 8;
    for( int i = 0; i < _item2->size(); ++i ) {
        for( int j = 0; j < _item1->size(); ++j ) {
            _tmp *= sin( (*_item1)[j] * (*_item2)[i] );
            if( _tmp != _tmp /*isnan*/ ) _tmp = 1.9284576 * (*_item1)[j];
        }
        const_cast< double & >( (*_item2)[i] ) = _tmp;
    }
    c.m_streamData.put( item_tag_type( m_id, tag ), _item2 );
    return CnC::CNC_Success;
}

int main( int argc, char * argv[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< pl_context > dc_init;
#endif
    if( argc != 6 ) {
        std::cerr << "Expecting 4 arguments: <number of stages> <number of pipelines> <number of doubles along each pipeline> <number of doubles per stage> <partitioning:[p,s,b]>\n";
        exit( 4711 );
    }
    int _nstages = atoi( argv[1] );
    int _nstreams = atoi( argv[2] );
    int _nps = atoi( argv[3] );
    int _npp = atoi( argv[4] );
    pl_stage::part_type _pt;
    switch( argv[5][0] ) {
        case 'p':
            _pt = pl_stage::STREAM; break;
        case 's':
            _pt = pl_stage::STAGE; break;
        case 'b':
            _pt = pl_stage::BLOCK; break;
        default:
            std::cerr << "wrong value in partitioning argument (" << argv[5] << "); expected p, s, or b\n"; exit( 12 );
        }
    
    // create context
    pl_context _ctxt( _nstages, _nstreams, _npp, _pt );
    CnC::debug::collect_scheduler_statistics( _ctxt );

    tbb::tick_count t1 = tbb::tick_count::now();

    item_type _item1, _item2;
    // put the input data and tags
    for( int i = 1; i <= _nstages; ++i ) {
        _ctxt.m_stageData.put( i, std::make_shared< std::vector< double > >( _nps, i ) );
    }
    //    CnC::parallel_for( 1, _nstreams, 1, [&_ctxt,_npp]( int i ) {
    for( int i = 1; i <= _nstreams; ++i ) {
            _ctxt.m_tagColl.put( i );
    };//, false );

    // now just wait for the eval to finish
    _ctxt.wait();
    
    tbb::tick_count t2 = tbb::tick_count::now();

    for( int i = 1; i <= _nstreams; ++i ) {
        item_type _tmp;
        _ctxt.m_streamData.get( item_tag_type( _nstages, i ), _tmp );
    }
    
    std::cout << "Stages: " << _nstages << " streams: " << _nstreams << " time: " << (t2-t1).seconds() << " sec\n";
    return 0;
}
