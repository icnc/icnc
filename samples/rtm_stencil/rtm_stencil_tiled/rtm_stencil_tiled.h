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
#ifndef _TILE_H_INCLUDED_
#define _TILE_H_INCLUDED_

#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include "tile.h"
#include <memory>

#ifndef USE_BSIZE
# define USE_BSIZE 123
#endif
#ifndef USE_HSIZE
# define USE_HSIZE 4
#endif
enum { BSIZE = USE_BSIZE,
       HSIZE = USE_HSIZE };

struct loop_tag
{
    loop_tag( int t_ = -1, int x_ = -1, int y_ = -1, int z_ = -1 ) : t( t_ ), x( x_ ), y( y_ ), z( z_ ) {}
    bool operator==( const loop_tag & c ) const { return t == c.t && x == c.x && y == c.y && z == c.z; }
    int t, x, y, z;
};

std::ostream & cnc_format( std::ostream& os, const loop_tag & t )
{
    os << "(" << t.t << "," << t.x << "," << t.y << "," << t.z << ")";
    return os;
}

template <>
struct cnc_hash< loop_tag >
{
    size_t operator()(const loop_tag& tt) const
    {
        return ( (tt.t+3) << 23 ) + ( tt.x << 11 ) + ( tt.y << 19 ) + ( tt.z << 3 );
    }
};

class stencil_context;

struct stencil_step
{
    int execute( const loop_tag & t, stencil_context & c ) const;
};

struct matrix_init
{
    int execute( const loop_tag & t, stencil_context & c ) const;
};

typedef Tile3d< BSIZE, HSIZE > tile_type;

CNC_BITWISE_SERIALIZABLE( loop_tag );

struct stencil_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    stencil_tuner( int ntx = 0, int nty = 0, int ntz = 0, int t = 0 );

    int compute_on( const loop_tag & t, stencil_context & ) const;

    template< class DC >
    void depends( const loop_tag & tag, stencil_context & c, DC & dc ) const;

    std::vector< int > consumed_on( const loop_tag & tag ) const;

    int get_count( const loop_tag & tag ) const;

private:
    int m_nProcs;            // number of partitions/processes
    int m_npx, m_npy, m_npz; // partition-grid (#partitions per dimension)
    int m_psx, m_psy, m_psz; // # blocks per partition and dimension
    int m_ntx, m_nty, m_ntz; // # of blocks per dimension
    int m_t;
};
CNC_BITWISE_SERIALIZABLE( stencil_tuner );


class stencil_context : public CnC::context< stencil_context >
{
public:
    stencil_context( int nx = 0, int ny = 0, int nz = 0, int t = -1 )
        : Nx( nx ),
          Ny( ny ),
          Nz( nz ),
          T( t ),
          BNX( ( nx + BSIZE - 1 - 2*HSIZE ) / BSIZE ),
          BNY( ( ny + BSIZE - 1 - 2*HSIZE ) / BSIZE ),
          BNZ( ( nz + BSIZE - 1 - 2*HSIZE ) / BSIZE ),
          m_tuner( BNX, BNY, BNZ, t ),
          m_steps( *this, m_tuner ),
          m_matrixInit( *this, m_tuner ),
          m_tags( *this ),
          m_initTags( *this ),
          m_tiles( *this, m_tuner )
    {
        assert( nx == 0 || ( BNX * BSIZE == Nx - 2*HSIZE &&  BNY * BSIZE == Ny - 2*HSIZE &&  BNZ * BSIZE == Nz - 2*HSIZE )
                || ( BNX * BSIZE == Nx &&  BNY * BSIZE == Ny &&  BNZ * BSIZE == Nz ) );
        m_tags.prescribes( m_steps, *this );
        m_initTags.prescribes( m_matrixInit, *this );
        // if( CnC::tuner_base::myPid() == 0 ) CnC::debug::trace( m_tiles );
        // if( CnC::tuner_base::myPid() == 0 )  CnC::debug::trace( m_steps );
    }

    int Nx, Ny, Nz, T, BNX, BNY, BNZ;
    float coef[HSIZE+1];
    stencil_tuner                                        m_tuner;
    CnC::step_collection< stencil_step, stencil_tuner >  m_steps;
    CnC::step_collection< matrix_init,  stencil_tuner >  m_matrixInit;
    CnC::tag_collection< loop_tag >                      m_tags;
    CnC::tag_collection< loop_tag >                      m_initTags;
    CnC::item_collection< loop_tag, std::shared_ptr< const tile_type >, stencil_tuner > m_tiles;

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        float * tmp = coef;
        ser & m_tuner
            & Nx & Ny & Nz
            & T
            & BNX & BNY & BNZ
            & CnC::chunk< float, CnC::no_alloc >( tmp, HSIZE+1 );
    }
#endif
};


