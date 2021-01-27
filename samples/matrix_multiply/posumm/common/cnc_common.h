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
//

#ifndef CNC_COMMON_H
#define CNC_COMMON_H
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
#include <cassert>
#include <cnc/debug.h>
#include "utils.h"


typedef enum {
    ROW_CYCLIC = 1,
    COLUMN_CYCLIC = 2,
    BLOCKED_CYCLIC = 3,
    BLOCKED_2D = 4,
    BLOCKED_ROW = 5,
    BLOCKED_COLUMN = 6,
    CANNON_BLOCK_2D = 7
} dist_type;

typedef std::pair<int, int> Pair;
// don't use a vector: tags are copied and vector-copy is expensive
class Triple
{
public:
    Triple() { m_arr[0] = m_arr[1] = m_arr[2] = 0; }
    Triple( int a, int b, int c ) { m_arr[0] = a; m_arr[1] = b; m_arr[2] = c; }
    inline int operator[]( int i )  const { return m_arr[i]; }
private:
    int m_arr[3];
};



template <>
class cnc_tag_hash_compare< Triple >
{
public:
    size_t hash( const Triple & t ) const
    {
        return (t[0] << 11) + (t[1]) + ( t[2] << 17);      
    }
    bool equal( const Triple & a, const Triple & b) const { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }
};

inline std::ostream & cnc_format( std::ostream& os, const Triple & t )
{
    os << "[ " << t[0] << ", " << t[1] << ", " << t[2] << " ]";
    return os;
}


template< typename T >
class Tile2d
{
public:
    Tile2d( int sz_x = 0, int sz_y = 0) 
    : m_sz_x( sz_x ),
      m_sz_y( sz_y ),
      m_array( 0 )
    { if( sz_x && sz_y) m_array.resize( sz_x*sz_y ); }

    ~Tile2d() {}
#define TOI( _i, _j, _s ) ((_i)*(_s)+(_j))
    inline T operator()( int i, int j ) const { return m_array[TOI(i,j,m_sz_y)]; }
    inline T & operator()( int i, int j ) { return m_array[TOI(i,j,m_sz_y)]; }
    inline T* addr() { return m_array.data(); }

private:
    Tile2d( const Tile2d< T > & ) { assert( 0 ); }
    Tile2d & operator=( const Tile2d< T > & ) { assert( 0 ); return *this; }
    
    int   m_sz_x;
    int   m_sz_y;
    std::vector< T > m_array;
public:
#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_sz_x & m_sz_y & m_array;
    }
#endif
};

#ifdef _DIST_
CNC_BITWISE_SERIALIZABLE( Triple );
CNC_BITWISE_SERIALIZABLE( dist_type );

#endif

#endif /* CNC_COMMON_H */
