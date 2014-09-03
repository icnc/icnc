#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>

struct my_context;

struct my_step
{
    int execute( const int tag, my_context & c ) const
    {
        return 0;
    }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step >  m_steps;
    CnC::tag_collection< int >       m_tags;

    my_context() 
        : m_steps( *this, "my_step" ),
          m_tags( *this, "my_tags" )
    {
        m_tags.prescribes( m_steps, *this );
    }
};

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;
    for( int i = 0; i < 1000; ++i ) c.m_tags.put( i );
    c.wait();
    return 0;
}
