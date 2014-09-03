//********************************************************************************
// Copyright (c) 2010-2014 Intel Corporation. All rights reserved.              **
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

// compute fibonacci numbers
//
// We need two steps, one for putting tags, one for the actual computation of f(n) = f(n-1) + f(n-2)

#define _CRT_SECURE_NO_DEPRECATE // to keep the VS compiler happy with TBB

// define _DIST_ to enable distCnC
// #define _DIST_

#ifdef _DIST_
#include <cnc/dist_cnc.h>         // include CnC header
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>       // needed for tracing and CnC scheduling statistics
#include <tbb/tick_count.h>
#include <cassert>

typedef unsigned long long fib_type; // let's use a large type to store fib numbers
struct fib_context;                  // forward declaration

// declaration of compute step class
struct fib_step
{
    // TODO1: declaration of execute method goes here
    int execute( const fib_type & tag, fib_context & c ) const;
};

// declaration of control step class
struct fib_ctrl
{
    // TODO1: declaration of execute method goes here
    int execute( const fib_type & tag, fib_context & c ) const;
};

#define ON_PROC( _t ) ( (_t) % numProcs() )

// tuner for the computation; used to declare dependencies to items
struct fib_tuner : public CnC::step_tuner<> // TODO3: derive from default tuner
{
    // TODO3: declaration of depends method goes here
    template< class dependency_consumer >
    void depends( const fib_type & tag, fib_context & c, dependency_consumer & dC ) const;
    // TODO4: declaration of preschedule method goes here
    // if you use XGet in step::execute, we need this to fully utilize its power
    bool preschedule() const { return false; }
    static int compute_on( const fib_type & tag, fib_context & /*arg*/ )
    {
        return ON_PROC( tag );
    }
    int affinity( const fib_type & tag, fib_context & ctxt ) const
    {
		std::cerr << "aff\n";
      return tag < 4 ? CnC::AFFINITY_HERE : tag % numThreads( ctxt );
    }
};

struct item_tuner : public CnC::hashmap_tuner
{
    typedef fib_type tag_type;
#ifndef _DIST_
    int get_count( const tag_type & tag ) const
    {
        return tag > 0 ? 2 : 1;
    }
#endif
    const std::vector< int > & consumed_on( const fib_type & tag ) const
    {
        // test returning consumer vector, also signaling UNKNOWN with empty vector (if tag < 5)
        if( tag > 4 ) return m_vecs[ON_PROC( tag )];
        else {
            if( tag == 1 ) {
                static const std::vector< int > _tmp1( 1, CnC::CONSUMER_ALL  );
                return _tmp1;
            } else if( tag == 2 && numProcs() > 2 ) {
                static const std::vector< int > _tmp2( 1, CnC::CONSUMER_ALL_OTHERS  );
                return _tmp2;
            }
            static const std::vector< int > _tmp( 0 );
            return _tmp;
        }
    }

    int produced_on( const fib_type & tag ) const
    {
        return tag > 1 ? ON_PROC( tag ) : CnC::PRODUCER_UNKNOWN;
    }

    // this is not thread-safe, but we know what we do
    item_tuner( int np ) : m_vecs( np ) { init( np ); }
    //    item_tuner( const item_tuner & t ) : m_vecs( t.m_vecs ) {}

private:
    typedef std::vector< std::vector< int > > vec_type;

    item_tuner() { CNC_ASSERT( 0 ); }

    // this is not thread-safe, but we know what we do
    void init( int np )
    {
        m_vecs.resize( np );
        for( int i = 0; i != np; ++i ) {
            m_vecs[i].resize( 2 );
            m_vecs[i][0] = ON_PROC( i+1 );
            int _tmp( ON_PROC( i+2 ) );
            if( _tmp == myPid() ) {
                m_vecs[i][1] = m_vecs[i][0];
                m_vecs[i][0] = myPid();
            } else {
                m_vecs[i][1] = _tmp;
            }
        }
    }

    vec_type m_vecs;
};

// this is our context containing collections and defining their depndencies
struct fib_context : public CnC::context< fib_context > // TODO1: derive from CnC::context
{
    item_tuner                                  m_itemTuner;
    CnC::step_collection< fib_step, fib_tuner > m_fibSteps;
    CnC::step_collection< fib_ctrl >            m_ctrlSteps;
    CnC::tag_collection< fib_type, CnC::preserve_tuner< fib_type > > m_tags; // tag collection to control steps
    CnC::item_collection< fib_type, fib_type, item_tuner >           m_fibs; // item collection holding the fib number(s)

