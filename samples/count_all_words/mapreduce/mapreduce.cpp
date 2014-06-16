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

#include "mapreduce.h"

/*
  Count the number of occurances of all words in a file.  An example
  of using mapreduce_context.
  It's bascially the same functionality as what's done in count_all_words
  but using the mapreduce abstraction.
  
  Please see mapreduce.h and count_all_words.cpp for details.
*/

// we use std::plus as the reduce operation
namespace std {
    template< typename T >
    inline CnC::bitwise_serializable serializer_category( const std::plus< T >  * ) {
        return CnC::bitwise_serializable();
    }
}

// our map function (functor)
// reads given stream word by word and counts all occuring words
// by simply putting 1 for each word
struct count
{
    typedef std::string key_type;
    typedef size_type result_type;

    template< typename IStream, typename OutCollection >
    inline void operator()( IStream & iss, OutCollection & out ) const
    {
        std::string _word;
        while( iss >> _word ) {
            // let's a do a case-insensitive matching
            std::transform(_word.begin(), _word.end(), _word.begin(), ::tolower);
            out.put( _word, 1 );
        }
    }
};
CNC_BITWISE_SERIALIZABLE( count );

int main( int argc, char * argv[])
{
    typedef mapreduce_context< count, std::plus<size_type> > caw_context;
#ifdef _DIST_
    CnC::dist_cnc_init< caw_context > _dc;
#endif
    
    if( argc < 2 ) {
        std::cerr << "expected arguments: <file1>[ <file2> ...]\n";
            exit(1);
    }
    // create the context
    caw_context * mapred = make_mapreduce( count(), std::plus<size_type>() );

    tbb::tick_count startTime = tbb::tick_count::now();

    for( int i = 1; i<argc; ++i ) {
        mapred->read_tags.put( argv[i] );
        //        reader_execute( argv[i], mapred );
    }
    mapred->wait();

    tbb::tick_count endTime = tbb::tick_count::now();

    std::cout << mapred->results.size() << std::endl;
    // iterate and write out all keys/value pairs with counts > 999
    for( auto i = mapred->results.begin(); i != mapred->results.end(); ++i ) {
        //        if( *i->second > 999 )
            std::cout << i->first << " \t " << *i->second << std::endl;
    }

    delete mapred;

    std::cout << "Time: " << (endTime-startTime).seconds() << "s.\n";

    return 0;
}
