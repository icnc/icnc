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
/// RTM kernel and initilization logic
#include <fstream>
#include "stencil.h"
#include "halo_tile.h"
#include "stencil.h"

class my_context;

// *******************************************************************************************
// *******************************************************************************************
// Step declarations
// stencil and initer are defined in "user"-code

// our stencil core
struct stencil_step
{
    int execute( const loop_tag & t, my_context & c ) const;
};

// initing the matrix
struct matrix_init
{
    int execute( const loop_tag & t, my_context & c ) const;
};

class my_context : public stencil_context< my_context, stencil_step, matrix_init >
{
public:
    my_context( int nx = 0, int ny = 0, int nz = 0, int t = -1, dist_type dt = BLOCKED_3D )
        : stencil_context< my_context, stencil_step, matrix_init >( nx, ny, nz, t, dt )
    {
        this->coeff_x[4] = -1.0f / 560.0f;
        this->coeff_x[3] = 8.0f / 315;
        this->coeff_x[2] = -0.2f;
        this->coeff_x[1] = 1.6f;
        this->coeff_x[0] = -1435.0f / 504 * 3;
    }
};

#ifdef USE_SIMD
# if defined(_MSC_VER) || defined(WIN32)
#  define ADD_SIMD() __pragma("simd")
# else
#  define ADD_SIMD() _Pragma("simd")
# endif 
#else
# define ADD_SIMD()
#endif

// *******************************************************************************************
// this is our actual stencil kernel
#define STENCIL_KERNEL( _htile, _x, _xe, _y, _z )                  \
    const float * const pTile = (_htile).tile->m_array;                 \
    const float * const pPrev = (_htile).prev->m_array;                 \
    float * const pNext = (_htile).next->m_array;                       \
    int c = (_htile).tile->idx((_x),(_y),(_z));                         \
    int n1 = (_htile).tile->idx((_x),(_y)-1,(_z));                      \
    int n2 = (_htile).tile->idx((_x),(_y)-2,(_z));                      \
    int n3 = (_htile).tile->idx((_x),(_y)-3,(_z));                      \
    int n4 = (_htile).tile->idx((_x),(_y)-4,(_z));                      \
    int s1 = (_htile).tile->idx((_x),(_y)+1,(_z));                      \
    int s2 = (_htile).tile->idx((_x),(_y)+2,(_z));                      \
    int s3 = (_htile).tile->idx((_x),(_y)+3,(_z));                      \
    int s4 = (_htile).tile->idx((_x),(_y)+4,(_z));                      \
    int u1 = (_htile).tile->idx((_x),(_y),(_z)-1);                      \
    int u2 = (_htile).tile->idx((_x),(_y),(_z)-2);                      \
    int u3 = (_htile).tile->idx((_x),(_y),(_z)-3);                      \
    int u4 = (_htile).tile->idx((_x),(_y),(_z)-4);                      \
    int b1 = (_htile).tile->idx((_x),(_y),(_z)+1);                      \
    int b2 = (_htile).tile->idx((_x),(_y),(_z)+2);                      \
    int b3 = (_htile).tile->idx((_x),(_y),(_z)+3);                      \
    int b4 = (_htile).tile->idx((_x),(_y),(_z)+4);                      \
    ADD_SIMD()                                                          \
    for( int x = _x; x < _xe; ++x ) {                                   \
        pNext[c] = 2 * pTile[c] - pPrev[c] + 0.001f *                   \
            (  c0 * pTile[c]                                            \
               + c1 * ((pTile[c+1] + pTile[c-1])                        \
                       + (pTile[s1] + pTile[n1])                        \
                       + (pTile[b1] + pTile[u1]))                       \
               + c2 * ((pTile[c+2] + pTile[c-2])                        \
                       + (pTile[s2] + pTile[n2])                        \
                       + (pTile[b2] + pTile[u2]))                       \
               + c3 * ((pTile[c+3] + pTile[c-3])                        \
                       + (pTile[s3] + pTile[n3])                        \
                       + (pTile[b3] + pTile[u3]))                       \
               + c4 * ((pTile[c+4] + pTile[c-4])                        \
                       + (pTile[s4] + pTile[n4])                        \
                       + (pTile[b4] + pTile[u4]))                       \
               );                                                       \
        c++; n1++; n2++; n3++; n4++; s1++; s2++; s3++; s4++;            \
        u1++; u2++; u3++; u4++; b1++; b2++; b3++; b4++;                 \
    }

