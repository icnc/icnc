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
#include <cnc/debug.h>
#include <tbb/blocked_range.h>

// let's use a large type to store fib numbers
typedef unsigned long long fib_type;
// forward declaration
struct fib_context;

// declaration of compute step class
struct fib_step
{
    // declaration of execute method goes here
    int execute( const fib_type & tag, fib_context & c ) const;
};

// a tuner for preserving tags and using a tag-range
// to use just a range without memoization you can just use
// CnC::tag_tuner< tbb::blocked_range< fib_type > >
struct fib_tuner : public CnC::preserve_tuner< fib_type >
{
	/// A tag tuner must provide the type of the range, default is no range
    typedef tbb::blocked_range< fib_type > range_type;
};

// this is our context containing collections and defining their depndencies
struct fib_context : public CnC::context< fib_context > // derive from CnC::context
{
	// step-collections
	CnC::step_collection< fib_step > m_steps;
    // item collection holding the fib number(s)
    CnC::item_collection< fib_type, fib_type > m_fibs;
    // tag collection to control steps
    // to use a tuner just for range without memoization you can just use
    // CnC::tag_tuner< tbb::blocked_range< fib_type > > instead of fib_tuner
    CnC::tag_collection< fib_type, fib_tuner > m_tags;

    // constructor
    fib_context();
};

fib_context::fib_context()
        : CnC::context< fib_context >(),
          // pass context to collection constructors
          m_steps( *this ),
          m_fibs( *this ),
          m_tags( *this )
{
    // prescribe compute step
    m_tags.prescribes( m_steps, *this );
    // show scheduler statistics when done
    CnC::debug::collect_scheduler_statistics( *this );
}

// the actual step code computing the fib numbers goes here
int fib_step::execute( const fib_type & tag, fib_context & ctxt ) const
{
    switch( tag ) {
        case 0 : ctxt.m_fibs.put( tag, 0 ); break;
        case 1 : ctxt.m_fibs.put( tag, 1 ); break;
        default : 
            // get previous 2 results
            fib_type f_1; ctxt.m_fibs.get( tag - 1, f_1 );
            fib_type f_2; ctxt.m_fibs.get( tag - 2, f_2 );
            // put our result
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

    // put tags to initiate evaluation
    ctxt.m_tags.put_range( tbb::blocked_range< fib_type >( 0, n ) );

    // wait for completion
    ctxt.wait(); 

    // get result
    fib_type res2;
    ctxt.m_fibs.get( n, res2 );

    // print result
    std::cout << "fib (" << n << "): " << res2 << std::endl;

    return 0;
}
