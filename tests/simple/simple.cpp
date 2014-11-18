#if 1
#define _CRT_SECURE_NO_DEPRECATE
#define __TBB_RELAXED_OWNERSHIP 0

// define _DIST_ to enable distCnC
//#define _DIST_

#include <iostream>
#include <sstream>
#include <tbb/atomic.h>
#include <tbb/tick_count.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb_thread.h>
#include <tbb/blocked_range.h>
#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>

#define SLEEP( _x ) tbb::this_tbb_thread::sleep( tbb::tick_count::interval_t( _x ) )

struct my_context;

struct my_step
{
    int execute( const std::pair< int, int > & tag, my_context & c ) const;
};

struct my_step2
{
    int execute( const int & tag, my_context & c ) const;
};

struct my_step3
{
    int execute( const int & tag, my_context & c ) const;
};

struct my_step4
{
    int execute( const tbb::blocked_range< int > & tag, my_context & c ) const
    {
#ifdef _DIST_
        if( CnC::tuner_base::myPid() == 0 )
#endif
        {
            std::ostringstream o;
            o << "step4:";
            for( int i = tag.begin(); i < tag.end(); ++i ) {
                o << " " << i;
            }
            o << std::endl;
            tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
            std::cout << o.str() << std::flush;
        }
        return 0;
    }
};

template<>
struct cnc_tag_hash_compare< tbb::blocked_range< int > >
{
    size_t hash( const tbb::blocked_range< int > & x ) const { 
        // Knuth's multiplicative hash
        return x.begin();
    }
    bool equal( const tbb::blocked_range< int > & a, const tbb::blocked_range< int > & b ) const {
        return a.begin() == b.begin() && a.end() == b.end();
    }
};

namespace tbb {
    CNC_BITWISE_SERIALIZABLE( blocked_range< int > );
}

template< class T >
class A {};

template<>
class A< int > {};

class my_item
{
public:
    int * m_int;
    my_item() : m_int( new int )
    { *m_int = 0; }
    my_item( const my_item & i ) : m_int( new int )
    { *m_int = *i.m_int; }
    my_item( int i ) : m_int( new int )
    { *m_int = i; }
     my_item & operator=( const my_item & i )
     {
         *m_int = *i.m_int;
         return *this;
     }
     ~my_item()
     { delete m_int; }
     void serialize( CnC::serializer & ser )
     { // test no_alloc
         ser & CnC::chunk< int, CnC::no_alloc >( m_int, 1 );
     }
};

#ifdef _DIST_
//CNC_BITWISE_SERIALIZABLE( my_item );
namespace CnC {
    void serialize( CnC::serializer & buf, int *& ptr )
    {
        buf & CnC::chunk< int >( ptr, 1 );
    }
}
#endif

struct my_tuner : public CnC::step_tuner<>
{
    // define dependencies separately
    template< class dependency_consumer >
    void depends( const std::pair< int, int > & tag, my_context & c, dependency_consumer & dC ) const;
    
    // use XGet in step::execute
    bool preschedule() const { return false; }
    
    int compute_on( const std::pair< int, int > & tag, my_context & ) const { return (tag.first & 0x8) ? 0 : 1; }
    
    //bool lock_globally( const std::pair< int, int > &, my_context & ) const { return true; }
};

struct my_tuner2 : public CnC::step_tuner<>
{
    int compute_on( const int & tag, my_context & ) const { return (tag & 0x8) ? 0 : 1; }
};

struct my_tuner3 : public CnC::step_tuner<>
{
    int compute_on( const int & tag, my_context & ) const
    {
        return ( tag - 22 ) / 60;
    }
    int compute_on( const tbb::blocked_range< int > & tag, my_context & c ) const
    {
        return compute_on( tag.begin(), c );
    }
};

