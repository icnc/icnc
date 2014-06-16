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
//
#include "rtm_stencil_tiled.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "util_time.h"

int stencil_tuner::get_count( const loop_tag & tag ) const
{
    if( tag.t == m_t ) return CnC::NO_GETCOUNT;
    if( tag.t == -1 ) return 2;
    int _gc = 8; // getcount is symmteric!
    if( tag.z >= m_ntz-1 ) --_gc;
    if( tag.y >= m_nty-1 ) --_gc;
    if( tag.x >= m_ntx-1 ) --_gc;
    if( tag.z <= 0 ) --_gc;
    if( tag.y <= 0 ) --_gc;
    if( tag.x <= 0 ) --_gc;
    if( tag.t == m_t - 1 ) --_gc; // prev
    // getcount incorrect at boundaries if T == 0
    return _gc;
}

int stencil_step::execute( const loop_tag & tag, stencil_context & c ) const
{
    std::shared_ptr< const tile_type > prev, tile, down, up, north, east, south, west;
    int t1 = tag.t-1;
    if( tag.t > 0 ) {
        if( tag.z < c.BNZ-1 ) c.m_tiles.get( loop_tag( t1, tag.x, tag.y, tag.z + 1 ), up );
        if( tag.y < c.BNY-1 ) c.m_tiles.get( loop_tag( t1, tag.x, tag.y + 1, tag.z ), north  );
        if( tag.x < c.BNX-1 ) c.m_tiles.get( loop_tag( t1, tag.x + 1, tag.y, tag.z ), east );
        if( tag.z > 0 ) c.m_tiles.get( loop_tag( t1, tag.x, tag.y, tag.z - 1 ), down );
        if( tag.y > 0 ) c.m_tiles.get( loop_tag( t1, tag.x, tag.y - 1, tag.z ), south );
        if( tag.x > 0 ) c.m_tiles.get( loop_tag( t1, tag.x - 1, tag.y, tag.z ), west );
    }
    c.m_tiles.get( loop_tag( t1, tag.x, tag.y, tag.z ), tile );
    if( t1 >= 0 ) c.m_tiles.get( loop_tag( t1-1, tag.x, tag.y, tag.z ), prev );
    else prev = tile;
    if( t1 > 0 ) const_cast< tile_type * >( tile.get() )->update_halo( north.get(), east.get(), south.get(), west.get(), up.get(), down.get() );
    std::shared_ptr< tile_type > next( new tile_type( tile.get(), north.get(), east.get(), south.get(), west.get(), up.get(), down.get() ) );
    
    float c0 = c.coef[0], c1 = c.coef[1], c2 = c.coef[2], c3 = c.coef[3], c4 = c.coef[4];
    for( int z = 0; z < BSIZE; ++z ) {
        for( int y = 0; y < BSIZE; ++y ) {
            //#pragma simd
            for( int x = 0; x < BSIZE; ++x ) {
                float div = c0 * (*tile)(x,y,z) 
                    + c1 * (((*tile)(x+1,y,z) + (*tile)(x-1,y,z))
                            + ((*tile)(x,y+1,z) + (*tile)(x,y-1,z))
                            + ((*tile)(x,y,z+1) + (*tile)(x,y,z-1)))
                    + c2 * (((*tile)(x+2,y,z) + (*tile)(x-2,y,z))
                            + ((*tile)(x,y+2,z) + (*tile)(x,y-2,z))
                            + ((*tile)(x,y,z+2) + (*tile)(x,y,z-2)))
                    + c3 * (((*tile)(x+3,y,z) + (*tile)(x-3,y,z))
                            + ((*tile)(x,y+3,z) + (*tile)(x,y-3,z))
                            + ((*tile)(x,y,z+3) + (*tile)(x,y,z-3)))
                    + c4 * (((*tile)(x+4,y,z) + (*tile)(x-4,y,z))
                            + ((*tile)(x,y+4,z) + (*tile)(x,y-4,z))
                            + ((*tile)(x,y,z+4) + (*tile)(x,y,z-4)));
                (*next)(x,y,z) = 2 * (*tile)(x,y,z) - (*prev)(x,y,z) + 0.001f /*vsq*/ * div;
                assert( (*next)(x,y,z) != 0.0f );
            }
        }
    }
    c.m_tiles.put( tag, std::shared_ptr< const tile_type >( next ) );
    t1 = tag.t + 1;
	if( t1 < c.T ) {
		c.m_tags.put( loop_tag( t1, tag.x, tag.y, tag.z ) );
	}
    return 0;
}

int matrix_init::execute( const loop_tag & tag, stencil_context & c ) const
{
    std::shared_ptr< tile_type > tile = std::make_shared< tile_type >();
    for (int z = 0; z < BSIZE+2*HSIZE; ++z) {
        for (int y = 0; y < BSIZE+2*HSIZE; ++y) {
            for (int x = 0; x < BSIZE+2*HSIZE; ++x) {
                assert( tile->idx(x-HSIZE,y-HSIZE,z-HSIZE) >= 0 && tile->idx(x-HSIZE,y-HSIZE,z-HSIZE) < c.Nx*c.Ny*c.Nz );
                /* set initial values */
                float r = fabs((float)((x+tag.x*BSIZE) - c.Nx/2 + (y+tag.y*BSIZE) - c.Ny/2 + (z+tag.z*BSIZE) - c.Nz/2) / 30);
                r = std::max(1 - r, 0.0f) + 1;
                assert( r != 0.0f );
                (*tile)(x-HSIZE,y-HSIZE,z-HSIZE) = r;
                //                            c.vsq[c.Nx * c.Ny * (z+bz*BSIZE) + c.Nx * (y+by*BSIZE) + (x+bx*BSIZE)]= 0.001f;  
            }
        }
    }
    c.m_tiles.put( tag, std::shared_ptr< const tile_type >( tile ) );
    return 0;
}

