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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <complex>
#include "tbb/tick_count.h"
typedef std::complex<double> complex;

int mandel(complex c, int max_count)
{
    int count = 0;
    complex z = 0;

    for(int i = 0; i < max_count; i++)
    {
        if (abs(z) >= 2.0) break;
        z = z*z + c;
        count++;
    }
    return count;
}

int main(int argc, char* argv[])
{
    bool verbose = false;
    int max_row = 100;
    int max_col = 100;
    int max_depth = 200;

    int ai = 1;
    if (argc > ai && 0 == strcmp("-v", argv[ai]))
    {
        verbose = true;
        ai++;
    }
    if (argc == ai+3)
    {
        max_row = atoi(argv[ai]);
        max_col = atoi(argv[ai+1]);
        max_depth = atoi(argv[ai+2]);
     }
    else
    {
        fprintf(stderr,"Usage: mandel [-v] rows columns max_depth\n");
        return -1;
    }

    {
        complex z( 1.0, 1.5 );
        std::cout << mandel(z,max_depth) << std::endl;
    }
    double r_origin = -2;
    double r_scale = 4.0/max_row;
    double c_origin = -2.0;
    double c_scale = 4.0/max_col;
    int *pixels = new int[max_row*max_col];

    tbb::tick_count t0 = tbb::tick_count::now();
    for (int i = 0; i < max_row; i++) 
    {
        for (int j = 0; j < max_col; j++ )
        {
            complex z = complex(r_scale*j +r_origin,c_scale*i + c_origin);
            pixels[i*max_col + j] = mandel(z, max_depth);
        }
    }
    tbb::tick_count t1 = tbb::tick_count::now();
    printf("Mandel %d %d %d in %g seconds\n", max_row, max_col, max_depth,
           (t1-t0).seconds());

    int check = 0;
    for (int i = 0; i < max_row; i++) 
    {
        for (int j = 0; j < max_col; j++ )
        {
            if (pixels[i*max_col + j ] == max_depth) check += (i*max_col +j );
        }
    }
    printf("Mandel check %d \n", check);
    
    if (verbose) 
    {
        for (int i = 0; i < max_row; i++) 
        {
            for (int j = 0; j < max_col; j++ )
            {
                if (pixels[i*max_col + j ] == max_depth)
                {
                    std::cout << " ";
                }
                else
                {
                    std::cout << ".";
                }
            }
            std::cout << std::endl;
        }
    }
}

