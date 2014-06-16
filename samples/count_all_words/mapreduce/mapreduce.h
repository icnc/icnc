// *******************************************************************************
// Copyright (c) 2013-2014 Intel Corporation. All rights reserved.              **
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
// *******************************************************************************

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
#include <cwctype>

/**
   Map-Reduce for CnC.
   
   The mapreduce_context provides a functionality similar to hadoop/mapreduce.
   At instantiation time it accepts the map- and reduce operations.
   Input data is accepted by putting files into its read_data collection.
   A call to wait() makes sure all mapreduce operations are done.
   Output data is stored in the "results" collection and can then be accessed
   through get-calls or iterators.

   Like in common mapreduce frameworks the map operation works on streams - not
   on CnC entities. Also like in mapreduce frameworks the result of the map
   is stored in a collection (of course here we use a CnC item collection).

   It supports an easy way to distribute computation on distributed memory.
   By appending':<processid>' to the input filenames it will execute the corresponding
   map operation on the given process '<processid>'. The reduction part is 
   automatically distributed.

   Internally we use a step for reading files, a step for mapping and
   a reduction graph for the reduce part.

   \todo write a driver for HPFS which provides the distribution information (appending':<processid>')
   \todo what about making better use of blocks (not entire files)?
   \todo what about making this a CnC::graph?
**/
template< typename MapOp, typename ReduceOp >
struct mapreduce_context : public CnC::context< mapreduce_context< MapOp, ReduceOp > >
{
    typedef std::pair< const std::string, long long int > map_tag_type;
    typedef typename MapOp::result_type result_type;
    typedef typename MapOp::key_type key_type;
    typedef std::string char_ptr_type;

    // the step which executes the map operation
    struct mapper
    {
        int execute( const map_tag_type & t, mapreduce_context< MapOp, ReduceOp > & ctxt ) const;
    };
    
    // the step which reads individual files
    struct reader 
    {
        int execute( const std::string & file, mapreduce_context< MapOp, ReduceOp > & ctxt ) const;
    };
    
    // a generic tuner providing constant get-count
    // and executes locally (e.g. whereever the step gets prescribed)
    template< int GC >
    struct gc_tuner : public CnC::hashmap_tuner
    {
        template< typename Tag >
        int get_count( const Tag & ) const
        {
            return GC;
        }
        template< typename Tag >
        int consumed_on( const Tag & ) const
        {
            return CnC::CONSUMER_LOCAL;
        }
        template< typename Tag >
        int produced_on( const Tag & ) const
        {
            return CnC::PRODUCER_LOCAL;
        }
    };

    // tuner for reading files, distributes file-reads
    struct read_tuner : public CnC::step_tuner< false >
    {
        // we analyze the input string and search for ':<pid>' prefix.
        // if found, the corresponding step will be executed on process <pid>.
        // Otherwise we distribute randomly.
        int compute_on( const std::string & file, mapreduce_context< MapOp, ReduceOp > & ) const
        {
            auto _col = file.rfind( ':' );
            if( _col == std::string::npos ) {
                cnc_hash< std::string > _h;
                int _x = _h( file ) % CnC::tuner_base::numProcs();
                return _x;
            }
            // manually convert to int (faster)
            const char * _p = file.c_str() + _col + 1;
            int _x = 0;
            while( *_p >= '0' && *_p <= '9' ) {
                _x = ( _x * 10 ) + ( *_p - '0' );
                ++_p;
            }
            return _x % CnC::tuner_base::numProcs();
        }
    };

    // the mapping stays local
    struct map_tuner : public CnC::step_tuner< false >
    {
        int compute_on( const map_tag_type &, mapreduce_context< MapOp, ReduceOp > & ) const
        {
            return CnC::COMPUTE_ON_LOCAL;
        }
    };

    // prescribes reading files
    CnC::tag_collection< std::string > read_tags = { *this, "read_tags" }; // need recent g++ or icpc
    // prescribes our computation
    CnC::tag_collection< map_tag_type > map_tags = { *this, "map_tags" }; // need recent g++ or icpc
    // one input line per item
    CnC::item_collection< map_tag_type, char_ptr_type, gc_tuner< 1 > > blocks = { *this, "blocks" }; // need recent g++ or icpc
    // every word instance we find we put in here with a unique tag
    CnC::item_collection< std::string, result_type, gc_tuner< 0 > > map_out = { *this, "map_out" }; // need recent g++ or icpc
    // we don't really need this, we use flush
    CnC::item_collection< key_type, int > cnt = { *this, "cnt" }; // need recent g++ or icpc
    // the results
    CnC::item_collection< key_type, result_type/*, OTuner*/ > results = { *this, "results" }; // need recent g++ or icpc
    // reading files
    CnC::step_collection< reader, read_tuner > read_steps = { *this, "reader" }; // need recent g++ or icpc
    // extracing words
    CnC::step_collection< mapper, map_tuner > map_steps = { *this, "mapper" }; // need recent g++ or icpc
    // the reduction sub-graph
    CnC::graph * reduce;
    // our private instance/copy of the map operation
    MapOp m_mapOp;
    // our private instance/copy of the reduce operation
    ReduceOp m_reduceOp;
    
