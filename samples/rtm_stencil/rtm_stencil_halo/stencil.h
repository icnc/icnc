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
#ifndef _STENCIL_H_INCLUDED_
#define _STENCIL_H_INCLUDED_

#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <cnc/itac.h>
#include "tile.h"
#include "blocked_range.h"
#include <memory>
#include <tbb/tick_count.h>

// *******************************************************************************************
// *******************************************************************************************
// Define tile-sizes and halo-size

#ifdef USE_BSIZE
#define USE_BSIZEX USE_BSIZE
#define USE_BSIZEY USE_BSIZE
#define USE_BSIZEZ USE_BSIZE
#endif
#ifndef USE_BSIZEX
# define USE_BSIZEX 80
#endif
#ifndef USE_BSIZEY
# define USE_BSIZEY 80
#endif
#ifndef USE_BSIZEZ
# define USE_BSIZEZ 80
#endif
#ifndef USE_HSIZE
# define USE_HSIZE 4
#endif

enum { BSIZEX = USE_BSIZEX,
       BSIZEY = USE_BSIZEY,
       BSIZEZ = USE_BSIZEZ,
       HSIZE  = USE_HSIZE,
       DSIZE  = 32 };

// *******************************************************************************************
// *******************************************************************************************
// types and constants

// distribution style
typedef enum {
    BLOCKED_3D = 0,
    BLOCKED_2D = 1,
    BLOCKED_CYCLIC_3D = 2,
    BLOCKED_CYCLIC_2D = 3,
    CYCLIC = 4,
    RED_BLACK = 5
} dist_type;

// *******************************************************************************************
// use a compile-time fixed size for tiles
typedef Tile3d< BSIZEX, HSIZE, BSIZEY, HSIZE, BSIZEZ, HSIZE > tile_type;

// core-tiles for rho, epsilon, delta, cos_table_1, sin_table_1, cos_table_2, sin_table_2
// don't have halos
typedef Tile3d< BSIZEX, 0, BSIZEY, 0, BSIZEZ, 0 > core_tile_type;

typedef blocked_range< 3 > tag_range_type;
typedef CnC::tag_tuner< tag_range_type, CnC::tag_partitioner<> > tag_tuner_type;

#include "rtm_tags.h"

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
class stencil_context;

// *******************************************************************************************
// *******************************************************************************************
// Step declarations
// stencil and initer are defined in "user"-code

// starting stencil steps
struct start_step
{
    template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
    int execute( const tag_range_type & tag, stencil_context< Parent_context, Stencil, Initer, Tuner > & c ) const
    {
#ifdef CNC_WITH_ITAC
        VT_FUNC( "start" );
#endif
        for( auto i = tag.begin(); i != tag.end(); ++ i ) {
            c.m_tags.put( i );
        }
        return 0;
    }
};


// *******************************************************************************************
// *******************************************************************************************
// our tuner declaration, the code is in a separate file
struct stencil_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    stencil_tuner( int ntx = 0, int nty = 0, int ntz = 0, int t = 0, dist_type dt = BLOCKED_3D );

    template< typename Context >
    int compute_on( const loop_tag & t, Context & ) const;
    template< typename Context >
    int compute_on( const tag_range_type & r, Context & ) const;

    template< typename Context, typename DC >
    void depends( const loop_tag & tag, Context & c, DC & dc ) const;
    template< typename Context, typename DC >
    void depends( const tag_range_type &, Context & , DC & ) const{}

    int consumed_on( const pos_tag & tag ) const;
    int consumed_on( const tile_tag & tag ) const;
    int consumed_on( const halo_tag & tag ) const;
#ifndef VERIFY
    int get_count( const pos_tag & tag ) const;
    int get_count( const tile_tag & tag ) const;
    int get_count( const halo_tag & tag ) const;
#endif
    //private:
    inline int proc_for_tile( int _x, int _y, int _z ) const;
    int m_nProcs;            // number of partitions/processes
    int m_npx, m_npy, m_npz; // partition-grid (#partitions per dimension)
    int m_psx, m_psy, m_psz; // # blocks per partition and dimension
    int m_ntx, m_nty, m_ntz; // # of blocks per dimension
    int m_t;                 // number of time steps
    dist_type m_dt;          // distribution type
};
CNC_BITWISE_SERIALIZABLE( stencil_tuner );

