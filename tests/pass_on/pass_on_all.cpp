
#define _CRT_SECURE_NO_DEPRECATE
#define __TBB_RELAXED_OWNERSHIP 0

#include <iostream>
#include <tbb/tick_count.h>
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>

const int N = 20;

struct my_context;

struct my_step : public CnC::step_tuner<>
{
    int execute( int t, my_context &c ) const;
    int compute_on( int t, my_context &c ) const;
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step, my_step > m_steps;
    CnC::tag_collection< int > m_tags;
    CnC::item_collection< int, int > m_items;

    my_context()        
      : CnC::context< my_context >(),
        m_steps( *this ),
        m_tags( *this ),
        m_items( *this )
    {
        m_tags.prescribes( m_steps, *this );
    }  
};

int my_step::compute_on( int t, my_context &c ) const
{
  return CnC::COMPUTE_ON_ALL;
}

int my_step::execute(int t, my_context &c) const
{
    assert( t < N );
    c.m_items.put( myPid() * N + t, t );
    tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
    std::cout << t << std::endl;
    return CnC::CNC_Success;
}

int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif

    my_context c;

    assert( CnC::tuner_base::myPid() == 0 );
    int numProcs = CnC::tuner_base::numProcs();

    for ( int i = 1; i < N; ++i ) {
        c.m_tags.put(i); 
    }

    c.wait();
    c.wait();
    //    assert(c.m_items.size() == numProcs * ( N - 1 ) || NULL == "Number of elements in the item collection is incorrect!");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for( CnC::item_collection< int, int >::const_iterator it = c.m_items.begin(); it != c.m_items.end(); ++it ) {
        std::cout << ( it->first - *it->second )/N << "," << *it->second << std::endl;
    }
    return 0;
}