    // we need a default constructor on distributed memory
    // the combination with serialize must match what the parametrized constructor does.
    mapreduce_context()
        : reduce( NULL ),
          m_mapOp(),
          m_reduceOp()
    {
        read_tags.prescribes( read_steps, *this );
        map_tags.prescribes( map_steps, *this );
        read_steps.produces( blocks );
        map_steps.consumes( blocks );
        map_steps.produces( map_out );
    }

    // this is the cosntructor the user actually sees/uses:
    // it provides the map- and reduce operations
    mapreduce_context( const MapOp & mo, const ReduceOp & ro )
        : mapreduce_context< MapOp, ReduceOp >()
    {
        m_mapOp = mo;
        m_reduceOp = ro;
        init_reduce();
    }
    
    // initing the reduce graph
    // called by serialize (unpack) and the programmer-used constructor
    void init_reduce() 
    {
        delete reduce;
        reduce = CnC::make_reduce_graph( *this, "reduce",
                                         map_out, //input collection
                                         cnt,           // counts per reduce, not used
                                         results,       // result
                                         m_reduceOp,    // reduce operation
                                         size_type(0),  // identity element
                                         // we use a lambda as the selector
                                         // it maps the item to the reduction identified by t.second (the string)
                                         // e.g. it reduces over all blocks
                                         []( const std::string & t, std::string& _s )->bool{_s=t;return true;} );
    }

    ~mapreduce_context()
    {
        delete reduce;
    }

    // we overwrite the normal wait to make it even easier
    // in some cases this might not be desired, but we don't care for now.
    void wait() 
    {
        // A reduction needs number of reduced items.
        // We do not know the count in advance.
        // We do not even know which words are going to show up.
        // We have to wait until processing is done
        // and then tell the reduction it can do the final pass.
        CnC::context< mapreduce_context< MapOp, ReduceOp > >::wait();
        reduce->flush();
        CnC::context< mapreduce_context< MapOp, ReduceOp > >::wait();
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_mapOp & m_reduceOp;
        if( ser.is_unpacking() ) init_reduce();
    }
#endif
};

// function templates are easier to use the template classes
// let's define a function which returns the right context type by
// just providing the operations.
template< typename MapOp, typename ReduceOp >
mapreduce_context< MapOp, ReduceOp > * make_mapreduce( const MapOp & mo, const ReduceOp & ro )
{
    return new mapreduce_context< MapOp, ReduceOp >( mo, ro );
}

// get the line, read word by word and put each instance
// with unique tag into map_out, which is the
// input collection of the reduction
template< typename MapOp, typename ReduceOp >
int mapreduce_context< MapOp, ReduceOp >::mapper::execute( const map_tag_type & t, mapreduce_context< MapOp, ReduceOp > & ctxt ) const
{
    char_ptr_type _str;
    ctxt.blocks.get( t, _str );
    std::istringstream iss( _str );
    ctxt.m_mapOp( iss, ctxt.map_out );
    return 0;
}

// read the given file and trigger map operations
template< typename MapOp, typename ReduceOp >
int mapreduce_context< MapOp, ReduceOp >::reader::execute( const std::string & file, mapreduce_context< MapOp, ReduceOp > & ctxt ) const
{
    const int MIN_BLK_SZ = 1024;

    std::string _fn( file );
    auto _col = file.rfind( ':' );
    if( _col != std::string::npos ) _fn.resize( _col );
    std::ifstream _file( _fn );
    if( ! _file ) {
        std::cerr << "Could not open file " << _fn << std::endl;
        return CnC::CNC_Failure;
    }

    long long int _id = 0;
    int _first = 0;
    char_ptr_type _block; //( new char[MIN_BLK_SZ], std::default_delete<char[]>() );
    _block.resize( MIN_BLK_SZ );
    // we read blocks of data
    // as a block might be in the middle of a word, we need some extra stuff
    // e.g. we find the last whitespace to end the block
    //      and copy the rest to the next block
    do {
        char * _b = const_cast< char * >( _block.c_str() );
        _file.read( _b + _first, MIN_BLK_SZ );
        int _cnt = _first + _file.gcount();
        _block.resize( _cnt );
        if( _cnt > 0 ) {
            // we'll miss the last word if the file doesn't end with a whitespace.
            auto _last = _block.find_last_of( " \t\n" );
            _first = _cnt - _last - 1;
            std::string _tmp;
            if( _file ) {
                _tmp.resize( MIN_BLK_SZ + _first );
                if( _first > 0 ) _tmp.replace( 0, _first, _block, _last + 1, _first );
                //                _tmp.resize( MIN_BLK_SZ + _first );
            }
            _block.resize( _last );
            auto _btag = std::make_pair( file, _id );
            // put each line
            ctxt.blocks.put( _btag, _block );
            // and control to start searching/matching
            ctxt.map_tags.put( _btag );
            _block = _tmp;//.reset( _tmp, std::default_delete<char[]>() );
            ++_id;
        }
    } while( _file );

    return 0;
}