struct my_tuner4 : public CnC::step_tuner<>
{
    int compute_on( const tbb::blocked_range< int > & tag, my_context & ) const { return tag.begin() / 60; }// CnC::COMPUTE_ON_LOCAL; }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< my_step, my_tuner >         m_steps;
    CnC::step_collection< my_step2, my_tuner2 >       m_steps2;
    CnC::step_collection< my_step3, my_tuner3 >       m_steps3;
    CnC::step_collection< my_step4, my_tuner4 >       m_steps4;
    CnC::tag_collection< std::pair< int, int >, CnC::preserve_tuner< std::pair< int, int > > >              m_tags;
    CnC::tag_collection< int >                                                                              m_tags2;
    CnC::tag_collection< int, CnC::tag_tuner< tbb::blocked_range< int >, CnC::default_partitioner< 30 > > > m_tags3;
    CnC::tag_collection< tbb::blocked_range< int >, CnC::tag_tuner< tbb::blocked_range< int >, CnC::tag_partitioner< 20 > > > m_tags4;
    CnC::item_collection< int, my_item >             m_items;
    CnC::item_collection< double, double> m_items2;
    typedef CnC::item_collection< int, int *, CnC::vector_tuner > ip_collection_type;
    ip_collection_type m_itemsP;
    my_context() 
        : CnC::context< my_context >(),
          m_steps( *this, "my_step" ),
          m_steps2( *this, "my_step2" ),
          m_steps3( *this ),
          m_steps4( *this ),
          m_tags( *this, "m_tags" ),
          m_tags2( *this ),
          m_tags3( *this ),
          m_tags4( *this ),
          m_items( *this ),
          m_items2( *this ),
          m_itemsP( *this )
    {
        m_tags.prescribes( m_steps, *this );
        m_tags2.prescribes( m_steps2, *this );
        m_tags3.prescribes( m_steps3, *this );
        m_tags4.prescribes( m_steps4, *this );
        m_itemsP.set_max( 900 );
    }
    
    virtual void serialize( CnC::serializer & ) 
    {
        tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
        std::cout << "my_context::serialize()\n" << std::flush;
    }
};

template< class dependency_consumer >
void my_tuner::depends( const std::pair< int, int > & tag, my_context & c, dependency_consumer & dC ) const
{
    //dC.depends( c.m_items, tag.first );
    //dC.depends( c.m_items2, tag.first );
}

struct fnctr
{
    void operator()( int i ) const {
#ifdef _DIST_
        if( CnC::tuner_base::myPid() == 0 )
#endif
        {
            std::ostringstream o;
            //            o << "fctr< " << m_tag << " >( " << i << " )\n";
            o << m_tag << " " << i << std::endl;
            tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
            std::cout << o.str() << std::flush;
        }
    }
    int m_tag;
    fnctr( int t = -1 ) : m_tag( t ) {}
};

tbb::atomic< int > counter;
tbb::atomic< int > checker[1001];

int my_step::execute( const std::pair< int, int > & tag, my_context & c ) const
{
    ++checker[tag.first];
    // traditional gets
    my_item i;
    c.m_items.get( tag.first, i );
    double j;
    c.m_items2.get( tag.first, j );

    c.flush_gets();

    ++counter;
    if( tag.first < 100 ) {
        checker[tag.first * 10] = 0;
        c.m_tags.put( std::make_pair( tag.first * 10, 2 ) );
        c.m_tags2.put( tag.first * 10 );
        SLEEP( 0.3 );
        c.m_items.put( tag.first * 10, tag.first * 10 + 1 );
        c.m_items2.put( tag.first * 10.0, tag.first * 10.0 + 1 );
    }
    //for( CnC::tag_collection< std::pair< int, int > >::const_iterator it = c.m_tags.begin(); it != c.m_tags.end(); ++it ) {
    //    std::ostringstream o;
    //    o << "T([" << it->first << ", " << it->second<< "]) ";
    //    std::cerr << o.str();
    //}
    //for( CnC::item_collection< int, my_item >::const_iterator it = c.m_items.begin(); it != c.m_items.end(); ++it ) {
    //    std::ostringstream o;
    //    o << "I(" << (it)->first << ", " << (*(it)->second->m_int) << ") ";
    //    std::cerr << o.str();
    //}
    return CnC::CNC_Success;
}


int my_step2::execute( const int & tag, my_context & c ) const
{
    CnC::parallel_for( 0, tag, 1, fnctr( tag ), CnC::pfor_tuner< false >() );
    c.m_itemsP.put( tag, new int( tag ) ); // TODO mem leak
    return CnC::CNC_Success;
}

static tbb::atomic< char > _chck[512];

// test putting custom ranges prescribing a step with dependencies 
int my_step3::execute( const int & tag, my_context & c ) const
{
    if( _chck[tag] != 0 )
        std::cerr << "step3( " << tag << " ) repeatedly called.\n";
    if( tag < 30 ) {
        double _tmp;
        c.m_items2.get( 100, _tmp );
    }
    //    std::cerr << "step3( " << tag << " ) called on " << CnC::tuner_base::myPid() << "\n" << std::flush;
    _chck[tag] = 1;
    return CnC::CNC_Success;
}


