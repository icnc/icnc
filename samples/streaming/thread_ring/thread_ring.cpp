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


// This simple microbenchmark is drawn from the "Great Language Shootout".
// It passes token(s) around a ring.

// This version is implemented with a separate step collection for
// each "actor" in the ring.

// The executable can be called with 1-3 arguments:
//   ./ThreadRing.exe [HOPS [AGENTS [TOKENS]]]

//   Ryan Newton 6/2009


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tbb/tick_count.h"
#include <cnc/cnc.h>

int HOPS   = 17;
int AGENTS = 503;
int TOKENS = 1;

using namespace CnC;

struct RingActor
{
  int execute( int n, tag_collection<int> & c ) const;
};

#define TunerTy CnC::step_tuner<>


item_collection<long,int>* actor_ids;

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< RingActor, TunerTy > m_steps;
    tag_collection<int>** outchans;

    my_context() : CnC::context< my_context >(), m_steps( *this )
    {
      // Lame, because it's global:
      actor_ids = new item_collection<long,int>(*this);

      outchans = new tag_collection<int>* [AGENTS];
      for (int i=0; i<AGENTS; i++) {
        outchans[i] = new tag_collection<int>(*this);
        // Connect to the previous actor in the ring:
        if (i != 0) 
          outchans[i-1]->prescribes( m_steps, *outchans[i] );
          // Associate the ID number with this pointer:
          actor_ids->put((long)outchans[i-1], i-1);
      }
      // Close the loop:
      outchans[AGENTS-1]->prescribes( m_steps, *outchans[0] );
      actor_ids->put((long)outchans[AGENTS-1], AGENTS-1);
    }
};


int RingActor::execute( int n, tag_collection<int>& outchan ) const
{
    if (n>0) outchan.put(n-1); else 
    {
        // What a hack!  Look up our ID number:
        int _tmp;
        actor_ids->get((long)&outchan, _tmp);
        printf("%d\n", _tmp);
    }

    return CnC::CNC_Success;
}


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
    if (argc >= 2) HOPS   = atoi(argv[1]);
    if (argc >= 3) AGENTS = atoi(argv[2]);
    if (argc >= 4) TOKENS = atoi(argv[3]);

    my_context c;

    //printf("initializing with context %p\n", &c);
    //actor_ids(c);    

    tbb::tick_count t0 = tbb::tick_count::now();

    c.outchans[0]->put(HOPS);

    c.wait();

    tbb::tick_count t1 = tbb::tick_count::now();

    printf("time: %f\n",(t1-t0).seconds());
    printf("resident memory, mb: %f\n", double((4096*ResidentMemory()))/1000000.0);
}