// *******************************************************************************************
// this is the actual stencil computation
// executed once per tile and time-step
int stencil_step::execute( const loop_tag & tag, my_context & c ) const
{
#ifdef CNC_WITH_ITAC
    VT_FUNC( "RTM::stencil" );
#endif
    const int NX = tag.x < c.BNX - 1 ? BSIZEX : c.BBSX;
    const int NY = tag.y < c.BNY - 1 ? BSIZEY : c.BBSY;
    const int NZ = tag.z < c.BNZ - 1 ? BSIZEZ : c.BBSZ;

    halo_tile htile;
    htile.get_tile_and_halo( tag, c.BNX, c.BNY, c.BNZ, NX, NY, NZ,
                             c.m_tiles, c.m_xhalos, c.m_yhalos, c.m_zhalos );
              

    float c0 = c.coeff_x[0], c1 = c.coeff_x[1], c2 = c.coeff_x[2], c3 = c.coeff_x[3], c4 = c.coeff_x[4];

#ifndef VBSZZ
# define VBSZZ BSIZEZ
#endif
 
    CnC::parallel_for( 0, NZ, (const int)VBSZZ, [=]( int bz ) {
            for( int z = bz; z < NZ && z < bz+VBSZZ; ++z ) {
                for( int y = 0; y < NY; ++y ) {
                    STENCIL_KERNEL( htile, 0, NX, y, z );
                }
            }
      }, CnC::pfor_tuner< false >() );

#ifdef DAMPING
    CnC::parallel_for( 0, NZ, (const int)VBSZZ, [=,&c]( int bz ) {
            int dampx_beg = 0;
            int dampx_end = NX;
            int dampy_beg = 0;
            int dampy_end = NY;
            
            int _start = tag.x * BSIZEX;
            if( _start < DSIZE ) {
                dampx_beg = _start + NX >= DSIZE ? DSIZE : NX;
                for( int z = bz; z < NZ && z < bz+VBSZZ; ++z ) {
                    for( int y = 0; y < NY; ++y ) {
                        STENCIL_KERNEL( htile, 0, dampx_beg, y, z );
                    }
                }
            }
            int _damp = c.Nx - DSIZE;
            if( _start + NX > _damp ) {
                dampx_end = _start < _damp ? _damp - _start : 0;
                for( int z = bz; z < NZ && z < bz+VBSZZ; ++z ) {
                    for( int y = 0; y < NY; ++y ) {
                        STENCIL_KERNEL( htile, dampx_end, NX, y, z );
                    }
                }
            }

            _start = tag.y * BSIZEY;
            if( _start < DSIZE ) {
                dampy_beg = _start + NY >= DSIZE ? DSIZE : NY;
                for( int z = bz; z < NZ && z < bz+VBSZZ; ++z ) {
                    for( int y = 0; y < dampy_beg; ++y ) {
                        STENCIL_KERNEL( htile, dampx_beg, dampx_end, y, z );
                    }
                }
            }
            _damp = c.Ny - DSIZE;
            if( _start + NY > _damp ) {
                dampy_end = _start < _damp ? _damp - _start : 0;
                for( int z = bz; z < NZ && z < bz+VBSZZ; ++z ) {
                    for( int y = dampy_end; y < NY; ++y ) {
                        STENCIL_KERNEL( htile, dampx_beg, dampx_end, y, z );
                    }
                }
            }

            _start = bz + tag.z * BSIZEZ;
            if( _start < DSIZE ) {
                int dampz_beg = _start + NZ >= DSIZE ? DSIZE : NZ;
                for( int z = bz; z < dampz_beg && z < bz+VBSZZ; ++z ) {
                    for( int y = dampy_beg; y < dampy_end; ++y ) {
                        STENCIL_KERNEL( htile, dampx_beg, dampx_end, y, z );
                    }
                }
            }
            _damp = c.Nz - DSIZE;
            if( _start + VBSZZ > _damp ) {
                int dampz_beg = _start < _damp ? _damp - _start : 0;
                int dampz_end = _start + VBSZZ >= NX ? NX : _start + VBSZZ;
                for( int z = dampz_beg; z < dampz_end; ++z ) {
                    for( int y = dampy_beg; y < dampy_end; ++y ) {
                        STENCIL_KERNEL( htile, dampx_beg, dampx_end, y, z );
                    }
                }
            }
      }, CnC::pfor_tuner< false >() );
#endif // DAMPING

    htile.put_tile_and_halo( tag, c.BNX, c.BNY, c.BNZ, c.T, NX, NY, NZ,
                             c.m_tiles, c.m_xhalos, c.m_yhalos, c.m_zhalos );

    // now let's put the next tag (if not at end of time)
    int t1 = tag.t + 1;
	if( t1 < c.T ) {
		c.m_tags.put( loop_tag( t1, tag.x, tag.y, tag.z ) );
	}
    return 0;
}


