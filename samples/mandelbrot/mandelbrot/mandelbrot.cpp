//********************************************************************************
// Copyright (c) 2010-2021 Intel Corporation. All rights reserved.              **
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
#include <iostream>
#include <complex>
#include "tbb/tick_count.h"
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cassert>


typedef std::complex<double> complex;
typedef std::pair<int,int> pair;
struct my_context;


struct ComputeMandel
{
    int execute( const pair & p, my_context & c ) const;
};

struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< ComputeMandel > m_steps;
    CnC::tag_collection<pair> m_position;
    CnC::item_collection<pair,complex> m_data;
    CnC::item_collection<pair,int> m_pixel;
    int max_depth;

    my_context( int md = 0 ) 
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_position( *this ),
          m_data( *this ),
          m_pixel( *this ),
          max_depth( md )
    {
        m_position.prescribes( m_steps, *this );
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & max_depth;
    }
#endif
};


int mandel( const complex &c, int max_depth )
{
    int count = 0;
    complex z = 0;

    for(int i = 0; i < max_depth; i++)
    {
        if (abs(z) >= 2.0) break;
        z = z*z + c;
        count++;
    }
    return count;
}


int ComputeMandel::execute( const pair & p, my_context &c) const
{
    complex _tmp;
    c.m_data.get( p, _tmp );
    int depth = mandel( _tmp, c.max_depth );
    c.m_pixel.put(p,depth);
    return CnC::CNC_Success;
}


int main(int argc, char* argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > dc_init;
#endif
    bool verbose = false;
    int max_row = 100;
    int max_col = 100;
    int max_depth = 10000;
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
        // you can't change a global variable when using distributed memory,
        // so this is in the context and must be set before the first tag/item is put
        max_depth = atoi(argv[ai+2]);
     }
    else
    {
        fprintf(stderr,"Usage: mandel [-v] rows columns max_depth\n");
        return -1;
    }

    complex z(1.0,1.5);
    std::cout << mandel(z, max_depth) << std::endl;
    
    double r_origin = -2;
    double r_scale = 4.0/max_row;
    double c_origin = -2.0;
    double c_scale = 4.0/max_col;
    int *pixels = new int[max_row*max_col];

    // set max-depth in the constructor
    my_context c( max_depth );

    tbb::tick_count t0 = tbb::tick_count::now();
    
    for (int i = 0; i < max_row; i++) 
    {
        for (int j = 0; j < max_col; j++ )
        {
            complex z = complex(r_scale*j +r_origin,c_scale*i + c_origin);
            c.m_data.put(pair(i,j),z);
            c.m_position.put(pair(i,j));
        }
    }
    c.wait();

    c.m_pixel.begin(); //in distCnC case, this gathers all items in one go

    for (int i = 0; i < max_row; i++) 
    {
        for (int j = 0; j < max_col; j++ )
        {
            c.m_pixel.get(pair(i,j), pixels[i*max_col + j]);
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
                if (pixels[i*max_col + j] == max_depth)
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

    return 0;
}

