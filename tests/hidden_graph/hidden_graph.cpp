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

/*
  Demonstration of a hidden graph that's active outside the on_put callbacks and
  the use of leave_quiescence and enter_quiescence to communicate internal state.

  The graph's internal computation is just sleeping random time, but when done it puts 
  control and data that a consumer step outside the hidden graph uses.
  As its internal work is not controlled by the CnC runtime, it must
  tell the runtime when it's working and when it's done. That's done
  with calling context::leave_quiescence and context::enter_quiescence.

  Environment puts data that the graph operates on.

  For distribution we use tuner::consumed_on to assign the input data to remote
  processes.
 */

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>
#include <ctime>
#include <tbb/concurrent_queue.h>
#include <thread>         // std::thread
#include <chrono>         // std::chrono::milliseconds

// This is our hidden graph.  It is parametrized by 3 collections: the
// input data colleciton, an output tag collection and a output data
// collection.
//
// It sets its state to active and registers a on_put callback which
// stores the input data in an internal queue.
//
// An internal thread pops entries from the queue, sleeps for a while
// and then puts a control tag ana corresponding data item.  When the
// thread has processed 7 entries it reports quiescence and exits.
//
// The environment only puts data, no control. Without quiescence
// reporting the wait call could return immediately as no steps are
// prescribed. AS the hidden graph sets its state to active right at
// the beginning, the wait() call will not succeed until the graph
// signaled its quiescence.
template< typename IC_IN, typename IC_OUT, typename TC_OUT >
class hidden_graph : public CnC::graph
{
private:
    typedef tbb::concurrent_bounded_queue< typename IC_IN::data_type > blocking_queue;

    // the callback for incoming data just pushes the items on our internal queue
    struct on_data : public IC_IN::callback_type
    {
        on_data( blocking_queue & q ) : m_q(q) {}
        void on_put( const typename IC_IN::tag_type & /*tag*/, const typename IC_IN::data_type & val )
        {
            m_q.push( val );
        };
        blocking_queue & m_q;
    };

    // we start a thread which takes data from our internal queue and works on it.
    // it enters quiescence and exits after processing 7 items
    // this is just a demo, so our work is simply sleepings
    struct observe_and_compute {
        void operator()( blocking_queue * queue, IC_OUT * dataout, TC_OUT * tagout, const hidden_graph< IC_IN, IC_OUT, TC_OUT > * graph )
        {
            typename IC_IN::data_type _item;
            typename TC_OUT::tag_type _tag(CnC::tuner_base::myPid()*1000);
            do {
                // get next input
                queue->pop( _item );
                // and do some work, we just sleep, but it could be anything
                // ideally of course this would use TBB tasks
                int _tm = rand() % 1111;
                std::this_thread::sleep_for( std::chrono::milliseconds(_tm) );
                // now produce our output, to make it a little more intersting let's make it conditional
                if( _tm % 4 ) {
                    dataout->put( _tag, _item + _tm );
                    tagout->put( _tag );
                    ++_tag;
                }
            } while( _tag < CnC::tuner_base::myPid()*1000+7 );
            // we are done, before exiting we must inform the runtime
            graph->enter_quiescence();
            CnC::Internal::Speaker oss;
            oss << "done observe_and_compute";
        }
    };
public:
    // the constructor tells the runtime that the graph is not quiescent
    // it also starts the thread and registers the on_put callback
    template< typename Ctxt >
    hidden_graph( CnC::context< Ctxt > & ctxt, const std::string & name, IC_IN & ic1, IC_OUT & ic2, TC_OUT & tc )
        : CnC::graph( ctxt, name ),
          m_input( ic1 ),
          m_dataout( ic2 ),
          m_tagout( tc ),
          m_queue(),
          m_thread( observe_and_compute(), &m_queue, &m_dataout, &m_tagout, this )
    {
        // we started a thread, we are not quiescent until it exits
        this->leave_quiescence();
        // register the callback
        m_input.on_put( new on_data( m_queue ) );
    }
    virtual ~hidden_graph()
    {
        m_thread.join();
    }
private:
    IC_IN  & m_input;   // here we expect the input to arrive
    IC_OUT & m_dataout; // here we put our output data
    TC_OUT & m_tagout;  // here we put the control tags
    blocking_queue m_queue; // the queue where we push the data and the threads pops data from
    std::thread m_thread;   // our internal thread
};

// a template function is much easier to use than a template class with many template arguments.
template< typename Ctxt, typename IC_IN, typename IC_OUT, typename TC_OUT >
hidden_graph< IC_IN, IC_OUT, TC_OUT > * make_hgraph( CnC::context< Ctxt > & ctxt, const std::string & name, IC_IN & ic1, IC_OUT & ic2, TC_OUT & tc )
{
    return new hidden_graph< IC_IN, IC_OUT, TC_OUT >( ctxt, name, ic1, ic2, tc );
}

struct hg_context;

// here we consume the values produced by the hidden graph
struct consume
{
    int execute( const int tag, hg_context & ctxt ) const;
};

// our tuner makes sure the data gets distributed
// as it gets produced in the env, we must define the consumed_on method
struct hg_tuner : public CnC::hashmap_tuner
{
    int consumed_on( const int & tag ) const
    {
        return tag % numProcs();
    }
};

// our actual "global" context
struct hg_context : public CnC::context< hg_context >
{
    CnC::step_collection< consume >   consumer;
    CnC::item_collection< int, int, hg_tuner >  input_data;
    CnC::item_collection< int, int >  processed_data;
    CnC::item_collection< int, int >  result_data;
    CnC::tag_collection< int >        consumer_tags;
    CnC::graph * hgraph;

    hg_context()
        : consumer( *this, "consumer" ),
          input_data( *this, "input_data" ),
          processed_data( *this, "processed_data" ),
          result_data( *this, "result_data" ),
          consumer_tags( *this, "consumer_tags" )
    {
        consumer_tags.prescribes( consumer, *this );
        consumer.consumes( processed_data );
        consumer.produces( result_data );
        hgraph = make_hgraph( *this, "hidden_graph", input_data, processed_data, consumer_tags );
        //        CnC::debug::trace_all( *this );
    }
};

// the consumer just gets the data and puts something into result
int consume::execute( const int tag, hg_context & ctxt ) const
{
    int _val;
    ctxt.processed_data.get( tag, _val );
    ctxt.result_data.put( tag+tag, _val*_val );
    return 0;
}

// the env only puts data and waits for completion
int main()
{
    srand( 11 );
#ifdef _DIST_
    CnC::dist_cnc_init< hg_context > _dc;
#endif
    hg_context _ctxt;
    for( int i = 0; i < 444; ++i ) {
        _ctxt.input_data.put( i, rand() );
    }
    _ctxt.wait();
    // we use srand, so the number of generated tuples should always be identical
    std::cout << _ctxt.result_data.size() << std::endl;
    return 0;
}
