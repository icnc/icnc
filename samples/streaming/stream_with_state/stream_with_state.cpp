//********************************************************************************
// Copyright (c) 2007-2013 Intel Corporation. All rights reserved.              **
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

// This simple example of how to emulate a streaming model in CnC.
// State is updated functionally (write new version to an item
// collection).  Streams are tagged with sequence numbers.  The
// problem is that the current runtime cannot deallocate old stream
// elements.

//   Ryan Newton 7/2009


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tbb/tick_count.h"
#include <cnc/cnc.h>

int STEPS = 100;

using namespace CnC;
using namespace std;

struct TunerTy : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    typedef int tag_type;
    int get_count( const tag_type & tag ) const
    { return 1; }
};

typedef tag_collection<int>         timechan;
typedef item_collection<int, int, TunerTy >   datachan;
typedef item_collection< int, int, TunerTy >  state_t;

//typedef pair<intchan, intchan> twochans;
//typedef pair<state, intchan>        state_and_chans;

struct my_context;
//struct my_context : public CnC::context< my_context >;

// Altogether an actor needs to access its input channel (item
// collection) and its state.  Tags serve only as time counters.
struct actor_package {
  state_t state;
  datachan data_in;
  datachan data_out;
  timechan tag_in;
  timechan tag_out;

  actor_package( CnC::context< my_context >& c )  
    : tag_out(c), tag_in(c),
      data_out(c), data_in(c), state(c)
  {
  }
};


//==============================================================================
// Actor definitions:

// Variant 1:
// Writing these as structs and passing pointers to their state to execute.

struct StreamActor
{
  //  int execute( int n, twochans& c ) const;
    int execute( int n, struct actor_package& c ) const;
};


//int StreamActor::execute( int n, twochans & chans ) const
int StreamActor::execute( int iter, struct actor_package& pkg ) const
{
    //printf("  Actor fire, iter %d\n", iter);
    int input;
    pkg.data_in.get(iter, input);
    int state;
    pkg.state.get(iter, state);
    
    pkg.state.put(iter + 1, state + 1 ); 
    pkg.tag_in.put(iter + 1);  // start next StreamActor when state is ready
    
    pkg.data_out.put(iter, input + state );
    pkg.tag_out.put(iter);

    //printf("    input %d, state %d, pushed %d onward...\n", input, state, input+state);
    return CnC::CNC_Success;
}



struct StreamSink
{  
  int execute( int n, struct actor_package& c ) const;
};


int StreamSink::execute( int iter, struct actor_package & pkg ) const
{
    int input;
    pkg.data_out.get(iter, input);
    if ((iter % 100000) == 0) printf("  Sink received %d\n", input);
    return CnC::CNC_Success;
}

//==============================================================================
// Graph Construction:

// Function to construct a pipeline of actors:
//void pipeline(twochans& chans) {   }

struct my_context : public CnC::context< my_context >
{
    // For now, make one stream actor and one sink:    
    CnC::step_collection< StreamActor, TunerTy > m_steps1;
    CnC::step_collection< StreamSink, TunerTy > m_steps2;

    struct actor_package pkg1;

    my_context()
        : CnC::context< my_context >(),
          m_steps1( *this ),
          m_steps2( *this ),
          pkg1(*this)
    { 
        // Connect source to first actor:
        pkg1.tag_in.prescribes( m_steps1, pkg1);
        pkg1.tag_out.prescribes( m_steps2, pkg1);
    }
};

int ResidentMemory()
{
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) { printf("(Couldn't read /proc/self/statm for resident memory.)\n"); return 0; }
    int total, resident, share, trs, drs, lrs, dt;
    fscanf(f,"%d %d %d %d %d %d %d", &total, &resident, &share, &trs, &drs, &lrs, &dt);
    return resident;
}

int main(int argc, char* argv[])
{
    if (argc >= 2) STEPS = atoi(argv[1]);

    //printf ("main starting\n");
    my_context c;

    tbb::tick_count t0 = tbb::tick_count::now();
    c.pkg1.state.put( 0, 0 );

    // There's no flow control on this source:

    for (int i=0; i<STEPS; i++) {
        c.pkg1.data_in.put(i, 100+i );
    }

    // Only enable the first step.  Otherwise, we will create STEPS
    // number of requeued steps, waiting for their state.

    c.pkg1.tag_in.put(0);  

    //for (int i=0; i<STEPS; i++) {
    //    c.pkg1.tag_in.put(i);
    //}
    
    c.wait();
    tbb::tick_count t1 = tbb::tick_count::now();
    printf("time: %f\n",(t1-t0).seconds());
    printf("MB: %f\n", double((4096*ResidentMemory()))/1000000.0);
}