// *******************************************************************************************
// *******************************************************************************************
// finally, our context

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner = stencil_tuner >
class stencil_context : public CnC::context< Parent_context >
{
public:
    typedef Tuner tuner_type;
    // some additional stuff, aligns with DSA requirements
    int Nx,  // number of elements in x dimension
        Ny,  // number of elements in y dimension
        Nz,  // number of elements in z dimension
        T,   // number of time-steps
        BNX, // number of tiles in x direction
        BNY, // number of tiles in y direction
        BNZ, // number of tiles in z direction
        BBSX, // block-size on the x boundary
        BBSY, // block-size on the y boundary
        BBSZ; // block-size on the z boundary
    float coeff_x[HSIZE+1]; // needed for RTM

    // our tuner
    Tuner                                        m_tuner;
    // step collections
    CnC::step_collection< Stencil, Tuner >     m_steps;
    CnC::step_collection< Initer,  Tuner >     m_matrixInit;
    CnC::step_collection< start_step, Tuner >  m_starts;
    // tag-collections
    CnC::tag_collection< loop_tag >                       m_tags;
    CnC::tag_collection< loop_tag >                       m_initTags;
    CnC::tag_collection< tag_range_type, tag_tuner_type > m_startTags;
    // item-collections
    CnC::item_collection< tile_tag, std::shared_ptr< const tile_type >, Tuner >     m_tiles;
    // we need one halo collections per dimension, because the types differ (different shapes)
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::xhalo_type >, Tuner > m_xhalos;
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::yhalo_type >, Tuner > m_yhalos;
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::zhalo_type >, Tuner > m_zhalos;
#ifdef SECOND_ORDER_HALO
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::xyhalo_type >, Tuner > m_xyhalos;
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::yzhalo_type >, Tuner > m_yzhalos;
    CnC::item_collection< halo_tag, std::shared_ptr< const tile_type::xzhalo_type >, Tuner > m_xzhalos;
#endif

    // now the constructor, accepting problem configuration (which is constant during the program run) as parameters
    stencil_context( int nx = 0, int ny = 0, int nz = 0, int t = -1, dist_type dt = BLOCKED_3D )
        : Nx( nx ),
          Ny( ny ),
          Nz( nz ),
          T( t ),
          BNX( ( nx + BSIZEX - 1 ) / BSIZEX ),
          BNY( ( ny + BSIZEY - 1 ) / BSIZEY ),
          BNZ( ( nz + BSIZEZ - 1 ) / BSIZEZ ),
          BBSX( Nx % BSIZEX ),
          BBSY( Ny % BSIZEY ),
          BBSZ( Nz % BSIZEZ ),
          m_tuner( BNX, BNY, BNZ, t, dt ),
          m_steps( *this, m_tuner ),
          m_starts( *this, m_tuner ),
          m_matrixInit( *this, m_tuner ),
          m_tags( *this ),
          m_initTags( *this ),
          m_startTags( *this ),
          m_tiles( *this, "tiles", m_tuner ),
          m_xhalos( *this, "xh", m_tuner ),
          m_yhalos( *this, "yh", m_tuner ),
          m_zhalos( *this, "zh", m_tuner )
#ifdef SECOND_ORDER_HALO
        , m_xyhalos( *this, "xyh", m_tuner ),
          m_yzhalos( *this, "yzh", m_tuner ),
          m_xzhalos( *this, "xzh", m_tuner )
#endif
    {
        if( BBSX == 0 ) BBSX = BSIZEX;
        if( BBSY == 0 ) BBSY = BSIZEY;
        if( BBSZ == 0 ) BBSZ = BSIZEZ;

        // prescription
        m_tags.prescribes( m_steps, (Parent_context&)*this );
        m_initTags.prescribes( m_matrixInit, (Parent_context &)*this );
        m_startTags.prescribes( m_starts, (Parent_context &)*this );
        // producer/consumer
        m_steps.consumes( m_tiles );
        m_steps.produces( m_tiles );
        m_steps.consumes( m_xhalos );
        m_steps.produces( m_xhalos );
        m_steps.consumes( m_yhalos );
        m_steps.produces( m_yhalos );
        m_steps.consumes( m_zhalos );
        m_steps.produces( m_zhalos );
#ifdef SECOND_ORDER_HALO
        m_steps.consumes( m_xyhalos );
        m_steps.produces( m_xyhalos );
        m_steps.consumes( m_yzhalos );
        m_steps.produces( m_yzhalos );
        m_steps.consumes( m_xzhalos );
        m_steps.produces( m_xzhalos );
#endif

        // if( CnC::tuner_base::myPid() == 0 ) CnC::debug::trace( m_tiles );
        // if( CnC::tuner_base::myPid() == 0 ) CnC::debug::trace( m_xhalos );
        // if( CnC::tuner_base::myPid() == 0 ) CnC::debug::trace( m_yhalos );
        // if( CnC::tuner_base::myPid() == 0 ) CnC::debug::trace( m_zhalos );
        // if( CnC::tuner_base::myPid() == 0 )  CnC::debug::trace( m_steps );
    }

// marshalling
#ifdef _DIST_
    virtual void serialize( CnC::serializer & ser )
    {
        float * tmpx = coeff_x;
        ser & m_tuner
            & Nx & Ny & Nz
            & T
            & BNX & BNY & BNZ
            & BBSX & BBSY & BBSZ
            & CnC::chunk< float, CnC::no_alloc >( tmpx, HSIZE+1 );
    }
#endif
    // *******************************************************************************************
    // some debugging/outputting helper
    void run( );
    void print_summary( const char *header, double interval );
    void print_y( std::ostream & os );
    void dump( const char *fn );
};

