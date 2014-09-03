
#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cnc/join.h>
#include <cassert>

/*
 Let's do an artificial but fancy join on tag-collections:
   - use one join-graph (join1) to join to int-tag-collections
     output type is pair<int, int>
   - use another join-graph (join2) to join the output of join1 with a thid int-tag-collection
     output type is pair<int,pair<int, int>>
   - a step prescribed by the first tag-collection puts the second integer
     this guarntees we get the joins executed in parallel and distributed
   - a step prescribed by the doubly-joined tag-collection puts dummy items with the same tag
   - main finally outputs all generated items we should get the full NxNxN cross product
 Note: this means we connect 2 graphs with each other!
 */

// number of tags in each "dimension"
const int N = 16;
//const int N = 6;

struct putter
{
    // prescribed by first tag-colla (a), simply puts same tag to b
    template< typename C >
    int execute( const int t, C & ctxt ) const
    {
        ctxt.b.put( t );
        return 0;
    }

    // prescribed by first joined tag-coll, simply puts dummy item for given tag
    template< typename C >
    int execute( const std::pair< int, int > & t, C & ctxt ) const
    {
        ctxt.i0.put( t, 1 );
        return 0;
    }

    // prescribed by final doubly joined tag-coll, simply puts dummy item for given tag
    template< typename C >
    int execute( const std::pair< int, std::pair< int, int > > & t, C & ctxt ) const
    {
        ctxt.i.put( t, 1 );
        return 0;
    }
};

struct jt : public CnC::step_tuner<>
{
    // make sure we know how we execute the step which triggers the joins
    template< typename C >
    int compute_on( const int t, C & ) const
    {
        return t % numProcs();
    }
    // the item-put is done where the join was done
    template< typename C >
    int compute_on( const std::pair< int, int > & t, C & c ) const
    {
        return compute_on( t.first, c );
        return CnC::COMPUTE_ON_LOCAL;
    }
    // the final item-put is done where the join was done
    template< typename C >
    int compute_on( const std::pair< int, std::pair< int, int > > & t, C & c ) const
    {
        return compute_on( t.first, c );
        return CnC::COMPUTE_ON_LOCAL;
    }
};

struct join_context : public CnC::context< join_context >
{
    // int-tag-collections to be joined into c
    CnC::tag_collection< int > a, b;
    // cross-product of axb, produced by join1
    CnC::tag_collection< std::pair< int, int >, CnC::preserve_tuner< std::pair< int, int > > > c;
    // cross-product of bxc, produced by join2
    CnC::tag_collection< std::pair< int, std::pair< int, int > >, CnC::preserve_tuner< std::pair< int, std::pair< int, int > > > > d;
    CnC::step_collection< putter, jt > ts;
    CnC::item_collection< std::pair< int, int >, int > i0;
    CnC::step_collection< putter, jt > i0s;
    CnC::item_collection< std::pair< int, std::pair< int, int > >, int > i;
    CnC::step_collection< putter, jt > is;
    // we have 2 sub-graphs
    CnC::graph * join1, * join2;

    join_context()
        : a( *this, "a" ),
          b( *this, "b" ),
          c( *this, "c" ),
          d( *this, "d" ),
          ts( *this, "tputter" ),
          i0( *this, "i0" ),
          i0s( *this, "i0putter" ),
          i( *this, "i" ),
          is( *this, "iputter" ),
          // a, b -> join1 -> c
          join1( CnC::make_join_graph( *this, "j1", a, b, c ) ),
          // b, c -> join2 > d
          join2( CnC::make_join_graph( *this, "j2", b, c, d ) )
    {
        a.prescribes( ts, *this );
        c.prescribes( i0s, *this );
        d.prescribes( is, *this );
        //        CnC::debug::trace_all( *this, 3 );
    }

    ~join_context()
    {
        delete join1;
        delete join2;
    }
};

int main()
{
#ifdef _DIST_
    CnC::dist_cnc_init< join_context > _dc;
#endif
    // create the context
    join_context ctxt;
    // put "a" tags
    for( int i = 0; i < N; ++i ) ctxt.a.put( i );
    // wait for safe state
    ctxt.wait();
    std::cerr << "done waiting\n";
    // iterate and write out all keys
    for( auto i = ctxt.i0.begin(); i != ctxt.i0.end(); ++i ) {
        cnc_format( std::cout, i->first );
        std::cout << std::endl;
    }
    for( auto i = ctxt.i.begin(); i != ctxt.i.end(); ++i ) {
        cnc_format( std::cout, i->first );
        std::cout << std::endl;
    }
    return 0;
}
