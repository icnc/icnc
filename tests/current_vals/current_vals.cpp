//********************************************************************************
// Copyright (c) 2013-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>
#include <ctime>

/*
  A non-deterministic graph. Emulating combination of unreliable transducers.
  From time to time the last known data is combined into a tuple and produced.
  For details see below docu for make_currVals_graph.
*/


// forward decls
template< typename IC1, typename IC2, typename IC3, typename TC > class currVal;
struct cv_context;

/// the output data tuple
template< typename t1, typename t2, typename t3 >
struct triple
{   
    triple( const t1 & v1, const t2 & v2, const t3 & v3 )
        : val1(v1), val2(v2), val3(v3)
    {}
    triple()
        : val1(0), val2(0), val3(0)
    {}
    triple( const triple< t1, t2, t3 > & o )
        : val1(o.val1), val2(o.val2), val3(o.val3)
    {}
    t1 val1;
    t2 val2;
    t3 val3;
};

#ifdef _DIST_
template< typename t1, typename t2, typename t3 >
inline CnC::bitwise_serializable serializer_category( const triple< t1, t2, t3 > * ) {
    return CnC::bitwise_serializable();
}
#endif

static tbb::atomic< time_t > __tm;
// on distributed memory a global counter is difficult -> use pid to encode globally unique time
// if we use non-globally-unique times we get duplicate tags and our verification fails
static time_t time_gen()
{
    return ( ( (time_t)CnC::Internal::distributor::myPid() ) << (sizeof(time_t)*4) ) + ( (time_t)(++__tm) );
}

/// \brief Create a non-determinstic graph
/// Simulates incoming data from 3 transducers.
/// each on_data callback stores an incoming sensor data in a atomic variable.
/// the on_tag callbacks computes and produces the tuple and control.
template< typename IC1, typename IC2, typename IC3, typename TC >
class currVal : public CnC::graph
{
public:
    typedef triple< typename IC1::data_type, typename IC2::data_type, typename IC3::data_type > triple_type;
    typedef currVal< IC1, IC2, IC3, TC > cv_type;
    typedef CnC::tag_collection< time_t > out_tcoll;        
    typedef CnC::item_collection< time_t, triple_type > out_icoll;
    friend struct on_data; // a template class defines the callback to store incoming values
    friend struct on_tag;  // callback is triggers read/write of value-tuples

    template< typename Ctxt >
    currVal( CnC::context< Ctxt > & ctxt, const std::string & name, IC1 & ic1, IC2 & ic2, IC3 & ic3, TC & tc, out_tcoll & ot, out_icoll & oi )
        : CnC::graph( ctxt, name ),
          outt( ot ),
          outi( oi )
    {
        // register the callbacks
        ic1.on_put( new on_data< IC1 >( &curr_v1 ) );
        ic2.on_put( new on_data< IC2 >( &curr_v2 ) );
        ic3.on_put( new on_data< IC3 >( &curr_v3 ) );
        tc.on_put( new on_tag( this ) );
        curr_v1 = 0;
        curr_v2 = 0;
        curr_v3 = 0;
    }

private:
    template< typename IC >
    struct on_data : public IC::callback_type
    {
        typedef typename tbb::atomic< typename IC::data_type > * val_ptr_type;
        // we accept the pointer to the curr-value member where we store the new value
        on_data( val_ptr_type vp  ) : curr_val(vp) {}
        void on_put( const typename IC::tag_type & tag, const typename IC::data_type & val )
        {
            *curr_val = val;
        };
        val_ptr_type curr_val;
    };

    struct on_tag : public TC::callback_type
    {
        on_tag( cv_type * _cv ) : cv( _cv ) {}
        void on_put( const typename TC::tag_type & tag )
        {
            // make unique time stamp
            time_t _tm = time_gen();
            // create tuple and put to output collection
            cv->outi.put( _tm, triple_type( cv->curr_v1, cv->curr_v2, cv->curr_v3 ) );
            // put control tag (same time)
            cv->outt.put( _tm );
        };
        cv_type * cv;
    };

    tbb::atomic< typename IC1::data_type > curr_v1;
    tbb::atomic< typename IC2::data_type > curr_v2;
    tbb::atomic< typename IC3::data_type > curr_v3;
    out_tcoll & outt;        
    out_icoll & outi;
};

