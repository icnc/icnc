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
#pragma once
#ifndef _TILE_H_
#define _TILE_H_

#include <cassert> 

#ifdef WIN32
#pragma warning( disable : 4351 )
#endif

// *******************************************************************************************
// *******************************************************************************************
// helper types etc.

// avoid including the holw CnC stuff just to know the existance of serializer
namespace CnC
{
    class serializer;
}

// direction of halo, relative to tile which uses it
typedef enum { NORTH = 0x1, SOUTH = 0x2, WEST = 0x4, EAST = 0x8, UP = 0x10, DOWN = 0x20,
               NORTH_WEST = NORTH|WEST, NORTH_EAST = NORTH|EAST, NORTH_UP = NORTH|UP, NORTH_DOWN = NORTH|DOWN,
               SOUTH_WEST = SOUTH|WEST, SOUTH_EAST = SOUTH|EAST, SOUTH_UP = SOUTH|UP, SOUTH_DOWN = SOUTH|DOWN,
               EAST_UP = EAST|UP, EAST_DOWN = EAST|DOWN, WEST_UP = WEST|UP, WEST_DOWN = WEST|DOWN
} direction_type;

// forward declare the halo type
template< int SZX, int SZY, int SZZ > class Halo3d;


// *******************************************************************************************
// *******************************************************************************************
// our 3D tile type
// static size, core and border sizes can be configured individually in each dimension
template< int TILESZX, int HALOSZX, int TILESZY = TILESZX,  int HALOSZY = HALOSZX, int TILESZZ = TILESZX, int HALOSZZ = HALOSZX >
class Tile3d
{
public:
    // typedefs to facilitate use of halo and tile types
    typedef Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ > tile_type;
    typedef Halo3d< HALOSZX, TILESZY, TILESZZ > xhalo_type;
    typedef Halo3d< TILESZX, HALOSZY, TILESZZ > yhalo_type;
    typedef Halo3d< TILESZX, TILESZY, HALOSZZ > zhalo_type;
#ifdef SECOND_ORDER_HALO
    typedef Halo3d< HALOSZX, HALOSZY, TILESZZ > xyhalo_type;
    typedef Halo3d< HALOSZX, TILESZY, HALOSZZ > xzhalo_type;
    typedef Halo3d< TILESZX, HALOSZY, HALOSZZ > yzhalo_type;
#endif

    Tile3d();

    // get halo objects and update the borders(halos within this tile
    // prev is expected to be the tile at the same position of the previous time.step
    // if a given halo is NULL then this tile is on the border of the overall problem domain and the halo from prev will be used instead
    void update_halo( const tile_type * prev, const int NX, const int NY, const int NZ,
                      const yhalo_type * north, const xhalo_type * east, const yhalo_type * south,
                      const xhalo_type * west, const zhalo_type * up, const zhalo_type * down
#ifdef SECOND_ORDER_HALO
                      , const yzhalo_type * nd, const yzhalo_type * nu, const yzhalo_type * sd, const yzhalo_type * su
                      , const xyhalo_type * ne, const xyhalo_type * se, const xyhalo_type * sw, const xyhalo_type * nw
                      , const xzhalo_type * wd, const xzhalo_type * ed, const xzhalo_type * wu, const xzhalo_type * eu
#endif
                      );

    // get the (internal) index for given (x,y,z)
    inline int idx( int x, int y, int z ) const {
        int _idx = HALOSZX+x+(y+HALOSZY)*(TILESZX+2*HALOSZX)+(z+HALOSZZ)*(TILESZX+2*HALOSZX)*(TILESZY+2*HALOSZY);
        assert( _idx >= 0 && _idx < ARRSZ );
        return _idx;
    }
    
    // returns the mutable entry for given (x,y,z)
    inline float & operator()( int x, int y, int z ) {
        return m_array[idx( x, y, z )];
    }
    // returns the read-only entry for given (x,y,z)
    inline const float & operator()( int x, int y, int z ) const {
        return m_array[idx( x, y, z )];
    }
    // marshalling for distCnC
    void serialize( CnC::serializer & ser );

    // JCB Vectorization "fix"
    // OOP is confusing the compiler - give us access to the raw bits!
    // use at your own risk
    const float * array() const { return m_array; }
    
//protected:
    // a few convenience values
    enum {
        TSZX   = TILESZX,
        TSZY   = TILESZY,
        TSZZ   = TILESZZ,
        ARRSZ  = (TILESZX+2*HALOSZX)*(TILESZY+2*HALOSZY)*(TILESZZ+2*HALOSZZ)
    };
    float m_array[ARRSZ]; // the actual array, matrix
    friend class Halo3d< HALOSZX, TILESZY, TILESZZ >;
    friend class Halo3d< TILESZX, HALOSZY, TILESZZ >;
    friend class Halo3d< TILESZX, TILESZY, HALOSZZ >; 
};

