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
#ifndef _HALO_TILE_H_
#define _HALO_TILE_H_

#include "rtm_tags.h"
#include "tile.h"
#include <cnc/itac.h>

// *******************************************************************************************
// *******************************************************************************************
// a little helper class for halo/tile management
class halo_tile
{
public:
    // *******************************************************************************************
    // getting and initing a tile and its halo
    template< typename TILE_COLL, typename XHALO_COLL, typename YHALO_COLL, typename ZHALO_COLL
#ifdef SECOND_ORDER_HALO
              , typename XYHALO_COLL, typename YZHALO_COLL, typename XZHALO_COLL
#endif
              >
    inline void get_tile_and_halo( const loop_tag & tag, const int BNX, const int BNY, const int BNZ,
                                   const int NX, const int NY, const int NZ,
                                   const TILE_COLL & tiles, const XHALO_COLL & xhalos, const YHALO_COLL & yhalos, const ZHALO_COLL & zhalos,
#ifdef SECOND_ORDER_HALO
                                   const XYHALO_COLL & xyhalos, const YZHALO_COLL & yzhalos, const XZHALO_COLL & xzhalos,
#endif
                                   int field = 0 )
    {
        VT_FUNC( "RTM:get_tile_and_halo" );

        const int t1 = tag.t-1;
        const tile_tag tag1( t1, tag.x, tag.y, tag.z, field );
        // get the tile from t-1
        tiles.get( tag1, tile );
        
        // if not doing the first time step, we need the halos and update the border
        if( t1 >= 0 ) {
            if( tag1.z < BNZ-1 ) zhalos.get( halo_tag( tag1, UP ), m_up );
            if( tag1.y < BNY-1 ) yhalos.get( halo_tag( tag1, NORTH ), m_north  );
            if( tag1.x < BNX-1 ) xhalos.get( halo_tag( tag1, EAST ), m_east );
            if( tag1.z > 0 ) zhalos.get( halo_tag( tag1, DOWN ), m_down );
            if( tag1.y > 0 ) yhalos.get( halo_tag( tag1, SOUTH ), m_south );
            if( tag1.x > 0 ) xhalos.get( halo_tag( tag1, WEST ), m_west );
#ifdef SECOND_ORDER_HALO
            if( tag.z > 0 ) {
                if( tag.y > 0 )     yzhalos.get( halo_tag( tag1, SOUTH_DOWN ), m_sd );
                if( tag.x > 0 )     xzhalos.get( halo_tag( tag1, WEST_DOWN ),  m_wd );
                if( tag.x < BNX-1 ) xzhalos.get( halo_tag( tag1, EAST_DOWN ),  m_ed );
                if( tag.y < BNY-1 ) yzhalos.get( halo_tag( tag1, NORTH_DOWN ), m_nd );
            }
            if( tag.y > 0 ) {
                if( tag.x > 0 )     xyhalos.get( halo_tag( tag1, SOUTH_WEST ), m_sw );
                if( tag.x < BNX-1 ) xyhalos.get( halo_tag( tag1, SOUTH_EAST ), m_se );
            }
            if( tag.y < BNY-1 ) {
                if( tag.x > 0 )     xyhalos.get( halo_tag( tag1, NORTH_WEST ), m_nw );
                if( tag.x < BNX-1 ) xyhalos.get( halo_tag( tag1, NORTH_EAST ), m_ne );
            }
            if( tag.z < BNZ-1 ) {
                if( tag.y > 0 )     yzhalos.get( halo_tag( tag1, SOUTH_UP ), m_su );
                if( tag.x > 0 )     xzhalos.get( halo_tag( tag1, WEST_UP ),  m_wu );
                if( tag.x < BNX-1 ) xzhalos.get( halo_tag( tag1, EAST_UP ),  m_eu );
                if( tag.y < BNY-1 ) yzhalos.get( halo_tag( tag1, NORTH_UP ), m_nu );
            }
#endif
            tiles.get( tile_tag( t1-1, tag.x, tag.y, tag.z, field ), prev );
            // this is kind of a hack: the borders/halo-region of this tile is only used within this step-instance
            // so it's safe to update the halo; all we do is avoid a copy; we don't semantically violate DSA
            {
                VT_FUNC( "RTM:update_halo" );

                const_cast< tile_type * >( tile.get() )->update_halo( prev.get(), NX, NY, NZ, m_north.get(), m_east.get(), m_south.get(), m_west.get(), m_up.get(), m_down.get()
#ifdef SECOND_ORDER_HALO
                                                                      , m_nd.get(), m_nu.get(), m_sd.get(), m_su.get()
                                                                      , m_ne.get(), m_se.get(), m_sw.get(), m_nw.get()
                                                                      , m_wd.get(), m_ed.get(), m_wu.get(), m_eu.get()
#endif
                                                                      );
            }

            // this is kind of a hack: we know this must be the last use of tile fromt t-2
            // this is an attribute of the algorithm and depends on nothing else
            // so it's safe to re-use the tile here; semantically no vilation of DSA
            // in the future CnC might provide a dedicated feature for this kind of scenario
            next = reinterpret_cast< std::shared_ptr< tile_type > & >( prev );
        } else { //first time step, no halo update needed
            prev = tile;
            next.reset( new tile_type );
            VT_FUNC( "RTM:update_halo" );
            next->update_halo( prev.get(), NX, NY, NZ, NULL, NULL, NULL, NULL, NULL, NULL
#ifdef SECOND_ORDER_HALO
                               , NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
#endif
                               );
        }
    }
    
private:
    // *******************************************************************************************
    // a little helper function to simplify the code below
    template< typename Halo >
    inline std::shared_ptr< const Halo > & copy( direction_type dir, const std::shared_ptr< tile_type > & tile, std::shared_ptr< const Halo > & halo,
                                                 const int NX, const int NY, const int NZ )
    {
        VT_FUNC( "RTM:copy_halo" );

        if( halo.get() ) const_cast< Halo * >( halo.get() )->copy( dir, tile.get(), NX, NY, NZ );
        else halo.reset( new Halo( dir, tile.get(), NX, NY, NZ ) );
        return halo;
    }
    
public:
    // *******************************************************************************************
    // putting a tile, updating and putting its halo
    template< typename TILE_COLL, typename XHALO_COLL, typename YHALO_COLL, typename ZHALO_COLL
#ifdef SECOND_ORDER_HALO
              , typename XYHALO_COLL, typename YZHALO_COLL, typename XZHALO_COLL
#endif
              >
    inline void put_tile_and_halo( const loop_tag & tag_, const int BNX, const int BNY, const int BNZ, const int T,
                                   const int NX, const int NY, const int NZ,
                                   TILE_COLL & tiles, XHALO_COLL & xhalos, YHALO_COLL & yhalos, ZHALO_COLL & zhalos,
#ifdef SECOND_ORDER_HALO
                                   XYHALO_COLL & xyhalos, YZHALO_COLL & yzhalos, XZHALO_COLL & xzhalos,
#endif
                                   int field = 0 )
    {
        VT_FUNC( "RTM:put_tile_and_halo" );

        tile_tag tag( tag_, field );
        // put the result for time t
        
        // now do the halo business
        // we could formulate it in a more compact manner
        // but we want to the same trick as above:
        // we know from the algorithm, that each halo is used only once, so we can re-use the halo objects to store our border
        if( tag.t < T ) {
            if( tag.z > 0 )     zhalos.put( halo_tag( tag.t, tag.x, tag.y, tag.z-1, UP, tag.f ),    copy( DOWN, next, m_down, NX, NY, NZ ) );
            if( tag.y > 0 )     yhalos.put( halo_tag( tag.t, tag.x, tag.y-1, tag.z, NORTH, tag.f ), copy( SOUTH, next, m_south, NX, NY, NZ ) );
            if( tag.x > 0 )     xhalos.put( halo_tag( tag.t, tag.x-1, tag.y, tag.z, EAST, tag.f ),  copy( WEST, next, m_west, NX, NY, NZ ) );
            if( tag.x < BNX-1 ) xhalos.put( halo_tag( tag.t, tag.x+1, tag.y, tag.z, WEST, tag.f ),  copy( EAST, next, m_east, NX, NY, NZ ) ); 
            if( tag.y < BNY-1 ) yhalos.put( halo_tag( tag.t, tag.x, tag.y+1, tag.z, SOUTH, tag.f ), copy( NORTH, next, m_north, NX, NY, NZ ) );
            if( tag.z < BNZ-1 ) zhalos.put( halo_tag( tag.t, tag.x, tag.y, tag.z+1, DOWN, tag.f ),  copy( UP, next, m_up, NX, NY, NZ ) );
#ifdef SECOND_ORDER_HALO
            if( tag.z > 0 ) {
                if( tag.y > 0 )     yzhalos.put( halo_tag( tag.t, tag.x,   tag.y-1, tag.z-1, NORTH_UP, tag.f ), copy( SOUTH_DOWN, next, m_sd, NX, NY, NZ ) );
                if( tag.x > 0 )     xzhalos.put( halo_tag( tag.t, tag.x-1, tag.y,   tag.z-1, EAST_UP,  tag.f ), copy( WEST_DOWN,  next, m_wd, NX, NY, NZ ) );
                if( tag.x < BNX-1 ) xzhalos.put( halo_tag( tag.t, tag.x+1, tag.y,   tag.z-1, WEST_UP,  tag.f ), copy( EAST_DOWN,  next, m_ed, NX, NY, NZ ) );
                if( tag.y < BNY-1 ) yzhalos.put( halo_tag( tag.t, tag.x,   tag.y+1, tag.z-1, SOUTH_UP, tag.f ), copy( NORTH_DOWN, next, m_nd, NX, NY, NZ ) );
            }
            if( tag.y > 0 ) {
                if( tag.x > 0 )     xyhalos.put( halo_tag( tag.t, tag.x-1, tag.y-1, tag.z, NORTH_EAST, tag.f ), copy( SOUTH_WEST, next, m_sw, NX, NY, NZ ) );
                if( tag.x < BNX-1 ) xyhalos.put( halo_tag( tag.t, tag.x+1, tag.y-1, tag.z, NORTH_WEST, tag.f ), copy( SOUTH_EAST, next, m_se, NX, NY, NZ ) );
            }
            if( tag.y < BNY-1 ) {
                if( tag.x > 0 )     xyhalos.put( halo_tag( tag.t, tag.x-1, tag.y+1, tag.z, SOUTH_EAST, tag.f ), copy( NORTH_WEST, next, m_nw, NX, NY, NZ ) );
                if( tag.x < BNX-1 ) xyhalos.put( halo_tag( tag.t, tag.x+1, tag.y+1, tag.z, SOUTH_WEST, tag.f ), copy( NORTH_EAST, next, m_ne, NX, NY, NZ ) );
            }
            if( tag.z < BNZ-1 ) {
                if( tag.y > 0 )     yzhalos.put( halo_tag( tag.t, tag.x,   tag.y-1, tag.z+1, NORTH_DOWN, tag.f ), copy( SOUTH_UP, next, m_su, NX, NY, NZ ) );
                if( tag.x > 0 )     xzhalos.put( halo_tag( tag.t, tag.x-1, tag.y,   tag.z+1, EAST_DOWN,  tag.f ), copy( WEST_UP,  next, m_wu, NX, NY, NZ ) );
                if( tag.x < BNX-1 ) xzhalos.put( halo_tag( tag.t, tag.x+1, tag.y,   tag.z+1, WEST_DOWN,  tag.f ), copy( EAST_UP,  next, m_eu, NX, NY, NZ ) );
                if( tag.y < BNY-1 ) yzhalos.put( halo_tag( tag.t, tag.x,   tag.y+1, tag.z+1, SOUTH_DOWN, tag.f ), copy( NORTH_UP, next, m_nu, NX, NY, NZ ) );
            }
#endif
            tiles.put( tag, std::shared_ptr< const tile_type >( next ) );
        }
    }

    std::shared_ptr< const tile_type > tile, prev;
    std::shared_ptr< tile_type > next;
private:
    std::shared_ptr< const tile_type::xhalo_type > m_east, m_west;
    std::shared_ptr< const tile_type::yhalo_type > m_north, m_south;
    std::shared_ptr< const tile_type::zhalo_type > m_down, m_up;
#ifdef SECOND_ORDER_HALO
    std::shared_ptr< const tile_type::xyhalo_type > m_nw, m_ne, m_sw, m_se;
    std::shared_ptr< const tile_type::xzhalo_type > m_wd, m_wu, m_ed, m_eu;
    std::shared_ptr< const tile_type::yzhalo_type > m_nd, m_nu, m_sd, m_su;
#endif
};

#endif //_HALO_TILE_H_
