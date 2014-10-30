#include <cnc/cnc.h>
#include <cnc/debug.h>
#include <tbb/tick_count.h>

#define SLEEP( _x ) tbb::this_tbb_thread::sleep( tbb::tick_count::interval_t( _x ) )

struct my_context;

int n = 31;

struct step1 : CnC::step_tuner<>
{
    int execute( const int tag, my_context & ctxt ) const;
    // template< class dependency_consumer > void depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const;
};

struct step2 : CnC::step_tuner<>
{
    int execute( const int tag, my_context & ctxt ) const;
    // template< class dependency_consumer > void depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const;
};

struct step3 : CnC::step_tuner<>
{
    int execute( const int tag, my_context & ctxt ) const;
    // template< class dependency_consumer > void depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const;
};

struct my_context : public CnC::context< my_context >
{
    my_context()
        : m_steps1( *this, "step1" ),
          m_steps2( *this, "step2" ),
          m_steps3( *this, "step3" ),
          m_tags( *this, "tags" ),
          m_items( *this )
    {
        m_tags.prescribes( m_steps1, *this );
        m_tags.prescribes( m_steps2, *this );
        m_tags.prescribes( m_steps3, *this );
        CnC::debug::trace( m_steps1 );
        CnC::debug::trace( m_tags );
        // CnC::debug::trace( m_steps2 );
        // CnC::debug::trace( m_steps3 );
        // CnC::debug::trace( m_items, "items" );
        CnC::debug::collect_scheduler_statistics( *this );
    }

    CnC::step_collection< step1, step1 >                  m_steps1;
    CnC::step_collection< step2, step2 >                  m_steps2;
    CnC::step_collection< step3, step3 >                  m_steps3;
    CnC::tag_collection< int, CnC::tag_tuner< tbb::blocked_range< int > > > m_tags;
    CnC::item_collection< int, int >                      m_items;
};

int step1::execute( const int tag, my_context & ctxt ) const
{
    int _tmp;
    if( tag < n ) {
        ctxt.m_items.get( tag+1, _tmp ); //dep to same step-coll
    }
    ctxt.m_items.put( tag, tag );
    tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
    std::cout << "step1 " << tag << std::endl;
    return CnC::CNC_Success;
}

// template< class dependency_consumer >
// void step1::depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const
// {
//     if( tag < n ) dC.depends( ctxt.m_items, tag+100000 );
// }

const int off2 = 10000;
const int off3 = 20000;

int step2::execute( const int tag, my_context & ctxt ) const
{
    int _tmp;
    if( tag ) {
        bool avail = ctxt.m_items.unsafe_get( tag-1+off3, _tmp ); // dep to step3
        if( ! avail ) {
            tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
            return CnC::Internal::CNC_NeedsSequentialize;
        }
    }
    ctxt.m_items.put( tag+off2, tag ); // step3 depends on this
    tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
    std::cout << "step2 " << tag << std::endl;
    return CnC::CNC_Success;
}

// template< class dependency_consumer >
// void step2::depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const
// {
//     if( tag ) dC.depends( ctxt.m_items, tag+off3 ); // dep to step3
// }

int step3::execute( const int tag, my_context & ctxt ) const
{
    int _tmp;
    ctxt.m_items.get( tag+off2, _tmp ); //dep to step2
    SLEEP( 0.1 );
    ctxt.m_items.put( tag+off3, tag ); // step2 depends on this
    tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
    std::cout << "step3 " << tag << std::endl;
    return CnC::CNC_Success;
}

// template< class dependency_consumer >
// void step3::depends( const int tag, my_context & ctxt, dependency_consumer & dC ) const
// {
//     if( tag % 3 ) dC.depends( ctxt.m_items, tag+off2 ); //dep to step2
// }


int main( int argc, char * argv[] )
{
    my_context ctxt;
    ctxt.m_tags.put_range( tbb::blocked_range< int >( 0, n+1, 3 ) );
    ctxt.wait();
    return 0;
}
