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
#include "tbb/tick_count.h"

int main(int argc, char* argv[])
{
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

    // Array to hold the primes computed
    int *primes = new int[n];
    
    printf("Determining primes from 1-%d \n",n);

    tbb::tick_count t0 = tbb::tick_count::now();

    primes[ number_of_primes++ ] = 2;

    for (int number = 3; number <= n; number += 2)
    {
        int factor = 3;
        while ( number % factor )
        {
            factor += 2;
        }
        if (factor == number)  // number is prime
        {
            primes[ number_of_primes++ ] = number;
        }
    }

    tbb::tick_count t1 = tbb::tick_count::now();

    printf("Found %d primes in %g seconds\n", number_of_primes,(t1-t0).seconds());

    if (verbose) 
    {
        // Print the prime numbers
        for (int i = 0; i < number_of_primes; i++) 
        {
            printf("%d\n", primes[i]);
        }
    }
}

