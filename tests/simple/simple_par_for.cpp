#define _CRT_SECURE_NO_DEPRECATE

// define _DIST_ to enable distCnC
//#define _DIST_

#include <iostream>
#include <sstream>
#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif

class my_context;

struct my_step
{
    int execute( const int & tag, my_context & c ) const;
};

struct my_context : public CnC::context< my_context >
{
    my_context()
        : CnC::context< my_context >(),
          m_tags( *this ),
          m_steps( *this )
    {
        m_tags.prescribes( m_steps, *this );
    }

    CnC::tag_collection< int >      m_tags;
    CnC::step_collection< my_step > m_steps;
};

int my_step::execute( const int & tag, my_context & c ) const
{
    CnC::parallel_for( 0, tag, 1, [&]( int i ) {
        std::ostringstream o;
        o << tag << "_" << i << std::endl;
        tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
        std::cout << o.str() << std::flush;
    } );
    return 0;
}

int main( int, char *[] )
{
    my_context c;
    for( int i = 0; i<10; ++ i ) c.m_tags.put( i );
    c.wait();
    return 0;
}