void init_variables( stencil_context & c ) 
{
    assert( BSIZE >= 4 );
    c.coef[4] = -1.0f / 560.0f;
    c.coef[3] = 8.0f / 315;
    c.coef[2] = -0.2f;
    c.coef[1] = 1.6f;
    c.coef[0] = -1435.0f / 504 * 3;

    for( int z = 0; z < c.BNZ; ++z ) {
        for( int y = 0; y < c.BNY; ++y ) {
            for( int x = 0; x < c.BNX; ++x ) {
                c.m_initTags.put( loop_tag( -1, x, y, z ) );
            }
        }
    }
}

void print_summary( stencil_context & c, const char *header, double interval ) {
    /* print timing information */
    //    long total = (long)c.Nx * c.Ny * c.Nz;
    printf("++++++++++++++++++++ %s ++++++++++++++++++++++\n", header);
    // printf("first non-zero numbers\n");
    // for(int i = 0; i < total; i++) {
    //     if(A[i] != 0) {
    //         printf("%d: %f\n", i, A[i]);
    //         break;
    //     }
    // }
  
    double mul = c.Nx-8;
    mul *= c.Ny-8;
    mul *= c.Nz-8;
    mul *= c.T;
    double perf = mul / (interval * 1e6);
    printf("Benchmark time: %f\n", interval);
    printf("Perf: %f Mcells/sec (%f M-FAdd/s, %f M-FMul/s)\n", 
           perf, 
           perf * 26, 
           perf * 7);    
}

void print_y( stencil_context & c, std::ostream & os )
{
    int bz = (c.Nz/2)/BSIZE;
    int bx = (c.Nx/2)/BSIZE;
    int z  = (c.Nz/2)%BSIZE;
    int x  = (c.Nx/2)%BSIZE;

    os.precision(4);os.setf(std::ios::fixed,std::ios::floatfield);
    for( int y = -HSIZE; y < 0; ++y ) os << 1.0 << "\n";
    for( int by = 0; by < c.BNY; by++) {
        std::shared_ptr< const tile_type > tile;
        c.m_tiles.get( loop_tag( c.T-1, bx, by, bz ), tile );
        const int NY = BSIZE;//by < c.BNY - 1 ? BSIZE : c.BBSY;
        for( int y = 0; y < NY; ++y ) os << (*tile)(x,y,z) << "\n";
    }
    for( int y = 0; y < HSIZE; ++y ) os << 1.0 << "\n";
    printf("Done writing output\n");
}

void dotest( int Nx, int Ny, int Nz, int T )
{
    //initialization
    double start, stop;

    stencil_context _ctxt( Nx, Ny, Nz, T );
    init_variables( _ctxt );

    start = getseconds();

    if( T > 0 ) {
        for( int z = 0; z < _ctxt.BNZ; ++z ) {
            for( int y = 0; y < _ctxt.BNY; ++y ) {
                for( int x = 0; x < _ctxt.BNX; ++x ) {
                    _ctxt.m_tags.put( loop_tag( 0, x, y, z ) );
                }
            }
        }
        _ctxt.wait();
    }

    stop = getseconds();
    //    float *A = copy_out( _ctxt );
    print_summary( _ctxt, "stencil_loop", stop - start);
    print_y( _ctxt, std::cout );
}

int main(int argc, char *argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< stencil_context > dc_init;
#endif
    int Nx = 100;
    int Ny = 100;
    int Nz = 20;
    int T = 40;
    if (argc > 3) {
        Nx = atoi(argv[1]);
        Ny = atoi(argv[2]);
        Nz = atoi(argv[3]);
    }  
    if (argc > 4)
        T = atoi(argv[4]);
 
    printf( "Order-%d 3D-Stencil (%d points) with space %dx%dx%d and time %d\n", 
            HSIZE, HSIZE*2*3+1, Nx, Ny, Nz, T);
	
    dotest( Nx, Ny, Nz, T );
    return 0;
}

// float * copy_out( stencil_context & c ) 
// {
//     float * A = new float[c.Nx * c.Ny * c.Nz];
//     for (int bz = 0; bz < c.BNZ; ++bz) {
//         for (int by = 0; by < c.BNY; ++by) {
//             for (int bx = 0; bx < c.BNX; ++bx) {
//                 std::shared_ptr< const tile_type > tile = NULL;
//                 c.m_tiles.get( loop_tag( c.T, bx, by, bz ), tile );
//                 for (int z = 0; z < BSIZE; ++z) {
//                     for (int y = 0; y < BSIZE; ++y) {
//                         for (int x = 0; x < BSIZE; ++x) {
//                             A[(x+HSIZE+bx*BSIZE)+(y+HSIZE+by*BSIZE)*c.Nx+(z+HSIZE+bz*BSIZE)*c.Nx*c.Ny] = (*tile)(x,y,z);
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     // Halo(border)
//     for( int z = 0; z < c.Nz; ++z ) {
//         for( int y = 0; y < c.Ny; ++y ) {
//             for( int x = 0; x < c.Nx; ++x ) {
//                 if( y < HSIZE || y >= c.Ny-HSIZE || z < HSIZE || z >= c.Nz-HSIZE || x < HSIZE || x >= c.Nx-HSIZE ) {
//                     float r = fabs((float)(x - c.Nx/2 + y - c.Ny/2 + z - c.Nz/2) / 30);
//                     r = std::max(1 - r, 0.0f) + 1;
//                     A[x+y*c.Nx+z*c.Nx*c.Ny] =1.0;// r;
//                 }
//             }
//         }
//     }
//     return A;
// }

