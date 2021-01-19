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

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step >  m_steps;
    CnC::tag_collection< int >       m_tags;
    CnC::item_collection< int, int > m_items;

    my_context() 
        : m_steps( *this, "my_step" ),
          m_tags( *this, "my_tags" ),
          m_items( *this, "my_items" )
    {
        m_tags.prescribes( m_steps, *this );
        CnC::debug::trace( m_steps );
        CnC::debug::trace( m_items );
        CnC::debug::collect_scheduler_statistics( *this );
    }
};

int my_step::execute( const int tag, my_context & c ) const
{
    if( tag == 0 ) {
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        c.m_items.put( 0, 0 );
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        c.m_items.put( 1, 1 );
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        c.m_items.put( 2, 2 );
        std::this_thread::sleep_for( std::chrono::milliseconds(200) );
        c.m_items.put( 3, 3 );
    } else {
        int _i0, _i1, _i2, _i3;
        c.m_items.unsafe_get( 0, _i0 );
        c.flush_gets();
        c.m_items.unsafe_get( 1, _i1 );
        c.flush_gets();
        c.m_items.unsafe_get( 2, _i2 );
        c.flush_gets();
        c.m_items.unsafe_get( 3, _i3 );
    }
    return 0;
}

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;
    c.m_tags.put( 1 );
    c.m_tags.put( 0 );
    c.wait();
    return 0;
}