// *******************************************************************************************
// init the matrix
// executed once per tile; inits the tile-core and its borders (halo-region)
int matrix_init::execute( const loop_tag & tag, my_context & c ) const
{
#ifdef CNC_WITH_ITAC
    VT_FUNC( "init" );
#endif
    const int NX = tag.x < c.BNX - 1 ? BSIZEX : c.BBSX;
    const int NY = tag.y < c.BNY - 1 ? BSIZEY : c.BBSY;
    const int NZ = tag.z < c.BNZ - 1 ? BSIZEZ : c.BBSZ;
    const int Nx = c.Nx + 2*HSIZE;
    const int Ny = c.Ny + 2*HSIZE;
    const int Nz = c.Nz + 2*HSIZE;

    std::shared_ptr< tile_type > tile( new tile_type );

    for (int z = 0; z < NZ+2*HSIZE; ++z) {
        for (int y = 0; y < NY+2*HSIZE; ++y) {
            for (int x = 0; x < NX+2*HSIZE; ++x) {
                /* set initial values */

                float r = fabs((float)( x + tag.x*BSIZEX - Nx/2 + y + tag.y*BSIZEY - Ny/2 + z + tag.z*BSIZEZ - Nz/2) / 30);
                r = std::max(1 - r, 0.0f) + 1;
                assert( r != 0.0f );
                (*tile)(x-HSIZE,y-HSIZE,z-HSIZE) = r;
            }
        }
    }
    c.m_tiles.put( tag, std::shared_ptr< const tile_type >( tile ) );
    return 0;
}

#define MK_STR_( _x ) #_x
#define MK_STR( _x ) MK_STR_(_x)

// *******************************************************************************************
// main; read command line args and start stencil
int main(int argc, char *argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > dc_init;
#endif
    int Nx = 100;
    int Ny = 100;
    int Nz = 20;
    int T = 40;
    dist_type dt = BLOCKED_3D;

    if (argc > 3) {
        Nx = atoi(argv[1]);
        Ny = atoi(argv[2]);
        Nz = atoi(argv[3]);
    }  
    if (argc > 4)
        T = atoi(argv[4]);
    if( argc > 5 )
        dt = static_cast< dist_type >( atoi( argv[5] ) );

    // create CnC context and init problem
    my_context _ctxt( Nx, Ny, Nz, T, dt );
    // run it
    _ctxt.run();

#ifdef OUTFILE
    std::ofstream of( MK_STR( OUTFILE ) );
    _ctxt.print_y( of );
    of.close();
#else
    _ctxt.print_y( std::cout );
#endif
#ifdef DUMP
    _ctxt.dump( "ptile.out" );
#endif

    return 0;
}
