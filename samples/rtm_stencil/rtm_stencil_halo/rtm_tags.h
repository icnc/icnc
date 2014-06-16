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
#ifndef _RTM_TAGS_H_INCLUDED_
#define _RTM_TAGS_H_INCLUDED_

// *******************************************************************************************
// tag which identifies a tile-position (no time)
struct pos_tag
{
    pos_tag( int x_ = -1, int y_ = -1, int z_ = -1 )
        : x( x_ ), y( y_ ), z( z_ ) {}
    bool operator==( const pos_tag & c ) const
    { return x == c.x && y == c.y && z == c.z; }
    int x, // x-coordinate of tile
        y, // y-coordinate of tile
        z; // y-coordinate of tile
};

// we are using non-standard types as tags -> need to provide hashing function
template <>
struct cnc_hash< pos_tag >
{
    size_t operator()(const pos_tag& tt) const;
};

// *******************************************************************************************
// tag which identifies an instance of stencil_step
struct loop_tag : public pos_tag
{
    loop_tag( int t_ = -1, int x_ = -1, int y_ = -1, int z_ = -1 )
        : pos_tag( x_, y_, z_ ), t( t_ ) {}
    loop_tag( const tag_range_type::const_iterator & r )
        : t( 0 ), pos_tag( (*r)[0], (*r)[1], (*r)[2] ) {}
    bool operator==( const loop_tag & c ) const
    { return pos_tag::operator==( c ) && t == c.t; }
    int t;// time-step;
};

// we are using non-standard types as tags -> need to provide hashing function
template <>
struct cnc_hash< loop_tag >
{
    size_t operator()(const loop_tag& tt) const;
};


// *******************************************************************************************
// tag which identifies a tile-instance
struct tile_tag : public loop_tag
{
    tile_tag( int t_ = -1, int x_ = -1, int y_ = -1, int z_ = -1, int f_ = 0 )
        : loop_tag( t_, x_, y_, z_ ), f( f_ ) {}
    tile_tag( const loop_tag & lt, int f_ = 0 )
        : loop_tag( lt ), f( f_ ) {}
    bool operator==( const tile_tag & c ) const
    { return loop_tag::operator==( c ) && f == c.f; }
    int f;// field-identifier
};

// we are using non-standard types as tags -> need to provide hashing function
template <>
struct cnc_hash< tile_tag >
{
    size_t operator()(const tile_tag& tt) const;
};


// *******************************************************************************************
// tag which identifies a halo-object
// a halo object represents data of a neighboring tile in one dimension (each tile has up to 6 halos)
// a halo-tag is just a specialization of loop-tag (similar to halo and tile)
// adds direction
struct halo_tag : public tile_tag
{
    halo_tag( const tile_tag & lt, direction_type d_ = NORTH )
        : tile_tag( lt ), d( d_ ) {}
    halo_tag(  int t_ = -1, int x_ = -1, int y_ = -1, int z_ = -1, direction_type d_ = NORTH, int f_ = 0)
        : tile_tag( t_, x_, y_, z_, f_ ), d( d_ ) {}
    bool operator==( const halo_tag & c ) const
    { return tile_tag::operator==( c ) && d == c.d; }
    direction_type d; // NORTH, SOUTH, WEST, EAST, UP, DOWN
};

// we are using non-standard types as tags -> need to provide hashing function
template <>
struct cnc_hash< halo_tag > 
{
    size_t operator()(const halo_tag& tt) const;
};


// *******************************************************************************************
// *******************************************************************************************
// implementations

size_t cnc_hash< pos_tag >::operator()(const pos_tag& tt) const
{
    return ( ( tt.x << 7 ) + ( tt.y << 15 ) + ( tt.z ) );
}

// *******************************************************************************************

size_t cnc_hash< loop_tag >::operator()(const loop_tag& tt) const
{
    return ( (tt.t+3) << 19 ) + ( tt.x << 7 ) + ( tt.y << 13 ) + ( tt.z );
}

// *******************************************************************************************

size_t cnc_hash< tile_tag >::operator()(const tile_tag& tt) const
{
    return ( ( (tt.t+3) << 19 ) + ( tt.x << 7 ) + ( tt.y << 13 ) + ( tt.z ) + ( tt.f << 23 ) );
}

// *******************************************************************************************

size_t cnc_hash< halo_tag >::operator()(const halo_tag& tt) const
{
    return ( ( (tt.t+3) << 19 ) + ( tt.x << 7 ) + ( tt.y << 13 ) + ( tt.z ) + ( tt.f << 23 ) + ((tt.d+7) << 27) );
}

// *******************************************************************************************
// *******************************************************************************************

std::ostream & cnc_format( std::ostream& os, const pos_tag & t )
{
    os << "(" << t.x << "," << t.y << "," << t.z << ")";
    return os;
}

std::ostream & cnc_format( std::ostream& os, const loop_tag & t )
{
    os << "(" << t.t << "," << t.x << "," << t.y << "," << t.z << ")";
    return os;
}

std::ostream & cnc_format( std::ostream& os, const tile_tag & t )
{
    os << "(" << t.t << "," << t.x << "," << t.y << "," << t.z << "," << t.f << ")";
    return os;
}

std::ostream & cnc_format( std::ostream& os, const halo_tag & t )
{
    os << "(" << t.t << "," << t.x << "," << t.y << "," << t.z << "," << t.f << "," << t.d << ")";
    return os;
}

// *******************************************************************************************
// *******************************************************************************************
// Marshalling for distCnC

#ifdef _DIST_
CNC_BITWISE_SERIALIZABLE( pos_tag );
CNC_BITWISE_SERIALIZABLE( loop_tag );
CNC_BITWISE_SERIALIZABLE( tile_tag );
CNC_BITWISE_SERIALIZABLE( halo_tag );
#endif //_DIST_

#endif // _RTM_TAGS_H_INCLUDED_