int main( int, char*[] )
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > _dinit;
#endif
    CnC::debug::set_num_threads( 64 );
    {
        my_context c;
        //return 0;
        tbb::tick_count start = tbb::tick_count::now();

#ifdef _DIST_
        assert( CnC::tuner_base::myPid() == 0 );
#endif
        CnC::debug::trace( c.m_tags );
        CnC::debug::trace( c.m_steps );
        //CnC::debug::trace( m_tags2 );
        CnC::debug::trace( c.m_steps2 );
        //CnC::debug::trace( my_step3() );
        //CnC::debug::trace( my_step4() );
        //CnC::debug::collect_scheduler_statistics( c );

        //for( int i = 0; i < 1; ++i ) {
        //    ENV( c )PUTT( c.m_tags, std::make_pair( i, 2 ) );
        //}
        c.wait();
//        counter = 0;
        for( int i = 1; i < 10; ++i ) {
            checker[i] = 0;
            c.m_tags.put( std::make_pair( i, 2 ) );
            c.m_items.put( i, i + 1 );
            c.m_items2.put( i, i + 1 );
            //SLEEP( 4 );
        }

        tbb::blocked_range< int > br( 22, 222, 1 );
        c.m_tags3.put_range( br );
        c.m_tags4.put_range( br );

        my_item mi;
        c.m_items.get( 90, mi );

        c.wait();

        tbb::tick_count stop = tbb::tick_count::now();
        std::cerr << "time: " << (stop-start).seconds() << std::endl;

        std::cout << std::endl << counter << std::endl;
        std::cout << "got " << (*mi.m_int) << std::endl;
        c.m_items.get( 74774, mi );

        for( int i = br.begin(); i != br.end(); ++i ) {
            if( my_tuner3().compute_on( i, c ) == CnC::tuner_base::myPid() && _chck[i] != 1 )
                std::cerr << "step3( " << i << " ) did not succeed.\n";
        }

        //for( CnC::tag_collection< std::pair< int, int > >::const_iterator it = c.m_tags.begin(); it != c.m_tags.end(); ++it ) {
        //    std::ostringstream o;
        //    o << "T([" << it->first << ", " << it->second<< "])\n";
        //    std::cout << o.str();
        //}
        for( CnC::item_collection< int, my_item >::const_iterator it = c.m_items.begin(); it != c.m_items.end(); ++it ) {
            std::ostringstream o;
            o << "I(" << (it)->first << ", " << (*(it)->second->m_int) << ")\n";
            std::cout << o.str();
        }
        for( my_context::ip_collection_type::const_iterator it = c.m_itemsP.begin(); it != c.m_itemsP.end(); ++it ) {
            std::ostringstream o;
            o << "IP(" << (it)->first << ", " << *(*((it)->second)) << ")\n";
            int * _tmp;
            std::cout << o.str();
        }

        int _sz = c.m_items.size();
        std::cout << "number of items: " << _sz << std::endl;
        c.unsafe_reset();
        _sz = c.m_items.size();
        std::cout << "number of items: " << _sz << std::endl;
        _sz = c.m_tags.size();
        std::cout << "number of tags: " << _sz << std::endl;

        // ////////////////////////////
#ifndef _DIST_
        CnC::item_collection< const char *, int > _tcc( c );
        {
            const char * _st = "kkiikk";
            _tcc.put( _st, 77 );
            int _tmp;
            _tcc.get( _st, _tmp );
            assert( _tmp == 77 );
            _tcc.get( "jj", _tmp );
        }
        CnC::item_collection< std::string, int > _tcs( c );
        {
            std::string _st = "kkiikk";
            _tcs.put( _st, 77 );
            int _tmp;
            _tcs.get( _st, _tmp );
            assert( _tmp == 77 );
            _tcs.get( "jj", _tmp );
        }
        CnC::item_collection< std::vector< int >, int > _tcv( c );
        {
            std::vector< int > _st;
            _st.push_back( 1 );
            _tcv.put( _st, 77 );
            int _tmp;
            _tcv.get( _st, _tmp );
            assert( _tmp == 77 );
            _st.push_back( 2 );
            _tcv.put( _st, 88 );
            _tcv.get( _st, _tmp );
            assert( _tmp == 88 );
            _st.push_back( 3 );
            _tcv.put( _st, 11 );
            _tcv.get( _st, _tmp );
            assert( _tmp == 11 );
            _st.push_back( 4 );
            _tcv.put( _st, 22 );
            _tcv.get( _st, _tmp );
            assert( _tmp == 22 );
            _st.push_back( 5 );
            _tcv.put( _st, 33 );
            _tcv.get( _st, _tmp );
            assert( _tmp == 33 );
            _st.push_back( 6 );
            _tcv.get( _st, _tmp );
        }
#endif
        
    }
    return 0;
}
#endif
