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
#include <fstream>

// Count the number of occurances of given strings in a file.  An
// example of using a pre-defined CnC::graph, e.g. a reduction.
//
// The file is read line by line (on the host).  A computation unit is
// tagged by the pair (line-id/block, search-string).  Such a
// computation gets the line/block to search in.  It looks for the
// string and puts the number of matches into a collection. The
// collection is part of a reduction.
//
// The input to the reduction is counts_per_block with the matched
// string as key/above pair as its tag. The selector (a lambda!) maps
// the input-pair to the string that is matched. The reduction also
// takes "counts" which provides the number of participating elements
// per reduction (for each string one reduction!).  The reduction's
// output is the overall count per string.
//
// This is not optimized in any way. To make this fast, we need to
// adjust the block size and read in parallel. Additionally,
// std::strings are used in a very memory-unfriendly and
// copy-intensive manner.
//
// On distributed memory an intelligent distribution plan
// would probably be helpful.

typedef std::pair< int, std::string > tag_type;

struct cw_context;

struct counter
{
    int execute( const tag_type & t, cw_context & ctxt ) const;
};

struct cw_context : public CnC::context< cw_context >
{
    // the prescribing tags
    CnC::tag_collection< tag_type > tags;
    // holds the lines/blocks of the file(s)
    CnC::item_collection< int, std::string > blocks;
    // for each pair(block,string) we put the number of occurances of string found in block
    CnC::item_collection< tag_type, size_type > counts_per_block;
    // the final number of occurances per string
    CnC::item_collection< std::string, size_type > counts;
    // the number of items (e.g. counts per string) participating in a reduction
    CnC::item_collection< std::string, size_type > red_counts;
    CnC::step_collection< counter > steps;
    // our reduction is provided as a graph, see constructor for its instantiation&wiring
    CnC::graph * reduce;
    
    cw_context()
        : tags( *this, "tags" ),
          blocks( *this, "blocks" ),
          counts_per_block( *this, "counts_per_block" ),
          counts( *this, "counts" ),
          red_counts( *this, "red_counts" ),
          steps( *this, "counter" ),
          reduce( NULL )
    {
        // here we wire the graph/reduction
        reduce = CnC::make_reduce_graph( *this,            // context
                                         "reduce",         // name
                                         counts_per_block, // input collection
                                         red_counts,       // number of items per reduction
                                         counts,           // the final result for each reduction
                                         std::plus<size_type>(), // the reduction operation
                                         size_type(0),     // identity element
                                         // we use a lambda as the selector
                                         // it maps the item to the reduction identified by t.second (the string)
                                         // e.g. it reduces over all blocks
                                         []( const tag_type & t, std::string & _s )->bool{_s=t.second;return true;} );
        tags.prescribes( steps, *this );
        steps.consumes( blocks );
        steps.produces( counts_per_block );
        //CnC::debug::trace( *reduce, 3 );
    }
    
    ~cw_context()
    {
        delete reduce;
    }
};

// get block/line (tag.first) and determine number of occurances of string (tag.second)
// put result into counts_per_block
int counter::execute( const tag_type & t, cw_context & ctxt ) const
{
    std::string _str;
    ctxt.blocks.get( t.first, _str );
    const size_type _sz = _str.size();
    size_type _pos = -1;
    size_type _cnt = 0;
    while( ( _pos = _str.find( t.second, _pos+1 ) ) < _sz ) ++_cnt;
    // always to results, even if 0
    // this allows us to determine the number of reduced items in main
    ctxt.counts_per_block.put( t, _cnt );
    return 0;
}

int main( int argc, char * argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< cw_context > _dc;
#endif
    
    if( argc < 3 ) {
        std::cerr << "expected arguments: <file> <word1> [<word2>...]\n";
            exit(1);
    }
    // create the context
    cw_context ctxt;
    std::ifstream _file( argv[1] );
    std::string _block;
    int _id = 0;
    // read line line
    while( std::getline( _file, _block ) ) {
        ctxt.blocks.put( _id, _block );
        int i = 1;
        // for each line, put control for each searched string
        while( ++i < argc ) {
            ctxt.tags.put( std::make_pair( _id, argv[i] ) );
        }
        ++_id;
    }

    // reduction needs number of reduced items
    // as we always put a number in the step, we know its #blocks
    // _id holds #blocks
    int i = 1;
    while( ++i < argc ) ctxt.red_counts.put( argv[i], _id );

    ctxt.wait();

    std::cout << "done" << std::endl;

    std::cout << ctxt.counts.size() << " " << ctxt.blocks.size() << std::endl;
    // iterate and write out all keys/value pairs
    for( auto i = ctxt.counts.begin(); i != ctxt.counts.end(); ++i ) {
        std::cout << i->first << " \t " << *i->second << std::endl;
    }
    ctxt.wait();
    std::cout << ctxt.counts.size() << " " << ctxt.blocks.size() << std::endl;

    return 0;
}
