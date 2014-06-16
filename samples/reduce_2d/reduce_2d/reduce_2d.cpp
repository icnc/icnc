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

/*
  Demo for connecting 2 dependent graphs (e.g. reductions).
  The input to the program are values ina  2d-space (a).
  The output is the sum of all values (sum2).
  The first reduction computes the sum for each row (sum1)
  The second takes this output and sums it to the final value (sum2).
  There is no barrier and only a single wait
  (ignore whats enclosed in #ifdef _CNC_TESTING_ ... #endif).
  We just use sum1 as the output for the first reduction (red1)
  and also as the input to the second (red2).
  Evaluation executes fully asynchronously.

  Additionally, the selector ignores certain values when summing the rows.

  Last but not least we also increase the stress on the runtime by providing
  the counts per reduction at different stages (early, mid, late).
 */

// size of "matrix"
const int N = 32;
// threashold for selecting candidates for row-sum
const int MX = 24;
// describes our 2d space
typedef std::pair< int, int > tag_type;


// selector performing different selections dependening on reduction/tag
struct selector
{
    // functor, assign second as reduction-id
    // return false if first >= MX (we only reduce items with tag->first < MX)
    template< typename T, typename S, typename R >
    bool operator()( const std::pair< T, S > & t, R & r ) const {
        if( t.first >= MX ) return false;
        r = t.second;
        return true;
    }

    // here we select all and return singleton 0
    template< typename T, typename R >
    bool operator()( const T & t, R & r ) const {
        r = 0;
        return true;
    }
};


// the step just puts the value to the "matrix"
// in real life this might first do some meaningful computation
struct putter
{
    // simply puts 1 with same tag to item-coll
    template< typename T, typename C >
    int execute( const T & t, C & ctxt ) const
    {
        ctxt.a.put( t, 1 );
        // in some cases we put the count here (just testing)
        if( t.first >= MX && t.first == t.second ) ctxt.cnt1.put( t.first, MX );
        return 0;
    }
};

struct reduce_context : public CnC::context< reduce_context >
{
    CnC::tag_collection< tag_type > tags;         // control tags for our step
    CnC::item_collection< tag_type, int > a;      // the matrix
    CnC::item_collection< int, int > cnt1, cnt2;  // provides number of participating items per reduction
    CnC::item_collection< int, int > sum1, sum2;  // outputs of reductions
    CnC::step_collection< putter > sc;            // the steps
    CnC::graph * red1, * red2;                    // out 2 sub-graphs: 2 reductions

    reduce_context()
        : tags( *this, "tags" ),
          a( *this, "a" ),
          cnt1( *this, "cnt1" ),
          cnt2( *this, "cnt2" ),
          sum1( *this, "sum1" ),
          sum2( *this, "sum2" ),
          sc( *this, "step" ),
          red1( NULL ),
          red2( NULL )
    {
        tags.prescribes( sc, *this );
        sc.produces( a );
        // first reduction inputs a and outputs sum1
        red1 = CnC::make_reduce_graph( *this, "red_row",
                                       a,            // input collection: our matrix
                                       cnt1,         // count of items per reduction
                                       sum1,         // output collection: sum per row
                                       std::plus<int>(),  // reduction operation '+'
                                       0,            //identity element
                                       selector() ); // selector
        // second reduction inputs sum1 and outputs sum2
        red2 = CnC::make_reduce_graph( *this, "red_red",
                                       sum1,         // the input collection is the output of red1
                                       cnt2,         // count of items per reduction
                                       sum2,         // the final output: sum of all values
                                       std::plus<int>(),  // reduction operation '+'
                                       0,            //identity element
                                       selector() ); // selector
#ifdef _CNC_TESTING_
        CnC::debug::trace( sum1, 1 );
        CnC::debug::trace( *red1, 2 );
        CnC::debug::trace( *red2, 2 );
#endif
    }

    ~reduce_context()
    {
        delete red1;
    }
};

int main()
{
#ifdef _DIST_
    CnC::dist_cnc_init< reduce_context > _dc;
#endif
    // create the context
    reduce_context ctxt;
    // put the count for some reduction before eveything else
    for( int i = 2; i < MX; ++i ) ctxt.cnt1.put( i, MX );
    // put control tags
    for( int i = 0; i < N; ++i ) {
        for( int j = N-1; j >= 0; --j ) ctxt.tags.put( tag_type( i, j ) );
    }

#ifdef _CNC_TESTING_
    // wait for all reductions except 3
    ctxt.wait();
#endif

    ctxt.cnt1.put( 0, MX );
    ctxt.cnt2.put(0,N);

#ifdef _CNC_TESTING_
    // wait for all reductions except one to complete
    ctxt.wait();
#endif
    // put the count for one reduction after everything else
    ctxt.cnt1.put( 1, MX );
    // wait for safe state
    ctxt.wait();

    // iterate, check and write out all keys/value pairs
    auto _sz = ctxt.sum1.size();
    if( _sz != N ) {
        std::cerr << "#reductions is " << _sz << ", expected " << N << std::endl;
        return 1239;
    }
    for( auto i = ctxt.sum1.begin(); i != ctxt.sum1.end(); ++i ) {
        if( *i->second != MX ) {
            std::cerr << "cnt1 of " << i->first << " is " << *i->second << ", expected " << MX << std::endl;
            return 1234;
        }
    }
    int n = 0;
    ctxt.sum2.get(0, n );
    if( n != N*MX ) {
        std::cerr << "reduce count is " << n << ", expected " << N*MX << std::endl;
        return 1236;
    }
    std::cout << "Success with " << n << std::endl;

    return 0;
}
