#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>
#include <thread>

struct my_context;

struct my_step
{
    int execute( const int tag, my_context & c ) const;
};

struct my_tuner : public CnC::step_tuner<>
{
    bool sequentialize( const int t, const my_context & ) const {
        return t % 2 == 1;
    }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step, my_tuner >  m_steps;
    CnC::tag_collection< int >       m_tags;
    CnC::item_collection< int, int > m_items;

    my_context() 
        : m_steps( *this, "my_step" ),
          m_tags( *this, "my_tags" ),
          m_items( *this, "my_items" )
    {
        m_tags.prescribes( m_steps, *this );
        if( CnC::tuner_base::myPid() == 0 ) {
            CnC::debug::trace_all( *this );
            CnC::debug::collect_scheduler_statistics( *this );
        }
    }
};

#define SLEEP( _x ) std::this_thread::sleep_for( std::chrono::milliseconds(_x) )

static int val = 1000000;

// we expect serialized steps (tag%2==1) to get scheduled one after the other from high to low.
// for steps < 10 we depends on the output of tag-2 -> for tags<10 they get executed from low to high.
// steps>14 have no dependency, so they kep getting executed first high to low
// steps<14 but >10 depends on step(1), so they get executed last low to high
// each serialized step alters a global var and prints its tag with the value.
// if steps get executed in a different order, the tag/value pairs will be different.
int my_step::execute( const int tag, my_context & c ) const
{
    SLEEP( 100 );
    if( tag % 2 == 1 ) {
        // we need a sleep to "wait" until remote processes deliver item 1
        if( tag == 3 ) SLEEP( 5000 );
        int item;
        if( tag < 14 ) c.m_items.get( tag > 10 ? 1 : tag-2, item );
        CnC::Internal::Speaker oss;
        oss << "tag " << tag << " with value " << val;
        val -= tag;
        c.m_items.put( tag, val );
    }
    return 0;
}

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;
    c.m_items.put( -1, -1 );
    for( int i = 0; i < 18; ++i ) {
        c.m_tags.put( i );
    }
    c.wait();
    return 0;
}