/// \brief Create a non-deterministic graph
/// Simulates incoming data from 3 transducers.  An input tag
/// collection triggers the creation of a tuple with the last known
/// values.  As the frequency might differ between collections
/// (e.g. some data comes more of the than other), the resulting
/// values depend on the timing.  In addition to generating a tuple
/// with the time as the tag, it also produces a corresponding control
/// tag with the same time as its value.
///
/// \param ctxt   the context to which the graph belongs
/// \param name   the name of this graph instance
/// \param ic1    the first input collection
/// \param ic2    the second input collection
/// \param ic3    the third input collection
/// \param tc     the (input) tag collections that triggers reading/creating sensor data
/// \param ot     each time-stamp for which the output data is produced, we also emit a control tag
/// \param oi     for each triggered read, we produce a tuple with current values, tag is kind of time-stamp
template< typename Ctxt, typename IC1, typename IC2, typename IC3, typename TC >
CnC::graph * make_currVals_graph( CnC::context< Ctxt > & ctxt, const std::string & name, IC1 & ic1, IC2 & ic2, IC3 & ic3, TC & tc,
                                  typename currVal< IC1, IC2, IC3, TC >::out_tcoll & ot, typename currVal< IC1, IC2, IC3, TC >::out_icoll & oi )
{
    return new currVal< IC1, IC2, IC3, TC >( ctxt, name, ic1, ic2, ic3, tc, ot, oi );
};

/// the sensor/transducer
struct transduce
{
    int execute( const int tag, cv_context & ctxt ) const;
};
/// here we consume the generated value tuples
struct consume
{
    int execute( const time_t tag, cv_context & ctxt ) const;
};

typedef triple< double, int, bool > mytriple;

struct cv_context : public CnC::context< cv_context >
{
    CnC::step_collection< transduce >   transducer;
    CnC::step_collection< consume >     consumer;
    CnC::item_collection< int, double > ic1;
    CnC::item_collection< int, int >    ic2;
    CnC::item_collection< int, bool >   ic3;
    CnC::tag_collection< int >          trigger;
    CnC::tag_collection< int, CnC::preserve_tuner< int > > kick; // we don't want duplicate kick executions
    CnC::tag_collection< time_t >            new_tags;
    CnC::item_collection< time_t, mytriple > vals;
    CnC::graph * cvgraph;

    // wiring the collections and the graph
    cv_context()
        : transducer( *this, "td" ),
          consumer( *this, "consumer" ),
          ic1( *this, "ic1" ),
          ic2( *this, "ic2" ),
          ic3( *this, "ic3" ),
          trigger( *this, "trigger" ),
          kick( *this, "kick" ),
          new_tags( *this, "nt" ),
          vals( *this, "vals" )
    {
        kick.prescribes( transducer, *this );
        transducer.produces( ic1 );
        transducer.produces( ic2 );
        transducer.produces( ic3 );
        transducer.controls( trigger );
        cvgraph = make_currVals_graph( *this, "currVals", ic1, ic2, ic3, trigger, new_tags, vals );
        new_tags.prescribes( consumer, *this );
        consumer.consumes( vals );
        //CnC::debug::trace_all( *this );
    }
};

/// producing sensor data
/// depending on the tag, some data gets produced, others might not
/// sometimes we also produce the trigger to compute the current value tuples
int transduce::execute( const int tag, cv_context & ctxt ) const
{
    if( tag % 2 == 0 ) ctxt.ic1.put( tag, tag * 9.9 );
    if( tag % 3 == 0 ) ctxt.ic2.put( tag, tag * 2 );
    if( tag % 4 == 0 ) ctxt.ic3.put( tag, (tag % 6) == 0 );
    if( tag % 5 == 0 ) ctxt.trigger.put( tag );
    return 0;
}

/// dummy: just get the value tuples for given time (tag)
int consume::execute( const time_t tag, cv_context & ctxt ) const
{
    mytriple _vals;
    ctxt.vals.get( tag, _vals );
    return 0;
}

int main()
{
    srand( 11 );
    __tm = rand();
#ifdef _DIST_
    CnC::dist_cnc_init< cv_context > _dc;
#endif
    cv_context _ctxt;
    const int _lim = 1234;
    // the transducer gets prescribed with random tags. We use a
    // preserve-tuner to make sure we don't duplicate step instances
    for( int i = 0; i < _lim; ++i ) {
        _ctxt.kick.put( rand() );
    }
    _ctxt.wait();
    // we use srand, so the number of generated tuples should always be identical
    std::cout << _ctxt.vals.size() << std::endl;
    return 0;
}
