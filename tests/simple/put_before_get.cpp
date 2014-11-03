#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>

struct my_context;

struct my_step { int execute( const int tag, my_context & c ) const; };

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step >  m_steps;
    CnC::item_collection< int, int > m_items;
    CnC::tag_collection< int >       m_tags;

    my_context() 
        : m_steps( *this, "my_step" ),
          m_items( *this, "my_items" ),
          m_tags( *this, "my_tags" )
    {
        m_tags.prescribes( m_steps, *this );
    }
};

int my_step::execute( const int tag, my_context & c ) const
{
    c.m_items.put( tag+1, tag );
    int i;
    c.m_items.get( tag, i );
    return 0;
}

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;
    c.m_items.put( 0, 0 );
    for( int i = 0; i < 1000; ++i ) c.m_tags.put( i );
    c.wait();
    return 0;
}
