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
#ifndef _Tile_H_
#define _Tile_H_

#include <cassert> 

namespace CnC
{
    class serializer;
}

template< int TILESZX, int HALOSZX, int TILESZY = TILESZX,  int HALOSZY = HALOSZX, int TILESZZ = TILESZX, int HALOSZZ = HALOSZX >
class Tile3d
{
public:
    typedef Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ > tile_type;
    
    Tile3d( const tile_type * prev,
            const tile_type * north, const tile_type * east, const tile_type * south,
            const tile_type * west, const tile_type * up, const tile_type * down );
    Tile3d(){}

    void update_halo( const tile_type * north, const tile_type * east, const tile_type * south,
                      const tile_type * west, const tile_type * up, const tile_type * down );

    inline int idx( int x, int y, int z ) const {
        int _idx = HALOSZX+x+(y+HALOSZY)*ARRSZX+(z+HALOSZZ)*ARRSZY;
        assert( _idx >= 0 && _idx < ARRSZ );
        return _idx;
    }
    inline float & operator()( int x, int y, int z ) {
        return m_array[idx( x,y, z )];
    }
    inline float operator()( int x, int y, int z ) const {
        return m_array[idx( x, y, z )];
    }

    void serialize( CnC::serializer & ser );

 protected:
    void copy_halo( const tile_type * north, const tile_type * east, const tile_type * south,
                      const tile_type * west, const tile_type * up, const tile_type * down );

    enum {
        ARRSZ  = (TILESZX+2*HALOSZX)*(TILESZY+2*HALOSZY)*(TILESZZ+2*HALOSZZ),
        ARRSZY = (TILESZX+2*HALOSZX)*(TILESZY+2*HALOSZY),
        ARRSZX = TILESZX+2*HALOSZX,
        OFF_000 = ARRSZY * HALOSZZ + ARRSZX * HALOSZY + HALOSZX,
        OFF_NNN = ARRSZ - OFF_000 - 1
    };
    float m_array[ARRSZ];
};

template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::Tile3d( const tile_type * prev,
                                                                        const tile_type * north, const tile_type * east,
                                                                        const tile_type * south, const tile_type * west,
                                                                        const tile_type * up, const tile_type * down )
    : m_array()
{
#ifndef NDEBUG
    memset( m_array, -1, sizeof( m_array ) );
#endif
    copy_halo( north ? NULL : prev,
               east ? NULL : prev,
               south ? NULL : prev,
               west ? NULL : prev,
               up ? NULL : prev,
               down ? NULL : prev );
}


template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
void Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::update_halo( const tile_type * north, const tile_type * east,
                                                                                  const tile_type * south, const tile_type * west,
                                                                                  const tile_type * up, const tile_type * down )
{
    if( down ) memcpy( m_array, &down->m_array[TILESZZ*ARRSZY], ARRSZY * HALOSZZ * sizeof( m_array[0] ) );
    if( up )   memcpy( &m_array[(TILESZZ+HALOSZZ)*ARRSZY], &up->m_array[HALOSZZ*ARRSZY], ARRSZY * HALOSZZ * sizeof( m_array[0] ) );

    for( int z = 0; z < TILESZZ; ++z ) {
        if( south ) memcpy( &m_array[ARRSZY*HALOSZZ+z*ARRSZY], &south->m_array[ARRSZY*HALOSZZ+ARRSZX*TILESZY+z*ARRSZY], ARRSZX * HALOSZY * sizeof( m_array[0] ) );
        if( north ) memcpy( &m_array[ARRSZY*HALOSZZ+ARRSZX*(TILESZY+HALOSZY)+z*ARRSZY], &north->m_array[ARRSZY*HALOSZZ+ARRSZX*HALOSZY+z*ARRSZY], ARRSZX * HALOSZY * sizeof( m_array[0] ) );

        for( int y = 0; y < TILESZY; ++y ) {
            if( west ) memcpy( &m_array[OFF_000-HALOSZX+y*ARRSZX+z*ARRSZY], &west->m_array[OFF_000+TILESZX-HALOSZX+y*ARRSZX+z*ARRSZY], HALOSZX * sizeof( m_array[0] ) );
            if( east ) memcpy( &m_array[OFF_000+TILESZX+y*ARRSZX+z*ARRSZY], &east->m_array[OFF_000+y*ARRSZX+z*ARRSZY], HALOSZX * sizeof( m_array[0] ) );
        }
    }
}

template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
void Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::copy_halo( const tile_type * north, const tile_type * east,
                                                                                const tile_type * south, const tile_type * west,
                                                                                const tile_type * up, const tile_type * down )
{
    if( down ) memcpy( m_array, down->m_array, ARRSZY * HALOSZZ * sizeof( m_array[0] ) );
    if( up )   memcpy( &m_array[(TILESZZ+HALOSZZ)*ARRSZY], &up->m_array[(TILESZZ+HALOSZZ)*ARRSZY], ARRSZY * HALOSZZ * sizeof( m_array[0] ) );

    for( int z = 0; z < TILESZZ; ++z ) {
        if( south ) memcpy( &m_array[ARRSZY*HALOSZZ+z*ARRSZY], &south->m_array[ARRSZY*HALOSZZ+z*ARRSZY], ARRSZX * HALOSZY * sizeof( m_array[0] ) );
        if( north ) memcpy( &m_array[ARRSZY*HALOSZZ+ARRSZX*(TILESZY+HALOSZY)+z*ARRSZY], &north->m_array[ARRSZY*HALOSZZ+ARRSZX*(TILESZY+HALOSZY)+z*ARRSZY], ARRSZX * HALOSZY * sizeof( m_array[0] ) );

        for( int y = 0; y < TILESZY; ++y ) {
            if( west ) memcpy( &m_array[OFF_000-HALOSZX+y*ARRSZX+z*ARRSZY], &west->m_array[OFF_000-HALOSZX+y*ARRSZX+z*ARRSZY], HALOSZX * sizeof( m_array[0] ) );
            if( east ) memcpy( &m_array[OFF_000+TILESZX+y*ARRSZX+z*ARRSZY], &east->m_array[OFF_000+TILESZX+y*ARRSZX+z*ARRSZY], HALOSZX * sizeof( m_array[0] ) );
        }
    }
}

// rtm-stencil has a border that's part of the array-size def -> the halo is actually initialized.
// As long as we compare results with the original version we must transfer the entire tile
// we could of course only transfer the outer halos, but that's not easy to detect here
template< int TILESZX, int HALOSZX, int TILESZY,  int HALOSZY, int TILESZZ, int HALOSZZ >
void Tile3d< TILESZX, HALOSZX, TILESZY, HALOSZY, TILESZZ, HALOSZZ >::serialize( CnC::serializer & ser )
{
    // float * _tmp = &m_array[0];
    // ser & CnC::chunk< float, CnC::no_alloc >( _tmp, ARRSZ );
    
    for( int z = 0; z < TILESZZ; ++z ) {
        for( int y = 0; y < TILESZY; ++y ) {
            float * _tmp = &m_array[idx( 0, y, z )];
            ser & CnC::chunk< float, CnC::no_alloc >( _tmp, TILESZX );
        }
    }
}

#endif // _Tile_H_
