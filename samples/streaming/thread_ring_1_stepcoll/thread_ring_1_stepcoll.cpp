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

// This alternate version creates multiple "actors" as step instances
// within a single step collection.  In this case we use the tag to
// indicate which actor we're communicating with and we use an item
// collection to communicate tokens.

// The executable can be called with 1-3 arguments:
//   ./ThreadRing.exe [HOPS [AGENTS [TOKENS]]]

// -Ryan Newton [2009.06.24]


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tbb/tick_count.h"
#include <cnc/cnc.h>

#define PRINTS 0

#define tag_t int
//typedef tag_t int 

int HOPS   = 17;
int AGENTS = 503;
int TOKENS = 1;

struct my_context;

struct RingActor
{
    int execute( tag_t  n, my_context & c ) const;    
};

using namespace CnC;

struct my_tuner : public CnC::hashmap_tuner
{
    typedef tag_t tag_type;
    int get_count( const tag_type & tag ) const
    { return 1; }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< RingActor > m_steps;
    CnC::tag_collection<tag_t> tags;
    CnC::item_collection<tag_t,int> items;
    RingActor* actors;
    
    my_context() 
        : CnC::context< my_context >(), 
          m_steps( *this ),
          tags( *this ),
          items( *this )
    {
        tags.prescribes( m_steps, *this );
    }
};

int RingActor::execute( tag_t n, my_context & c ) const
{
    // HACK: we can recursively stimulate ourselves but only with UNIQUE tags:
    // This is because tags are dynamic *single* assignment.
    //int myid = n;
    tag_t myid = n % AGENTS;;

    if (PRINTS) printf("  Execute called (%p), %d my context %p\n", this, myid, &c);

    // Get our data item from the collection.
    int token;
    c.items.get(n, token);

    if (PRINTS) printf("    Token: %d\n", token);

    if (token == 0) 
      printf("%d\n", myid); 
    else 
    {
      // Sendto the next actor:
      //int next = (n+1) % AGENTS;
      tag_t next = (n+1);

      if (PRINTS) printf("    Sending %d to next: %d\n", token-1, next);
      c.items.put(next, token-1 );
      c.tags.put (next);
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
    if (PRINTS) printf("Calling main function.\n");
    my_context c;
    if (PRINTS) printf("Context initialized.\n");

    if (argc >= 2) HOPS   = atoi(argv[1]);
    if (argc >= 3) AGENTS = atoi(argv[2]);
    if (argc >= 4) TOKENS = atoi(argv[3]);

    tbb::tick_count t0 = tbb::tick_count::now();

    // Schedule initial token:
    if (PRINTS) printf("Inserting initial tag into channel %p\n", &c.tags);

    // Start off the first (only) token at actor 0:
    c.items.put( 0, HOPS );
    c.tags.put(0);
    
    c.wait();

    tbb::tick_count t1 = tbb::tick_count::now();
    printf("time: %f\n",(t1-t0).seconds());
    printf("MB: %f\n", double((4096*ResidentMemory()))/1000000.0);
}