#include "stencil_tuner.h"

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
void stencil_context< Parent_context, Stencil, Initer, Tuner >::run()
{
#ifdef CNC_WITH_ITAC
    VT_initialize( NULL, NULL );
#endif
    assert( BSIZEZ >= 4 && BSIZEX >= 4 && BSIZEY >= 4 );

    printf( "Order-%d 3D-Stencil (%d points) with space %dx%dx%d and time %d dist %d\n", 
            HSIZE, HSIZE*2*3+1
#ifdef SECOND_ORDER_HALO
            + HSIZE*2*6
#endif
            , this->Nx, this->Ny, this->Nz, this->T, this->m_tuner.m_dt);

    //    CnC::debug::collect_scheduler_statistics( ctxt );
    for( int z = 0; z < this->BNZ; ++z ) {
        for( int y = 0; y < this->BNY; ++y ) {
            for( int x = 0; x < this->BNX; ++x ) {
                this->m_initTags.put( loop_tag( -1, x, y, z ) );
            }
        }
    }
    this->wait();

    tbb::tick_count start = tbb::tick_count::now();

    if( this->T > 0 ) {
        // put all the control tags for first time-step
        tag_range_type tag_space;
        tag_space.set_dim( 0, 0, this->BNX );
        tag_space.set_dim( 1, 0, this->BNY );
        tag_space.set_dim( 2, 0, this->BNZ );
        tag_space.set_grain( 0, this->m_tuner.m_psx );
        tag_space.set_grain( 1, this->m_tuner.m_psy );
        tag_space.set_grain( 2, this->m_tuner.m_psz );
        this->m_startTags.put_range( tag_space );
        this->wait();
    }

    tbb::tick_count stop = tbb::tick_count::now();
    //    float *A = copy_out( ctxt );
    print_summary( "stencil_loop", (stop-start).seconds() );
}

