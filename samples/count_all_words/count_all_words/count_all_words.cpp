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

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cnc/reduce.h>
#include <cassert>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>

// Count the number of occurances of all words in a file.  An example
// of using a pre-defined CnC::graph, e.g. a reduction.
//
// The file is read line by line (on the host).  A computation unit is
// tagged by the line-id/block.  Such a computation gets the
// line/block to search in.  It reads word by word and puts an item
// into an item-collection "counts_per_block". To make the tag unique,
// its a triple (block/line, word#, word) [case-insensitive].  The
// data itself is just a dummy.  The collection is part of a reduction
// which will finally compute the number of occurances per word.
//
// The input to the reduction is counts_per_block. The selector (a
// lambda!) maps the input-triple to the word that is matched. The
// reduction also takes "counts". As we don't know which words we will
// find and count, there is no way to know the number of participating
// items per reduction.  Instead we have to indicate when all lines
// have been processed.  After a first call to wait, we know no more
// computation is done, we then call reduction::flush to indicate that
// no more input arrives.  A second wait() makes sure that the
// reduction completes before we access the result.  The reduction's
// output is the overall count for ever word in the file.
//
// As an opimization, we assign a get-count of 0 to all words we put.
// In this example the words are used for nothing else than the
// reduction.  With a get-couhjnt of 0 the items will be passed to the
// reduction but never really stored.
//
// This is not optimized in any way. To make this fast, we need to
// adjust the block size and read in parallel. Additionally,
// std::strings are used in a very memory-unfriendly and
// copy-intensive manner.
//
// On distributed memory an intelligent distribution plan would
// probably be helpful.

typedef long long int id_type;
typedef std::pair< id_type, const std::string > tag_type;

struct cw_context;

struct counter
{
    int execute( const int & t, cw_context & ctxt ) const;
};

struct gc0_tuner : public CnC::hashmap_tuner
{
    template< typename Tag >
    int get_count( const Tag & ) const
    {
        return 0;
    }
};

struct cw_context : public CnC::context< cw_context >
{
    // prescribes our computation
    CnC::tag_collection< int > tags;
    // one input line per item
    CnC::item_collection< int, std::string > blocks;
    // every word instance we find we put in here with a unique tag
    CnC::item_collection< tag_type, size_type, gc0_tuner > counts_per_block;
    // we don't really need this, we use flush
    CnC::item_collection< std::string, int > cnt;
    // the resulting count per word
    CnC::item_collection< std::string, size_type > counts;
    // extracing words
    CnC::step_collection< counter > steps;
    // the reduction sub-graph
    CnC::graph * reduce;
    
    cw_context()
        : tags( *this, "tags" ),
          blocks( *this, "blocks" ),
          counts_per_block( *this, "counts_per_block" ),
          cnt( *this, "cnt" ),
          counts( *this, "counts" ),
          steps( *this, "counter" ),
          reduce( NULL )
    {
        reduce = CnC::make_reduce_graph( *this, "reduce",
                                         counts_per_block, //input collection
                                         cnt,              // counts per reduce, not used
                                         counts,           // result
                                         std::plus<size_type>(), // reduce operation
                                         size_type(0),     // identity element
                                         // we use a lambda as the selector
                                         // it maps the item to the reduction identified by t.second (the string)
                                         // e.g. it reduces over all blocks
                                         []( const tag_type & t, std::string& _s )->bool{_s=t.second;return true;} );
        tags.prescribes( steps, *this );
        steps.consumes( blocks );
        steps.produces( counts_per_block );
        //        CnC::debug::trace_all( *this );
        //CnC::debug::trace( steps );
        //        CnC::debug::trace( *reduce, 2 );
    }
    
    ~cw_context()
    {
        delete reduce;
    }
};

// get the line, read word by word and put each instance
// with unique tag into counts_per_block, which is the
// input collection of the reduction
int counter::execute( const int & t, cw_context & ctxt ) const
{
    std::string _str;
    ctxt.blocks.get( t, _str );
    std::istringstream iss( _str );
    std::string _word;
    id_type _id = (t << 16);
    while( iss >> _word ) {
        // let's a do a case-insensitive matching
        std::transform(_word.begin(), _word.end(), _word.begin(), ::tolower);
        ctxt.counts_per_block.put( tag_type(_id, _word), 1 );
        ++_id;
    }
    return 0;
}

int main( int argc, char * argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< cw_context > _dc;
#endif
    
    if( argc < 2 ) {
        std::cerr << "expected arguments: <file>\n";
            exit(1);
    }
    // create the context
    cw_context ctxt;
    std::ifstream _file( argv[1] );
    std::string _block;
    int _id = 0;

    tbb::tick_count startTime = tbb::tick_count::now();
    
    // read line line
    while( std::getline( _file, _block ) ) {
        // put each line
        ctxt.blocks.put( _id, _block );
        // and control to start searching/matching
        ctxt.tags.put( _id );
        ++_id;
    }
    // A reduction needs number of reduced items.
    // We do not know the count in advance.
    // We do not even know which words are going to show up.
    // We have to wait until processing is done
    // and then tell the reduction it can do the final pass
    ctxt.wait();
    ctxt.reduce->flush();
    ctxt.wait();

    tbb::tick_count endTime = tbb::tick_count::now();
    
    std::cout << ctxt.counts.size() << std::endl;
    // iterate and write out all keys/value pairs with counts > 999
    for( auto i = ctxt.counts.begin(); i != ctxt.counts.end(); ++i ) {
        if( *i->second > 999 )
            std::cout << i->first << " \t " << *i->second << std::endl;
    }

    std::cout << "Time: " << (endTime-startTime).seconds() << "s.\n";
    return 0;
}
