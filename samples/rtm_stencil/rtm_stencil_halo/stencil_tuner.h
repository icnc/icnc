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

#include "stencil.h"


#ifndef VERIFY
// *******************************************************************************************

int stencil_tuner::get_count( const pos_tag & tag ) const
{
    return CnC::NO_GETCOUNT;
}

// *******************************************************************************************

int stencil_tuner::get_count( const tile_tag & tag ) const
{
    if( tag.t < m_t-2 ) return 2;
    if( tag.t >= m_t-1 ) return CnC::NO_GETCOUNT;
    return 1; //if( tag.t == m_t-2 ) 
}

// *******************************************************************************************

int stencil_tuner::get_count( const halo_tag & tag ) const
{
    if( tag.t < m_t-1 ) return 1;
    return 0; // t-1
}
#endif // VERIFY

// *******************************************************************************************

int stencil_tuner::proc_for_tile( int x, int y, int z ) const
{
    if( m_dt != RED_BLACK )  return ( ( z / m_psz ) * m_npx * m_npy + ( y / m_psy ) * m_npx + ( x / m_psx ) ) % m_nProcs;
    return ( z  % ( m_nProcs / 2 ) ) * 2 + ( ( x + ( y % 2 ) ) % 2 );
}

// *******************************************************************************************

template< typename Context >
int stencil_tuner::compute_on( const loop_tag & t, Context & ) const
{
    return proc_for_tile( t.x, t.y, t.z );
}

template< typename Context >
int stencil_tuner::compute_on( const tag_range_type & r, Context & c ) const
{
    return compute_on( r.begin(), c );
}

// *******************************************************************************************

template< typename Context, typename DC >
void stencil_tuner::depends( const loop_tag & tag, Context & c, DC & dc ) const
{
    int t1 = tag.t-1;
    if( t1 < 0 ) return; // init step
    // declare dependencies to tiles
    // step execution will be delayed until all dependencies are satisfied (by puts)
    if( tag.t > 0 ) {
        if( tag.z < m_ntz-1 ) dc.depends( c.m_zhalos, halo_tag( t1, tag.x, tag.y, tag.z, UP ) );
        if( tag.y < m_nty-1 ) dc.depends( c.m_yhalos, halo_tag( t1, tag.x, tag.y, tag.z, NORTH ) );
        if( tag.x < m_ntx-1 ) dc.depends( c.m_xhalos, halo_tag( t1, tag.x, tag.y, tag.z, EAST ) );
        if( tag.z > 0 ) dc.depends( c.m_zhalos, halo_tag( t1, tag.x, tag.y, tag.z, DOWN ) );
        if( tag.y > 0 ) dc.depends( c.m_yhalos, halo_tag( t1, tag.x, tag.y, tag.z, SOUTH ) );
        if( tag.x > 0 ) dc.depends( c.m_xhalos, halo_tag( t1, tag.x, tag.y, tag.z, WEST ) );
#ifdef SECOND_ORDER_HALO
            if( tag.z > 0 ) {
                if( tag.y > 0 )     dc.depends( c.m_yzhalos, halo_tag( t1, tag.x, tag.y, tag.z, SOUTH_DOWN ) );
                if( tag.x > 0 )     dc.depends( c.m_xzhalos, halo_tag( t1, tag.x, tag.y, tag.z, WEST_DOWN ) );
                if( tag.x < m_ntx-1 ) dc.depends( c.m_xzhalos, halo_tag( t1, tag.x, tag.y, tag.z, EAST_DOWN ) );
                if( tag.y < m_nty-1 ) dc.depends( c.m_yzhalos, halo_tag( t1, tag.x, tag.y, tag.z, NORTH_DOWN ) );
            }
            if( tag.y > 0 ) {
                if( tag.x > 0 )     dc.depends( c.m_xyhalos, halo_tag( t1, tag.x, tag.y, tag.z, SOUTH_WEST ) );
                if( tag.x < m_ntx-1 ) dc.depends( c.m_xyhalos, halo_tag( t1, tag.x, tag.y, tag.z, SOUTH_EAST ) );
            }
            if( tag.y < m_nty-1 ) {
                if( tag.x > 0 )     dc.depends( c.m_xyhalos, halo_tag( t1, tag.x, tag.y, tag.z, NORTH_WEST ) );
                if( tag.x < m_ntx-1 ) dc.depends( c.m_xyhalos, halo_tag( t1, tag.x, tag.y, tag.z, NORTH_EAST ) );
            }
            if( tag.z < m_ntz-1 ) {
                if( tag.y > 0 )     dc.depends( c.m_yzhalos, halo_tag( t1, tag.x, tag.y, tag.z, SOUTH_UP ) );
                if( tag.x > 0 )     dc.depends( c.m_xzhalos, halo_tag( t1, tag.x, tag.y, tag.z, WEST_UP ) );
                if( tag.x < m_ntx-1 ) dc.depends( c.m_xzhalos, halo_tag( t1, tag.x, tag.y, tag.z, EAST_UP ) );
                if( tag.y < m_nty-1 ) dc.depends( c.m_yzhalos, halo_tag( t1, tag.x, tag.y, tag.z, NORTH_UP ) );
            }
#endif
    }
    dc.depends( c.m_tiles, loop_tag( t1, tag.x, tag.y, tag.z ) );
    if( t1 >= 0 ) dc.depends( c.m_tiles, loop_tag( t1-1, tag.x, tag.y, tag.z ) );
}