// *******************************************************************************************
// *******************************************************************************************

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
void stencil_context< Parent_context, Stencil, Initer, Tuner >::print_summary( const char *header, double interval )
{
    /* print timing information */
    printf("++++++++++++++++++++ %s ++++++++++++++++++++++\n", header);
  
    double mul = this->Nx-8;
    mul *= this->Ny-8;
    mul *= this->Nz-8;
    mul *= this->T;
    double perf = mul / (interval * 1e6);
    printf("Benchmark time: %f\n", interval);
    printf("Perf: %f Mcells/sec (%f M-FAdd/s, %f M-FMul/s)\n", perf, perf * 26, perf * 7);    
}

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
void stencil_context< Parent_context, Stencil, Initer, Tuner >::print_y( std::ostream & os )
{
    int bz = (this->Nz/2)/BSIZEZ;
    int bx = (this->Nx/2)/BSIZEX;
    int z  = (this->Nz/2)%BSIZEZ;
    int x  = (this->Nx/2)%BSIZEX;
    
#ifndef OUTFILE
    os.precision(4);os.setf(std::ios::fixed,std::ios::floatfield);
#endif
    for( int by = 0; by < this->BNY; by++) {
        std::shared_ptr< const tile_type > tile;
        this->m_tiles.get( loop_tag( this->T-1, bx, by, bz ), tile );
        const int NY = by < this->BNY - 1 ? BSIZEY : this->BBSY+HSIZE;
        for( int y = by > 0 ? 0 : -HSIZE; y < NY; ++y ) {
#ifdef _DIST_
            if( y < 0 || ( by == this->BNY - 1 && y >= this->BBSY ) ) os << 1.0 << "\n";
            else
#endif
            os << (*tile)(x,y,z) << "\n";
        }
    }
    printf("Done writing output\n");
}

template< typename Parent_context, typename Stencil, typename Initer, typename Tuner >
void stencil_context< Parent_context, Stencil, Initer, Tuner >::dump( const char *fn )
{
    std::ofstream myfile(fn); //, ios::out | ios::binary);
    tile_tag ptag( this->T-1, -9681 );
    std::shared_ptr< const tile_type > tile;
    for( int z = 0-HSIZE; z < this->Nz+HSIZE; ++z ) {
        const int tz = z < this->Nz ? ( z > 0 ? z / BSIZEZ : 0 ) : this->BNZ-1;
        const int iz = z < this->Nz ? ( z > 0 ? z % BSIZEZ : z ) : z - tz * BSIZEZ;
        for( int y = 0-HSIZE; y < this->Ny+HSIZE; ++y ) {
            const int ty = y < this->Ny ? ( y > 0 ? y / BSIZEY : 0 ) : this->BNY-1;
            const int iy = y < this->Ny ? ( y > 0 ? y % BSIZEY : y ) : y - ty * BSIZEY;
            for( int x = 0-HSIZE; x < this->Nx+HSIZE; ++x ) {
                const int tx = x < this->Nx ? ( x > 0 ? x / BSIZEX : 0 ) : this->BNX-1;
                const int ix = x < this->Nx ? ( x > 0 ? x % BSIZEX : x ) : x - tx * BSIZEX;

                tile_tag tag( this->T-1, tx, ty, tz );
                if( ! ( tag == ptag ) ) {
                    this->m_tiles.get( tag, tile );
                    ptag = tag;
                }
                //std::cout << x << " " << y << " " << z << " " << (*tile)(ix,iy,iz) << std::endl;
                myfile.write( (const char *)( &(*tile)(ix,iy,iz) ), sizeof( float ) );
            }
        }
    }
    // for (int x = 0; x < this->BNX; ++x) {
    //     int sx = x == 0 ? - HSIZE : 0;
    //     int ex = x == this->BNX-1 ? this->BBSX + HSIZE : BSIZEX;
    //     for (int xx = sx; xx < ex; ++xx) {
    //         for (int y = 0; y < this->BNY; ++y) {
    //             int sy = y == 0 ? - HSIZE : 0;
    //             int ey = y == this->BNY-1 ? this->BBSY + HSIZE : BSIZEY;
    //             for (int yy = sy; yy < ey; ++yy) {
    //                 for (int z = 0; z < this->BNZ; ++z) {
    //                     int sz = z == 0 ? - HSIZE : 0;
    //                     int ez = z == this->BNZ-1 ? this->BBSZ + HSIZE : BSIZEZ;
    //                     std::shared_ptr< const tile_type > tile;
    //                     this->m_tiles.get( loop_tag( this->T-1, x, y, z ), tile );
    //                     for (int zz = sz; zz < ez; ++zz) {
    //                         //                            myfile << (*tile)(xx,yy,zz) << " ";
    //                         myfile.write( (const char *)( &(*tile)(xx,yy,zz) ), sizeof( float ) );
    //                     }
    //                     //                        myfile << std::endl;
    //                 }
    //             }
    //         }
    //     }
    // }
    myfile.close();
}

#endif // _STENCIL_H_INCLUDED_
