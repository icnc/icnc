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

struct mandel_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    int compute_on( const int & t ) const
    {
        return t % numProcs();
    }

    int compute_on( const pair & t, my_context & /*arg*/ ) const
    {
        return compute_on( t.first );
    }
    int get_count( const int & t ) const
    {
        return 1;
    }
    int consumed_on( const int & tag  ) const
    {
        return compute_on( tag );
    }
};

struct res_tuner : public CnC::hashmap_tuner
{
    int consumed_on( const int & tag  ) const
    {
        return 0;
    }
};

struct my_context : public CnC::context< my_context >
{
    typedef std::shared_ptr< const std::vector< complex > > item_type;
    typedef std::shared_ptr< const std::vector< int > > pixel_type;
    CnC::step_collection< ComputeMandel, mandel_tuner >  m_steps;
    CnC::tag_collection< pair >                          m_position;
    CnC::item_collection< int, item_type, mandel_tuner > m_data;
    CnC::item_collection< int, pixel_type, res_tuner >   m_pixel;
    my_context() 
        : CnC::context< my_context >(),
          m_steps( *this, "mandel" ),
          m_position( *this ),
          m_data( *this, "data" ),
          m_pixel( *this, "pixel" )
    {
        m_position.prescribes( m_steps, *this );
        CnC::debug::trace( m_data );
        CnC::debug::trace( m_steps );
    }
};

int max_depth = 10000; // FIXME global variables are EVIL


int mandel(const complex &c)
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


int ComputeMandel::execute( const pair & t, my_context &c) const
{
    my_context::item_type _v;
    c.m_data.get( t.first, _v );

    my_context::pixel_type _p = std::make_shared< std::vector< int > >( t.second );
    for( int i = 0; i < t.second; ++i ) {
        const_cast< int & >( (*_p)[i] ) = mandel( (*_v)[i] );
    }
    c.m_pixel.put( t.first, _p );

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
#ifndef _DIST_
        // you can't change a global variable when using distributed memory
        max_depth = atoi(argv[ai+2]);
#else
        assert( atoi(argv[ai+2]) == max_depth );
#endif
     }
    else
    {
        fprintf(stderr,"Usage: mandel [-v] rows columns max_depth\n");
        return -1;
    }

    complex z(1.0,1.5);
    std::cout << mandel(z) << std::endl;
    
    double r_origin = -2;
    double r_scale = 4.0/max_row;
    double c_origin = -2.0;
    double c_scale = 4.0/max_col;
    int *pixels = new int[max_row*max_col];


    my_context c;

    tbb::tick_count t0 = tbb::tick_count::now();
    
    //    for (int i = 0; i < max_row; i++) 
        CnC::parallel_for( 0, max_row, 1, [&c,max_col,r_scale,r_origin,c_scale,c_origin]( int i )
        {
            my_context::item_type _v = std::make_shared< const std::vector< complex > >( max_col );
            for (int j = 0; j < max_col; j++ ) {
                const_cast< complex & >( (*_v)[j] ) = complex(r_scale*j + r_origin,c_scale*i + c_origin);
            }
            c.m_data.put(i,_v);
            c.m_position.put(pair(i,max_col));
        }, CnC::pfor_tuner< false >() );


    c.wait();

    for (int i = 0; i < max_row; i++) 
        // CnC::parallel_for( 0, max_row, 1, [&c,max_col,pixels]( int i )
        {
            my_context::pixel_type _v;
            c.m_pixel.get( i, _v ); 
            for (int j = 0; j < max_col; j++ ) {
                pixels[i*max_col + j] = (*_v)[j];
            } 
        }//, CnC::pfor_tuner< false >() );

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

