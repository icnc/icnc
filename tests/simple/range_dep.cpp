#include <iostream>
#include <cnc/cnc.h>
#include <cnc/debug.h>

struct ctx;
typedef int tag_type;
typedef tbb::blocked_range< tag_type > range_type;

// Tuners
struct step2_tuner : public CnC::step_tuner<>
{
    template< class dependency_consumer >
    void depends( const tag_type& t, ctx& c, dependency_consumer& dC ) const;
};

struct data_tuner : public CnC::hashmap_tuner
{
    int get_count( const tag_type & /*tag*/ ) const
    {
        return 1;
    }
};

// Steps
struct step1
{
    int execute( const tag_type& t, ctx& c ) const;
};

struct step2
{
    int execute( const tag_type& t, ctx& c ) const;
};

// template<>
// struct cnc_tag_hash_compare< tag_type > 
// {
//     size_t hash( const tag_type& x ) const
//     {
//         return x.begin();
//     }   

//     bool equal( const tag_type& a, const tag_type& b ) const
//     {
//         return a.begin() == b.begin() && a.end() == b.end();
//     }   
// };

// The context class
struct ctx : public CnC::context< ctx >
{
    // Step collections
    CnC::step_collection< step1 >              m_step1s;
    CnC::step_collection< step2, step2_tuner > m_step2s;

    // Item collections
    CnC::item_collection< tag_type, int, data_tuner > m_data;

    // Tag collections
    CnC::tag_collection< tag_type, CnC::tag_tuner< range_type, CnC::default_partitioner< 4 > > > m_tags;

    // The context class constructor
    ctx()
        : m_step1s( *this, "step1" ),
          m_step2s( *this, "step2" ),
          // Initialize each item collection
          m_data( *this ),
          // Initialize each tag collection
          m_tags( *this )
    {
        // Prescriptive relations
        m_tags.prescribes( m_step1s, *this );
        m_tags.prescribes( m_step2s, *this );
        CnC::debug::trace( m_step1s );
        CnC::debug::trace( m_step2s );
    }
};

template< class dependency_consumer >
void step2_tuner::depends( const tag_type& t, ctx& c, dependency_consumer& dC ) const
{
    dC.depends( c.m_data, t );
}

int step1::execute( const tag_type& t, ctx& c ) const
{
    c.m_data.put( t, 8 );
    return CnC::CNC_Success;
}

int step2::execute( const tag_type& t, ctx& c ) const
{
    int val;
    c.m_data.get( t, val );
    return CnC::CNC_Success;
}

int main( int argc, char* argv[] )
{
    if( argc < 3 ) {
        std::cerr << "Usage: " << argv[0] << " [size] [granularity]\n";
        exit( -1 );
    }
  
    int n = atoi( argv[1] );
    int g = atoi( argv[2] );

    ctx c;
    CnC::debug::collect_scheduler_statistics( c );

    c.m_tags.put_range( range_type( 0, n, g ) );
 
    c.wait();

    return 0;
}
