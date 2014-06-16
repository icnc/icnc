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
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

#ifndef _H_CHOLESKY_TYPES_INCLUDED_H_
#define _H_CHOLESKY_TYPES_INCLUDED_H_

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <iostream>
#include <cassert>

typedef enum {
    BLOCKED_ROWS = 0,
    ROW_CYCLIC = 1,
    COLUMN_CYCLIC = 2,
    BLOCKED_CYCLIC = 3
} dist_type;

typedef std::pair<int,int> pair;
// don't use a vector: tags are copied and vector-copy is expensive
class triple
{
public:
    triple() { m_arr[0] = m_arr[1] = m_arr[2] = 0; }
    triple( int a, int b, int c ) { m_arr[0] = a; m_arr[1] = b; m_arr[2] = c; }
    inline int operator[]( int i )  const { return m_arr[i]; }
private:
    int m_arr[3];
};

template <>
struct cnc_hash< triple >
{
    size_t operator()( const triple & t ) const { return t[0] + ( t[1] << 11 ) + ( t[2] << 17 ); }
};

template<>
struct cnc_equal< triple >
{
    bool operator()( const triple & a, const triple & b) const { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }
};

inline std::ostream & cnc_format( std::ostream& os, const triple & t )
{
    os << "[ " << t[0] << ", " << t[1] << ", " << t[2] << " ]";
    return os;
}

template< typename T >
class Tile
{
public:
    Tile( int sz = 0 ) : m_sz( sz ), m_array( NULL )/*, m_full( true )*/
    { if( sz ) CnC::serializer::construct_array< T >( m_array, sz*sz ); }
    ~Tile() { CnC::serializer::destruct_array< T >( m_array, m_sz*m_sz ); }
#define TOI( _i, _j, _s ) ((_j)*(_s)+(_i))
    inline T operator()( int i, int j ) const { return m_array[TOI(i,j,m_sz)]; }
    inline T & operator()( int i, int j ) { return m_array[TOI(i,j,m_sz)]; }
    inline const T * get_array() const { return m_array; }
    inline T * get_array() { return m_array; }
    inline void copy( const Tile< T > & t ) {assert( m_sz == t.m_sz ); memcpy( m_array, t.m_array, m_sz * sizeof( *m_array ) );}
private:
    Tile( const Tile< T > & ) { assert( 0 ); }
    Tile & operator=( const Tile< T > & ) { assert( 0 ); return *this; }
    int   m_sz;
    T   * m_array;
    //    bool  m_full;
#ifdef _DIST_
public:
    void serialize( CnC::serializer & ser )
    {
        ser & m_sz;
        ser & CnC::chunk< T >( m_array, m_sz*m_sz );
    }
#endif
};

typedef Tile< double > tile_type;
typedef std::shared_ptr< const tile_type > tile_const_ptr_type;
typedef std::shared_ptr< tile_type > tile_ptr_type;

#ifdef _DIST_

CNC_BITWISE_SERIALIZABLE( triple );
CNC_BITWISE_SERIALIZABLE( dist_type );

inline void serialize( CnC::serializer & ser, std::shared_ptr< const tile_type > & ptr )
{
    tile_type * _tmp = const_cast< tile_type * >( ptr.get() );
    if( ser.is_unpacking() ) {
        assert( _tmp == NULL );
        _tmp = new tile_type;
    }
    if( ! ser.is_cleaning_up() ) ser & CnC::chunk< tile_type, CnC::no_alloc >( _tmp, 1 );
    if( ser.is_unpacking() ) {
        ptr.reset( _tmp );
    }
}
#endif

#endif //_H_CHOLESKY_TYPES_INCLUDED_H_