// *******************************************************************************************
// *******************************************************************************************

// Our halo, we use actual halo objects
// a halo is just a specialization of tile
template< int SZX, int SZY, int SZZ >
class Halo3d : public Tile3d< SZX, 0, SZY, 0, SZZ, 0 >
{
public:
    // default constructor needed for distCnC
    Halo3d();
    // Use this in your code, don't use the default constructor
    template< typename TileType >
    Halo3d( direction_type, const TileType *, const int NX, const int NY, const int NZ );
    // initialize halo from given tile (e.g. copy the tile's border in the respective direction)
    template< typename TileType >
    void copy( direction_type, const TileType *, const int NX, const int NY, const int NZ );
    
};

// *******************************************************************************************
// *******************************************************************************************

template< int SZX, int SZY, int SZZ >
Halo3d< SZX, SZY, SZZ >::Halo3d()
    : Tile3d< SZX, 0, SZY, 0, SZZ, 0 >()
{}

// *******************************************************************************************

template< int SZX, int SZY, int SZZ >
template< typename TileType >
Halo3d< SZX, SZY, SZZ >::Halo3d( direction_type dir, const TileType * tile, const int NX, const int NY, const int NZ )
{
    copy( dir, tile, NX, NY, NZ );
}

// *******************************************************************************************

template< int SZX, int SZY, int SZZ >
template< typename TileType >
void Halo3d< SZX, SZY, SZZ >::copy( direction_type dir, const TileType * tile, const int NX, const int NY, const int NZ )
{
    int _tx = 0;
    int _ty = 0;
    int _tz = 0;

    // first determine the bounds in y and z directions
    if( dir & UP ) {
        _tz = std::max( 0, NZ-SZZ );
    }
    if( dir & NORTH ) {
        _ty = std::max( 0, NY-SZY );
    }
    if( dir & EAST ) {
        _tx = std::max( 0, NX-SZX );
    }

    const int x = 0;
    for( int z = 0; z < SZZ; ++z ) {
        // then do the copy
        for( int y = 0; y < SZY; ++y ) {
            memcpy( &(*this)( x, y, z ), &(*tile)( _tx+x, _ty+y, _tz+z ), SZX * sizeof( this->m_array[0] ) );
            //            assert( (*this)( 0, y, z ) != 0.0f );
        }
    }
}

// *******************************************************************************************
// *******************************************************************************************

template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::Tile3d()
    : m_array()
{
}

// *******************************************************************************************

template< typename TT, typename HT >
static inline void do_copy( float * tile_array, const HT * halo, const TT * prev, const int SZX, const int SZY, const int SZZ, const int xoff, const int yoff, const int zoff )
{
    const int x = 0;
    for( int z = 0; z < SZZ; ++z ) {
        for( int y = 0; y < SZY; ++y ) {
            size_t _off = prev->idx( x+xoff, y+yoff, z+zoff );
            const float * _tmp =  halo ? &(*halo)(x,y,z) : &prev->m_array[_off];
            memcpy( &tile_array[_off], _tmp, SZX * sizeof( tile_array[0] ) );
        }
    }
}

