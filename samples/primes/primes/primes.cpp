//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
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
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tbb/tick_count.h>
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#include <cnc/internal/dist/distributor.h>
#else
#include <cnc/cnc.h>
#endif


struct my_context;

struct FindPrimes
{
    int execute( int n, my_context & c ) const;
};

struct primes_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    int consumed_on( const int & ) const
    {
        return 0;
    }

    int compute_on( const int & p, my_context & ) const
    {
        return( p * 5 / 3 + 1 ) % numProcs();// ( 1 + p / ( 1000000 / 20 ) ) % m_nP;//( p * 5 / 3 + 1 ) % 5;
    }
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< FindPrimes, primes_tuner > m_steps;
    CnC::tag_collection<int>                         m_tags;
    CnC::item_collection<int, int, primes_tuner>     m_primes;
    my_context() 
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_tags( *this ),
          m_primes( *this )
    {
        m_tags.prescribes( m_steps, *this );
    }
};


int FindPrimes::execute( int n, my_context & c ) const
{
    int factor = 3;

    while ( n % factor ) factor += 2;
    if (factor == n) c.m_primes.put(n, n);

    return CnC::CNC_Success;
}


int main(int argc, char* argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > dc_init;
#endif
    bool verbose = false;
    int n = 0;
    int number_of_primes = 0;

    if (argc == 2) 
    {
        n = atoi(argv[1]);
    }
    else if (argc == 3 && 0 == strcmp("-v", argv[1]))
    {
        n = atoi(argv[2]);
        verbose = true;
    }
    else
    {
        fprintf(stderr,"Usage: primes [-v] n\n");
        return -1;
    }

    my_context c;

    printf("Determining primes from 1-%d \n",n);

    tbb::tick_count t0 = tbb::tick_count::now();

    //    CnC::parallel_for( 3, n, 2, [&c]( int number )
    for (int number = 3; number < n; number += 2)
        {
            c.m_tags.put(number);
        }
    //, false );

    c.wait();

    // FIXME we have to transfer the items to the host first (distCnC)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    number_of_primes = (int)c.m_primes.size() + 1;
    tbb::tick_count t1 = tbb::tick_count::now();

    printf("Found %d primes in %g seconds\n", number_of_primes, (t1-t0).seconds());

    if (verbose)
    {
        printf("%d\n", 2);
        for (CnC::item_collection<int, int, primes_tuner>::const_iterator cii = c.m_primes.begin(); cii != c.m_primes.end(); cii++) 
        {
            printf("%d\n", cii->first); // kludge
        }
    }
}