    fib_context()                                       // constructor
        : CnC::context< fib_context >(),
          m_itemTuner( CnC::tuner_base::numProcs() ),
          m_fibSteps( *this, "fib" ),
          m_ctrlSteps( *this, "ctrl" ),
          // TODO1: tag collection to control steps
          m_tags( *this, "tags" ),                         // pass context to collection constructors
          m_fibs( *this, "fibs", m_itemTuner )
    {
        m_tags.prescribes( m_fibSteps, *this );  // m_tags prescribe compute step which use a tuner
        m_tags.prescribes( m_ctrlSteps, *this ); // m_tags also prescribe control step
        // TODO2:   print scheduler statistics when done
        // CnC::debug::collect_scheduler_statistics( *this ); // print scheduler statistics when done
#ifndef __DIST_
        CnC::debug::trace_all( *this, 3 );
#endif

    }
    // TODO?:   trace/debug output for tags
    // TODO?:   trace/debug output for compute steps
    // TODO?:   trace/debug output for compute steps
};

// TODO1: the actual step code computing the fib numbers goes here
int fib_step::execute( const fib_type & tag, fib_context & ctxt ) const
{
    if( tag > 1 ) {
        fib_type f_1; ctxt.m_fibs.unsafe_get( tag - 1, f_1 );
        fib_type f_2; ctxt.m_fibs.unsafe_get( tag - 2, f_2 );
        ctxt.flush_gets();
        ctxt.m_fibs.put( tag, f_1 + f_2 );
    } else {
        ctxt.m_fibs.put( tag, tag );
    }
    return CnC::CNC_Success;
}

// TODO1: the step code controlling (recursive) tag creation goes here
int fib_ctrl::execute( const fib_type & tag, fib_context & ctxt ) const
{
    if( tag > 1 ) {
        ctxt.m_tags.put( tag - 1 );
        ctxt.m_tags.put( tag - 2 );
    }
    return CnC::CNC_Success;
}


// TODO3: implementation of fib_tuner::depends goes here
// the tuner can be used to declare a step's dependencies prior to its actual execution
template< class dependency_consumer >
void fib_tuner::depends( const fib_type & tag, fib_context & c, dependency_consumer & dC ) const
{
    // declare dependencies
    // including producer processes, except for 0 and 1, which should be requested via bcast
    // note that all tags > 4 will be pushed through item-tuner!
    if( tag > 1 ) {
        assert( compute_on( tag - 1, c ) >= 0 );
        dC.depends( c.m_fibs, tag - 1 );
        assert( compute_on( tag - 2, c ) >= 0 );
        dC.depends( c.m_fibs, tag - 2 );
    }
}

int main( int argc, char* argv[] )
{
#if 0
#ifdef _WIN32
    _putenv( "CNC_SCHEDULER=FIFO_STEAL" );
    _putenv( "CNC_USE_AFFINITY=1" );
#else
    setenv( "CNC_SCHEDULER", "FIFO_STEAL", 1 );
    setenv( "CNC_USE_AFFINITY", "1", 1 );
#endif
#endif
#ifdef _DIST_
    CnC::dist_cnc_init< fib_context > _dinit;
#endif
    if( argc < 2 ) {
        std::cerr << "usage: fib n\n";
        return 1;
    }
    fib_type n = atol( argv[1] );

    tbb::tick_count start2 = tbb::tick_count::now(); // start timer
    // TODO1: create context
    fib_context ctxt;                                // create context
    // TODO:1 put tag to initiate evaluation
    ctxt.m_tags.put( n );                            // put tag to initiate evaluation
    // TODO:1 wait for completion
    ctxt.wait();                                     // wait for completion
    tbb::tick_count stop2 = tbb::tick_count::now();  // stop timer
    // TODO1: get result
    fib_type res2;
    ctxt.m_fibs.get( n, res2 );                     // get result
    std::cout << "CnC recursive (" << n << "): " << res2 << std::endl;
    std::cerr << "time: " << (stop2 - start2).seconds() << " s\n";

    return 0;
}