// *******************************************************************************************

int stencil_tuner::consumed_on( const tile_tag & tag ) const
{
    if( tag.t == m_t ) return CnC::CONSUMER_UNKNOWN;
    return proc_for_tile( tag.x, tag.y, tag.z );
}

// *******************************************************************************************

int stencil_tuner::consumed_on( const halo_tag & tag ) const
{
    return proc_for_tile( tag.x, tag.y, tag.z );
}

// *******************************************************************************************

int stencil_tuner::consumed_on( const pos_tag & tag ) const
{
    return proc_for_tile( tag.x, tag.y, tag.z );
}

// *******************************************************************************************

stencil_tuner::stencil_tuner( int ntx, int nty, int ntz, int t, dist_type dt )
    : m_nProcs( numProcs() ),
      m_npx( m_nProcs ), m_npy( 1 ), m_npz( 1 ), 
      m_psx( ntx ), m_psy( nty ), m_psz( ntz ),
      m_ntx( ntx ), m_nty( nty ), m_ntz( ntz ),
      m_t( t ), m_dt( dt )
{
    if( m_nProcs > 1 && ntx ) {
        assert( nty && ntz );
        
        switch( dt ) {
        case BLOCKED_2D :
            while( m_npx > m_npy && m_npx % 2 == 0 ) {
                m_npx /= 2;
                m_npy *= 2;
            }
            m_psx = ntx / m_npx;
            m_psy = nty / m_npy;
            std::cerr << "distributing BLOCKED_2D\n";
            break;
        case BLOCKED_CYCLIC_3D :
            m_psx = m_psy = m_psz = 2;
            m_npx = m_ntx / m_psx;
            m_npy = m_nty / m_psy;
            m_npz = m_ntz / m_psz;
            std::cerr << "distributing BLOCKED_CYCLIC_3D\n";
            break;
        case BLOCKED_CYCLIC_2D :
            m_psx = 2;
            m_psy = 1;
            m_npx = m_ntx / m_psx;
            m_npy = m_nty / m_psy;
            std::cerr << "distributing BLOCKED_CYCLIC_2D\n";
            break;
        case CYCLIC :
            m_psx = m_psy = m_psz = 1;
            m_npx = m_ntx;
            m_npy = m_nty;
            m_npz = m_ntz;
            std::cerr << "distributing CYCLIC\n";
            break;
        case RED_BLACK:
            break;
        default :
            int _factor = 0;
            m_npx = m_npy = m_npz = 1;
            // n_p* : number of partitions/processes in *-direction
            while( m_npx * m_npy * m_npz < m_nProcs ) {
                int ranks = m_npx * m_npy * m_npz;
                int factor = m_nProcs / ranks;
                int mxx = 1 + m_nProcs*m_nProcs, mxy = mxx, mxz = mxx;
                // find smallest possible (even) divider for each dimension
                for( int i = 2; i <= factor && i <= ntx/m_npx; ++ i ) if( ( m_nProcs / ranks ) % i == 0 && ( ntx / m_npx ) % i == 0 ) { mxx = m_npx * i; break; }
                for( int i = 2; i <= factor && i <= nty/m_npy; ++ i ) if( ( m_nProcs / ranks ) % i == 0 && ( nty / m_npy ) % i == 0 ) { mxy = m_npy * i; break; }
                for( int i = 2; i <= factor && i <= ntz/m_npz; ++ i ) if( ( m_nProcs / ranks ) % i == 0 && ( ntz / m_npz ) % i == 0 ) { mxz = m_npz * i; break; }
                // next cut is in dimension with largest partition-size
                int tmpx = ntx / mxx, tmpy = nty / mxy, tmpz = ntz / mxz;
                if( nty % mxy == 0 && tmpy >= tmpx && tmpy >= tmpz ) m_npy = mxy;
                else if( ntz % mxz == 0 && tmpz >= tmpx && tmpz >= tmpy ) m_npz = mxz;
                else if( ntx % mxx == 0 ) m_npx = mxx;
                else { // no even partitioning what so ever
                    tmpx = ntx / m_npx; tmpy = nty / m_npy; tmpz = ntz / m_npz;
                    if( tmpy >= tmpx && tmpy >= tmpz ) m_npy *= 2;
                    else if( tmpz >= tmpx && tmpz >= tmpy ) m_npz *= 2;
                    else if( mxx != m_nProcs*m_nProcs ) m_npx *= 2;
                }
            };
            if( m_npx * m_npy * m_npz != m_nProcs || ntx % m_npx|| nty % m_npy || ntz % m_npz ) {
                std::cerr << "[CnC] Warning: no even partitioning found.\n";
            }
            m_psx = ntx / m_npx;
            m_psy = nty / m_npy;
            m_psz = ntz / m_npz;
            std::cerr << "distributing BLOCKED_3D\n";
            break;
        }

        if( m_dt == RED_BLACK ) {
            std::cerr << "distributing RED_BLACK\n";
        } else {
            std::cerr << m_npx << "x" << m_npy << "x" << m_npz << " partitions of " << m_psx << "x" << m_psy << "x" << m_psz << std::endl;
        }
    } else if( m_nProcs == 1 ) {
        m_npx = m_npx = m_npz = m_psx = m_psy = m_psz = 1;
    }
}

#endif // _TILE_H_INCLUDED_