stencil_tuner::stencil_tuner( int ntx, int nty, int ntz, int t )
    : m_nProcs( numProcs() ),
      m_npx( m_nProcs ), m_npy( 1 ), m_npz( 1 ), 
      m_psx( 1 ), m_psy( 1 ), m_psz( 1 ),
      m_ntx( ntx ), m_nty( nty ), m_ntz( ntz ),
      m_t( t )
{
    assert( m_nProcs == 1 || ( ntx * nty * ntz ) % m_nProcs == 0 );
    if( m_nProcs > 1 && ntx ) {
        assert( nty && ntz );
        
        int _factor = 0;
        while( ( _factor = m_npx % 2 == 0 ? 2 : 0 ) > 0 && _factor * m_npy < m_npx ) {
            m_npx /= _factor;
            m_npy *= _factor;
            std::swap( m_npy, m_npz );
        }
        assert( m_npx * m_npy * m_npz == m_nProcs );
        m_psx = ntx / m_npx;
        m_psy = nty / m_npy;
        m_psz = ntz / m_npz;
        assert( m_psx * m_npx * m_psy * m_npy * m_psz * m_npz == ntx * nty * ntz );
        std::cerr << m_npx << "x" << m_npy << "x" << m_npz << " partitions of " << m_psx << "x" << m_psy << "x" << m_psz << std::endl;
    } 
}

#define PROC_FOR_TILE( _x, _y, _z, _sx, _sy, _sz, _nx, _ny, _np )       \
    ( ( ( (_z) / (_sz) ) * (_nx) * (_ny) + ( (_y) / (_sy) ) * (_nx) + ( (_x) / (_sx) ) ) % (_np) )

int stencil_tuner::compute_on( const loop_tag & t, stencil_context & ) const
{
    //  if( t.t < 0 ) {
    //     tbb::queuing_mutex::scoped_lock _lock( ::CnC::Internal::s_tracingMutex );
    //     std::cout << t.x << " " << t.y << " " << t.z << " on " << PROC_FOR_TILE( t.x, t.y, t.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs ) << std::endl;
    // }
    return PROC_FOR_TILE( t.x, t.y, t.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
}


template< class DC >
void stencil_tuner::depends( const loop_tag & tag, stencil_context & c, DC & dc ) const
{
    int t1 = tag.t-1;
    if( t1 < 0 ) return; // init step
    // declare dependencies to tiles
    // step execution will be delayed until all dependencies are satisfied (by puts)
    if( tag.t > 0 ) {
        if( tag.z < m_ntz-1 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y, tag.z + 1 ) );
        if( tag.y < m_nty-1 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y + 1, tag.z ) );
        if( tag.x < m_ntx-1 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x + 1, tag.y, tag.z ) );
        if( tag.z > 0 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y, tag.z - 1 ) );
        if( tag.y > 0 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y - 1, tag.z ) );
        if( tag.x > 0 ) dc.depends( c.m_tiles, loop_tag( t1, tag.x - 1, tag.y, tag.z ) );
    }
    dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y, tag.z ) );
    if( t1 >= 0 ) dc.depends( c.m_tiles, loop_tag( t1-1, tag.x, tag.y, tag.z ) );
}

#define addit( _p, _set, _all ) \
        if( _set[_p] == false ) {\
             _all.push_back( _p );\
             _set[_p] = true;\
        }

std::vector< int > stencil_tuner::consumed_on( const loop_tag & tag ) const
{
    std::vector< int > _all;
    if( tag.t == m_t ) {
        _all.resize( 1 );
        _all[0] = CnC::CONSUMER_UNKNOWN;
        return _all;
    }
    int _me = PROC_FOR_TILE( tag.x, tag.y, tag.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
    if( tag.t < 0 ) {
        _all.resize( 1 );
        _all[0] = _me;
        return _all;
    }

    _all.reserve( 4 );
    _all.push_back( _me );
    bool * _set = new bool[numProcs()];
    memset( _set, 0, sizeof( *_set ) * numProcs() );
    _set[_me] = true;

    if( tag.z > 0 ) {
        int _tmp = PROC_FOR_TILE( tag.x, tag.y, tag.z - 1, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    if( tag.y > 0 ) {
        int _tmp = PROC_FOR_TILE( tag.x, tag.y - 1, tag.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    if( tag.x > 0 ) {
        int _tmp = PROC_FOR_TILE( tag.x - 1, tag.y, tag.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    if( tag.z < m_ntz-1 ) {
        int _tmp = PROC_FOR_TILE( tag.x, tag.y, tag.z + 1, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    if( tag.y < m_nty-1 ) {
        int _tmp = PROC_FOR_TILE( tag.x, tag.y + 1, tag.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    if( tag.x < m_ntx-1 ) {
        int _tmp = PROC_FOR_TILE( tag.x + 1, tag.y, tag.z, m_psx, m_psy, m_psz, m_npx, m_npy, m_nProcs );
        addit( _tmp, _set, _all );
    }
    
    delete [] _set;
    return _all;
}

#endif // _TILE_H_INCLUDED_
