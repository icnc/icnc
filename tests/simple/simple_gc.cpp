
#define _CRT_SECURE_NO_DEPRECATE
#define __TBB_RELAXED_OWNERSHIP 0

#include <iostream>
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>
#include <thread>
#include <tbb/tick_count.h>
#include <cstdlib>

#ifdef _WIN32
#include <stdlib.h>
#endif

struct my_context;

struct my_step// : public CnC::step_tuner<>
{
    int execute( int t, my_context &c ) const;
};

const int N = 2000;

template< typename Base >
struct ituner : public Base
{
    int get_count( const int tag ) const
    {
        if( tag == 0 || tag == N-1 ) return CnC::NO_GETCOUNT;
        return 2;
    }
};


struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step >          m_steps;
    CnC::tag_collection< int >               m_tags;
    CnC::item_collection< int, int, ituner< CnC::hashmap_tuner > > m_hitems;
    CnC::item_collection< int, int, ituner< CnC::vector_tuner > >  m_vitems;

    my_context()        
      : CnC::context< my_context >(),
        m_steps( *this ),
        m_tags( *this ),
        m_hitems( *this ),
        m_vitems( *this )
    {
        m_vitems.set_max( N );
        m_tags.prescribes( m_steps, *this );
        // CnC::debug::trace( m_hitems );
    }  
};

// int my_step::compute_on( int t, my_context &c ) const
// {
//   return CnC::COMPUTE_ON_ALL;
// }

int my_step::execute(int t, my_context &c) const
{
    int _tmp;
    c.m_hitems.get( t-2, _tmp );
    c.m_hitems.get( t-1, _tmp );
    c.m_vitems.get( t-2, _tmp );
    c.m_vitems.get( t-1, _tmp );
    c.m_hitems.put( t, t );
    c.m_vitems.put( t, t );
    return CnC::CNC_Success;
}

int main( int, char*[] )
{

#ifdef _WIN32
    _putenv( "CNC_SCHEDULER=FIFO_STEAL" );
#else
    setenv( "CNC_SCHEDULER", "FIFO_STEAL", 1 );
#endif

#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif

    my_context c;
    c.m_hitems.put( 0, 0 );
    c.m_hitems.put( 1, 1 );
    c.m_vitems.put( 0, 0 );
    c.m_vitems.put( 1, 1 );
    for ( int i = 2; i < N; ++i ) {
        c.m_tags.put(i); 
    }
    c.wait();

    // sleep a bit to make sure the GC in the background has completed (distCnC)
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << c.m_vitems.size() << " " << c.m_hitems.size() << std::endl;

    for( CnC::item_collection< int, int, ituner< CnC::hashmap_tuner > >::const_iterator it = c.m_hitems.begin(); it != c.m_hitems.end(); ++it ) {
        std::cout << "H(" << (it)->first << ", " << (*(it)->second) << ")\n";
    }
    for( CnC::item_collection< int, int, ituner< CnC::vector_tuner > >::const_iterator it = c.m_vitems.begin(); it != c.m_vitems.end(); ++it ) {
        std::cout << "V(" << (it)->first << ", " << (*(it)->second) << ")\n";
    }

    return 0;
}

