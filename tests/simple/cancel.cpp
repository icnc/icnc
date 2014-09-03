
#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>

struct cl_context;

struct cl_step
{
    int execute( int t, cl_context & ctxt ) const;
};

struct cl_tuner : public CnC::cancel_tuner< int, false >
{
    cl_tuner( cl_context & c ) : CnC::cancel_tuner< int, false >( c ) {}

    // 0-4 on rank 0, 5-9 on 1
    int compute_on( const int & tag, cl_context & /*arg*/ ) const
    {
        return ( tag % 10 ) / 5;
    }
};

struct cl_context : public CnC::context< cl_context >
{
    cl_tuner                                  m_tuner;
    CnC::step_collection< cl_step, cl_tuner > m_steps; // our step-collections is cancelable
    CnC::tag_collection< int >                m_tags;
    CnC::item_collection< int, int >          m_items;

    cl_context()
        : m_tuner( *this ),
          m_steps( *this, m_tuner ),
          m_tags( *this ),
          m_items( *this )
    {
        m_tags.prescribes( m_steps, *this );
        m_steps.consumes( m_items );
        m_steps.produces( m_items );
    }
};

int cl_step::execute( int t, cl_context & ctxt ) const
{
    // let's cancel the tag which we compute (yes, steps can cancel other steps)
    // main will wait for a while before putting the next tag (which will be canceled by then)
    if( t < 100 ) {
        ctxt.m_tuner.cancel( t+1 );
        ctxt.m_items.put( t, t );
    } else {
        int _i;
        ctxt.m_items.get( t, _i );
    }
    return 0;
}

int main()
{
#ifdef _DIST_
    CnC::dist_cnc_init< cl_context > _dcinit;
#endif

    cl_context _ctxt;
#ifdef _DIST_
    assert( CnC::tuner_base::myPid() == 0 );
#endif
    CnC::debug::trace( _ctxt.m_steps, 3 );
    _ctxt.wait();

    // let's put 10 tags, all steps will fail as no data is available
    for( int i = 100; i < 110; ++i ) {
        _ctxt.m_tags.put( i );
    }
    // wait to make sure all steps are suspended
    _ctxt.wait();
    // now let's put data for some steps to succeed
    for( int i = 0; i < 3; ++i ) {
        _ctxt.m_items.put( 105+i, i );
        _ctxt.m_items.put( 100+i, i );
    }
    // let's make them all complete
    _ctxt.wait();
    // now cancel all remaining steps
    _ctxt.m_tuner.cancel_all();

    // put the data, steps should be unsuspended but not executed
    for( int i = 3; i < 5; ++i ) {
        _ctxt.m_items.put( 105+i, i );
        _ctxt.m_items.put( 100+i, i );
    }
    _ctxt.wait();

    // ok, next round
    // we need to reset cancel tuner (we are in safe state)
    _ctxt.m_tuner.unsafe_reset();

    for( int i = 0; i < 10; ++i ) {
        _ctxt.m_tags.put( i );
        // let's wait a bit to make sure the (even) step has executed and canceled the next (odd) one
        // before putting the next step
        _ctxt.wait();
    }

    // ok, next round
    // after reset all steps should go through if we execute them "downwards"
    _ctxt.unsafe_reset();
    for( int i = 9; i >= 0; --i ) {
        _ctxt.m_tags.put( i );
        // let's wait a bit to make sure the step has executed
        // before putting the next step
        _ctxt.wait();
    }
    for( CnC::item_collection< int, int >::const_iterator i = _ctxt.m_items.begin(); i != _ctxt.m_items.end(); ++i ) {
        std::cout << i->first << std::endl;
    }

    _ctxt.m_tags.unsafe_reset();
    _ctxt.m_items.unsafe_reset();
    
    return 0;
}
