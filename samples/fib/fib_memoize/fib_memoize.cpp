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

// compute fibonacci numbers
//

#define _CRT_SECURE_NO_DEPRECATE // to keep the VS compiler happy with TBB

#include <cnc/cnc.h>
struct fib_context;

// let's use a large type to store fib numbers
typedef unsigned long long fib_type;

// let's use a tuner to pre-declare dependencies
struct fib_tuner : public CnC::step_tuner<>
{
    // pre-declare data-dependencies
    template< class dependency_consumer >
    void depends( const int & tag, fib_context & c, dependency_consumer & dC ) const;
};

struct item_tuner : public CnC::hashmap_tuner
{
    // provide number gets to each item
    int get_count( const int & tag ) const;
};

#include "fib.h"

template< class dependency_consumer >
void fib_tuner::depends( const int & tag, fib_context & c, dependency_consumer & dC ) const
{
    // we have item-dependencies only if tag > 1
    if( tag > 1 ) {
      dC.depends( c.m_fibs, tag - 1 );
      dC.depends( c.m_fibs, tag - 2 );
    }
}

int item_tuner::get_count( const int & tag ) const
{
    return tag > 0 ? 2 : 1;
}

// the actual step code computing the fib numbers goes here
int fib_step::execute( const int & tag, fib_context & ctxt ) const
{
    switch( tag ) {
        case 0 : ctxt.m_fibs.put( tag, 0 ); break;
        case 1 : ctxt.m_fibs.put( tag, 1 ); break;
        default : 
            // get previous 2 results
            fib_type f_1; ctxt.m_fibs.get( tag - 1, f_1 );
            fib_type f_2; ctxt.m_fibs.get( tag - 2, f_2 );
            // put our result, and specify that this data item will be
			// get'ed twice (i.e. for the next two fib calculations)
			// before it is destroyed
            ctxt.m_fibs.put( tag, f_1 + f_2 );
    }
    return CnC::CNC_Success;
}

int main( int argc, char* argv[] )
{
    int n = 42;
    // eval command line args
    if( argc < 2 ) {
        std::cerr << "usage: " << argv[0] << " n\nUsing default value " << n << std::endl;
    } else n = atol( argv[1] );

    // create context
    fib_context ctxt; 

    // show scheduler statistics when done
    CnC::debug::collect_scheduler_statistics( ctxt );

	// put tags to initiate evaluation
    for( int i = 0; i <= n; ++i ) ctxt.m_tags.put( i );

    // wait for completion
    ctxt.wait(); 

    // get result
    fib_type res2;
    ctxt.m_fibs.get( n, res2 );

    // print result
    std::cout << "fib (" << n << "): " << res2 << std::endl;

    return 0;
}
