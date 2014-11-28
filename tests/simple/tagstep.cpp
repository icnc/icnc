#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>
#include <iostream>

struct my_context;

template< typename Tag, typename Step >
class tagged_step_collection : public CnC::tag_collection< Tag >, public CnC::step_collection< Step >
{
public:
    template< typename Ctxt >
    tagged_step_collection( Ctxt & ctxt, const std::string & name )
      : CnC::tag_collection< Tag >( ctxt, name ), CnC::step_collection< Step >( ctxt, name )
    {
        this->prescribes( *this, ctxt );
    }
};

struct my_step
{
    int execute( const int tag, my_context & c ) const;
};

struct my_context : public CnC::context< my_context >
{
    tagged_step_collection< int, my_step >  m_steps;
    CnC::item_collection< int, int  >       m_items;

    my_context() 
        : m_steps( *this, "tagged_step" ),
          m_items( *this, "items" )
    {
        m_steps.consumes( m_items );
        m_steps.produces( m_items );
    }
};

int my_step::execute( const int tag, my_context & c ) const
{
    int i;
    c.m_items.get( tag, i );
    c.m_items.put( tag+1, i );
    return 0;
}

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    my_context c;
    for( int i = 0; i < 1000; ++i ) {
        c.m_steps.put( i );
    }
    c.m_items.put( 0, 0 );
    c.wait();
    int i = 1212;
    c.m_items.get( 1000, i );
    std::cerr << ( i == 0 ? "Success\n" : "Failed\n");
    return i;
}
