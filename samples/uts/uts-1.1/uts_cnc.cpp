/*
 *         ---- The Unbalanced Tree Search (UTS) Benchmark ----
 *  
 *  Copyright (c) 2010 See AUTHORS file for copyright holders
 *
 *  This file is part of the unbalanced tree search benchmark.  This
 *  project is licensed under the MIT Open Source license.  See the LICENSE
 *  file for copyright and licensing information.
 *
 *  UTS is a collaborative project between researchers at the University of
 *  Maryland, the University of North Carolina at Chapel Hill, and the Ohio
 *  State University.  See AUTHORS file for more information.
 *
 */
//********************************************************************************
// Copyright (c) 2010-2021 Intel Corporation. All rights reserved.              **
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#include <cstdlib>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cnc/itac.h>
#include <cassert>

#include "uts.h"

/***********************************************************
 *  UTS Implementation Hooks                               *
 ***********************************************************/

// Return a string describing this implementation
const char * impl_getName() {
    return "CnC";
}


// construct string with all parameter settings 
int impl_paramsToStr(char *strBuf, int ind) {
    ind += sprintf(strBuf+ind, "Execution strategy:  ");
    ind += sprintf(strBuf+ind, "Parallel search using threads\n" ); //FIXME print # threads
    return ind;
}

int impl_parseParam(char *param, char *value) {
    int err = 0;  // Return 0 on a match, nonzero on an error

    switch (param[1]) {
    default:
        err = 1;
        break;
    }

    return err;
}

void impl_helpMessage() {
    printf("   none.\n");
}

void impl_abort(int err) {
    exit(err);
}

/***********************************************************
 *                                                         *
 *  FUNCTIONS                                              *
 *                                                         *
 ***********************************************************/

/*
 *	Tree Implementation      
 *
 */
void initNode(Node * child)
{
    child->type = -1;
    child->height = -1;
    child->numChildren = -1;    // not yet determined
}

/***********************************************************
 *                                                         *
 *  CnC bits                                               *
 *                                                         *
 ***********************************************************/

typedef unsigned long long int auto_id;

static auto_id id_gen()
{
    static std::atomic< unsigned int > g_cnt;
    return ( ( (auto_id)CnC::Internal::distributor::myPid() ) << (sizeof(auto_id)*4) ) + ( (auto_id)(++g_cnt) );
}

struct uts_context;
struct stats_t
{
    stats_t() : numNodes( 0 ), numLeafs( 0 ), maxDepth( 0 ) {}
    int numNodes, numLeafs, maxDepth;
};

CNC_BITWISE_SERIALIZABLE( stats_t );
CNC_BITWISE_SERIALIZABLE( Node );

struct uts_step
{
    int execute( const Node & parent, uts_context & ctxt, int sl, int & num, stats_t & stats ) const;
    int execute( const Node & parent, uts_context & ctxt ) const
    {
        stats_t _tmp;
        int _num( 0 );
        return execute( parent, ctxt, -1, _num, _tmp );
    }
};

template <>
class cnc_tag_hash_compare< Node > {
  public:
    static size_t hash(const Node& p)
    {
        const int * _p = reinterpret_cast< const int * >( & p );
        return size_t( ( _p[0] << 3 ) + ( _p[1] << 7 ) + ( _p[2] << 11 ) + ( _p[4] << 17 ) + ( _p[5] << 9 ) + ( _p[7] << 13 ) );
    }
       
    bool equal(const Node & a, const Node & b) const { return memcmp( &a, &b, sizeof( a ) ) == 0; }
};

struct uts_context : public CnC::context< uts_context >
{
    uts_context()
        : m_steps( *this ), 
          m_tags( *this ),
          m_stats( *this )
    {
        m_tags.prescribes( m_steps, *this );
        //        CnC::debug::collect_scheduler_statistics( *this );
    }
    CnC::step_collection< uts_step > m_steps;
    CnC::tag_collection< Node > m_tags;
    CnC::item_collection< auto_id, stats_t > m_stats;
};

/* 
 * Generate all children of the parent
 *
 * details depend on tree type, node type and shape function
 *
 */
int uts_step::execute( const Node & tag, uts_context & ctxt, int sl, int & _num, stats_t & stats ) const
{
    //    VT_FUNC( "uts" );
    //    std::cerr << "uts\n" << std::flush;
    Node parent( tag );
    int childType;
    Node child;
    if( sl == -1 ) {
        sl = parent.height;
    }
    
    /* template for children */
    initNode( &child );
    
    // FIXME record number of children in parent (item_coll)
    parent.numChildren = uts_numChildren(&parent);
    childType   = uts_childType(&parent);

    ++stats.numNodes;

    // construct children and push onto stack
    if( parent.numChildren > 0 ) {
        child.type = childType;
        child.height = parent.height + 1;
        int diff = parent.height - sl;
        if( diff > stats.maxDepth ) stats.maxDepth = diff;

        for( int i = 0; i < parent.numChildren; i++ ) {
            for( int j = 0; j < computeGranularity; j++ ) {
                // TBD:  add parent height to spawn
                // computeGranularity controls number of rng_spawn calls per node
                rng_spawn(parent.state.state, child.state.state, i);
            }
            if( ( parent.height > 4 && _num < 10000 ) || i >= parent.numChildren - 1 ) {
                execute( child, ctxt, sl, _num, stats );
            } else  {
                ctxt.m_tags.put( child );
            }
        }
    } else {
        ++stats.numLeafs;
    }
    ++_num;
    if( sl == parent.height ) ctxt.m_stats.put( id_gen(), stats );
    return 0;
}

/*  Main() function for: Sequential, OpenMP, UPC, and Shmem
 *
 *  Notes on execution model:
 *     - under openMP, global vars are all shared
 *     - under UPC, global vars are private unless explicitly shared
 *     - UPC is SPMD starting with main, OpenMP goes SPMD after
 *       parsing parameters
 */
int main(int argc, char *argv[])
{
    uts_parseParams(argc, argv);
#ifdef _DIST_
    CnC::dist_cnc_init< uts_context > _dinit;
#endif
    uts_printParams();
    
    Node root;
    double t1, t2, et;
    uts_context ctxt;
    
    /* initialize root node and push on thread 0 stack */
    uts_initRoot(&root, type);
    
    t1 = uts_wctime();
    /* start with root */
    ctxt.m_tags.put( root );
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ctxt.wait();
    t2 = uts_wctime();
    et = t2 - t1;
    
    int numNodes( 0 ), numLeafs( 0 ), maxDepth( 0 );
    for(  CnC::item_collection< auto_id, stats_t >::const_iterator it = ctxt.m_stats.begin(); it != ctxt.m_stats.end(); ++it ) {
        numNodes += it->second->numNodes;
        numLeafs += it->second->numLeafs;
        if( maxDepth < it->second->maxDepth ) maxDepth = it->second->maxDepth;
    }
    uts_showStats( 1, -1, et, numNodes, numLeafs, maxDepth );

    return 0;
}