template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
void Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::update_halo( const tile_type * prev, const int NX, const int NY, const int NZ,
                                                                                  const yhalo_type * north, const xhalo_type * east, const yhalo_type * south,
                                                                                  const xhalo_type * west, const zhalo_type * up, const zhalo_type * down
#ifdef SECOND_ORDER_HALO
                                                                                  , const yzhalo_type * nd, const yzhalo_type * nu, const yzhalo_type * sd, const yzhalo_type * su
                                                                                  , const xyhalo_type * ne, const xyhalo_type * se, const xyhalo_type * sw, const xyhalo_type * nw
                                                                                  , const xzhalo_type * wd, const xzhalo_type * ed, const xzhalo_type * wu, const xzhalo_type * eu
#endif
                                                                                  )
{
    do_copy( m_array, down,  prev, TILESZX, TILESZY, HALOSZZ, 0,        0,        -HALOSZZ );
    do_copy( m_array, south, prev, TILESZX, HALOSZY, TILESZZ, 0,        -HALOSZY, 0        );
    do_copy( m_array, west,  prev, HALOSZX, TILESZY, TILESZZ, -HALOSZX, 0,        0        );
    do_copy( m_array, east,  prev, HALOSZX, TILESZY, TILESZZ, NX,       0,        0        );
    do_copy( m_array, north, prev, TILESZX, HALOSZY, TILESZZ, 0,        NY,       0        );
    do_copy( m_array, up,    prev, TILESZX, TILESZY, HALOSZZ, 0,        0,        NZ       );
#ifdef SECOND_ORDER_HALO
    do_copy( m_array, sd,    prev, TILESZX, HALOSZY, HALOSZZ, 0,        -HALOSZY, -HALOSZZ );
    do_copy( m_array, wd,    prev, HALOSZX, TILESZY, HALOSZZ, -HALOSZX, 0,        -HALOSZZ );
    do_copy( m_array, ed,    prev, HALOSZX, TILESZY, HALOSZZ, NX,       0,        -HALOSZZ );
    do_copy( m_array, nd,    prev, TILESZX, HALOSZY, HALOSZZ, 0,        NY,       -HALOSZZ );
    do_copy( m_array, sw,    prev, HALOSZX, HALOSZY, TILESZZ, -HALOSZX, -HALOSZY, 0        );
    do_copy( m_array, se,    prev, HALOSZX, HALOSZY, TILESZZ, NX,       -HALOSZY, 0        );
    do_copy( m_array, nw,    prev, HALOSZX, HALOSZY, TILESZZ, -HALOSZX, NY,       0        );
    do_copy( m_array, ne,    prev, HALOSZX, HALOSZY, TILESZZ, NX,       NY,       0        );
    do_copy( m_array, su,    prev, TILESZX, HALOSZY, HALOSZZ, 0,        -HALOSZY, NZ      );
    do_copy( m_array, wu,    prev, HALOSZX, TILESZY, HALOSZZ, -HALOSZX, 0,        NZ      );
    do_copy( m_array, eu,    prev, HALOSZX, TILESZY, HALOSZZ, NX,       0,        NZ      );
    do_copy( m_array, nu,    prev, TILESZX, HALOSZY, HALOSZZ, 0,        NY,       NZ      );
#endif
    
}

// *******************************************************************************************

// if the tile has no border, we treat is as a halo -> copy the entire thing
// if it has a border, we only copy it's core, the halo needs ecplicit updating
template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
void Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::serialize( CnC::serializer & ser )
{
    if( HALOSZX == 0 && HALOSZY == 0 && HALOSZZ == 0 ) {
        float * _tmp = &m_array[0];
        ser & CnC::chunk< float, CnC::no_alloc >( _tmp, ARRSZ );
    } else {
        const int x = 0;
        for( int z = 0; z < TILESZZ; ++z ) {
            for( int y = 0; y < TILESZY; ++y ) {
                float * _tmp = &m_array[idx( x, y, z )];
                ser & CnC::chunk< float, CnC::no_alloc >( _tmp, TILESZX );
            }
        }
    }
}

#endif // _TILE_H_

#if 0

    for( int z = 0; z < HALOSZZ; ++z ) {
        for( int y = 0; y < TILESZY; ++y ) {
            size_t _off = idx( 0, y, z-HALOSZZ );
            const float * _tmp =  down ? &(*down)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, TILESZX * sizeof( m_array[0] ) );
        }
    }
    for( int z = 0; z < HALOSZZ; ++z ) {
        for( int y = 0; y < TILESZY; ++y ) {
            size_t _off = idx( 0, y, NZ+z );
            const float * _tmp =  up ? &(*up)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, TILESZX * sizeof( m_array[0] ) );
        }
    }
        
    for( int z = 0; z < TILESZZ; ++z ) {
        for( int y = 0; y < HALOSZY; ++y ) {
            size_t _off = idx( 0, y-HALOSZY, z );
            const float * _tmp = south ? &(*south)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, TILESZX * sizeof( m_array[0] ) );
        }
    }
    for( int z = 0; z < TILESZZ; ++z ) {
        for( int y = 0; y < HALOSZY; ++y ) {
            size_t _off = idx( 0, NY+y, z );
            const float * _tmp = north ? &(*north)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, TILESZX * sizeof( m_array[0] ) );
        }
    }

    for( int z = 0; z < TILESZZ; ++z ) {
        for( int y = 0; y < TILESZY; ++y ) {
            size_t _off = idx( -HALOSZX, y, z );
            const float * _tmp = west ? &(*west)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, HALOSZX * sizeof( m_array[0] ) );
        }
    }
    for( int z = 0; z < TILESZZ; ++z ) {
        for( int y = 0; y < TILESZY; ++y ) {
            size_t _off = idx( NX, y, z );
            const float * _tmp = east ? &(*east)(0,y,z) : &prev->m_array[_off];
            memcpy( &m_array[_off], _tmp, HALOSZX * sizeof( m_array[0] ) );
        }
    }
}
#endif
