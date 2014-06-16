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

/* this version differs from the plain version
   - it uses smart-pointersm, which avoids data-copies and allows distributed
   allocation of the tile-array;
   performance seems to suffer a bit from this, but memory footprint is cut to half
   - in dist-mode, the array/matrix is generated distributedly, e.g. each
   process generates what it owns; input to the CnC graph is also done distributedly
   Mimicking SPMD-style interaction with MPI codes.
   Search for "FOR_ME" and "_DIST_" to look at these changes.
*/

#ifdef _DIST_
#include <mpi.h>
#include <cnc/dist_cnc.h>
#include <cnc/internal/dist/distributor.h>
#else
#include <cnc/cnc.h>
#endif
#include "cnc/debug.h"

#include <utility>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <cassert>
#include <memory>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "tbb/tick_count.h"
#include "tbb/atomic.h"

#include "../tile.h"

using namespace CnC;


class tile_array;

struct tile_tag {
    int m_i0, m_i1, m_i2, m_dim;
    
    tile_tag( int dim, int i0, int i1, int i2 ) :
        m_i0(i0), m_i1(i1), m_i2(i2), m_dim(dim) {};

    bool operator==( const tile_tag & t ) const {
        return m_dim == t.m_dim &&
            m_i0 == t.m_i0 && m_i1 == t.m_i1 && m_i2 == t.m_i2;
    }
#ifdef _DIST_
    tile_tag()
        : m_i0( -1 ), m_i1( -1 ), m_i2( -1 ), m_dim( 0 )
    {}
    void serialize( CnC::serializer & ser )
    {
        ser & m_i0 & m_i1 & m_i2 & m_dim;
    }
#endif
    
};


template <>
struct cnc_hash< tile_tag > 
{
    size_t operator()(const tile_tag& tt) const
    {
        unsigned int h = (int)tt.m_dim;
        
        unsigned int high = h & 0xf8000000;
        h = h << 5;
        h = h ^ (high >> 27);
        h = h ^ tt.m_i0;
        
        unsigned int high1 = h & 0xf8000000;
        h = h << 5;
        h = h ^ (high1 >> 27);
        h = h ^ tt.m_i1;

        unsigned int high2 = h & 0xf8000000;
        h = h << 5;
        h = h ^ (high2 >> 27);
        h = h ^ tt.m_i2;

        return size_t(h);
    }
};


namespace std {
    std::ostream & cnc_format( std::ostream& os, const tile_tag &tt )
    {
        return os << "(" << tt.m_dim << ":"
                  << tt.m_i0 << "," << tt.m_i1 << "," << tt.m_i2 << ")";
    }
}

struct my_context;



struct compute_inverse
{
    int execute( const tile_tag & t, my_context & c ) const;
};

//#define NO_CONSUMED_ON 1

struct my_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    typedef tile_tag tag_type;

    int PROC_FOR_TILE( int _x, int _y ) const { return ( ( ( ( (_y) * m_tnx + 31 ) + (_x) ) ) % m_nP ); }
    int COMPUTE_ON( int _i, int _j )    const { return ( PROC_FOR_TILE( (_i) / m_tsx, (_j) / m_tsy ) ); }

    template< class dependency_consumer >
    void depends( const tile_tag & tag, my_context & c, dependency_consumer & dC ) const;
    my_tuner( int ntiles = 0 );
    int compute_on( const tile_tag & tag, my_context & ) const;

    int get_count( const tag_type & tag ) const;
#ifdef NO_CONSUMED_ON
    int consumed_on( const tag_type & tag ) const { return CnC::CONSUMER_UNKNOWN; }
#else
    const std::vector< int > & consumed_on( const tile_tag & tag ) const;
#endif
    int produced_on( const tile_tag & tag ) const;
    int m_nP;
    int m_tnx, m_tny, m_tsx, m_tsy;
    std::vector< std::vector< int > > m_rows, m_cols, m_rowcols, m_procs;
    std::vector< int > m_all;
    void serialize( CnC::serializer & ser ) 
    {
        ser & m_nP & m_tnx & m_tny & m_tsx & m_tsy
            & m_rows & m_cols & m_rowcols & m_procs & m_all;
        assert( m_nP > 0 && m_nP == (int)m_procs.size() );
    }
};


typedef std::shared_ptr< const tile > const_tile_type;
typedef std::shared_ptr< tile > tile_type;

              
struct my_context : public CnC::context< my_context >
{
    my_tuner m_tuner;
    CnC::step_collection< compute_inverse, my_tuner > m_steps;
    CnC::item_collection< tile_tag, const_tile_type, my_tuner >  m_tiles;
    CnC::tag_collection< tile_tag > m_tags;

    my_context( int nt = 0 )
        : CnC::context< my_context >(),
          m_tuner( nt ),
          m_steps( *this, "mi", compute_inverse(), m_tuner ),
          m_tiles( *this, m_tuner ),
          m_tags( *this )
    {
        m_tags.prescribes( m_steps, *this );
#if 0
        CnC::debug::trace( m_tiles, 3 );
        //        CnC::debug::trace( m_tags, 3 );
        //        CnC::debug::trace( m_steps, 3 );
#endif        
        CnC::debug::collect_scheduler_statistics(*this);
    }

    virtual void serialize( CnC::serializer & ser ) 
    {
        ser & m_tuner;
    }
};


#ifdef _DIST_
// returns if the given tile (x,y) is owned by my (r)
#  define FOR_ME( _c, _r, _x, _y ) ((_r) == _c.m_tuner.COMPUTE_ON( _x, _y ))
// returns if the given tile (x,y) is owned by my (r)m always true if r==0
#  define FOR_ME_OR_0( _c, _r, _x, _y ) (((_r)==0) || FOR_ME( _c, _r, _x, _y ))
#else
#  define FOR_ME( _c, _r, _x, _y ) true
#  define FOR_ME_OR_0( _c, _r, _x, _y ) true
#endif


class tile_array
{
    int m_dim;
    int m_size;
    const_tile_type * m_tiles;
    
public:

    int dim() const { return m_dim; }
    int size() const { return m_size; }

    tile_array( int size = 0 ) :
        m_dim((size + TILESIZE - 1)/TILESIZE), // Size/TILESIZE rounded up
        m_size(size), 
        m_tiles( NULL )
    {
        if( m_dim ) m_tiles = new const_tile_type[m_dim*m_dim];
    }
    
    ~tile_array()
    {
        delete[] m_tiles;
    }

    tile_array(const tile_array& t)
    {
        m_size = t.m_size;
        m_dim = t.m_dim;
        int sz = m_dim*m_dim;
        m_tiles = new const_tile_type[sz];
        for( int i = 0; i < sz; ++i ) {
            m_tiles[i] = t.m_tiles[i];
        }
    }
    
    tile_array& operator=(const tile_array& t)
    {
        if (this != &t) {
            int sz = t.m_dim*t.m_dim;
            if( m_dim != t.m_dim ) {
                delete[] m_tiles;
                m_tiles = new const_tile_type[sz];
                m_size = t.m_size;
                m_dim = t.m_dim;
            }
            for( int i = 0; i < sz; ++i ) {
                m_tiles[i] = t.m_tiles[i];
            }
        }
        return *this;
    }
    
    void dump( double epsilon = 1e-12 ) const {
        for (int i = 0; i < m_dim; i++ ) {
            for (int j = 0; j < m_dim; j++ ) {
                std::cout << "(" << i << "," << j << ")" << std::endl;
                m_tiles[m_dim*i+j]->dump(epsilon);
            }
            std::cout << std::endl;
        }
    }

    int generate_matrix( int dimension, my_context & c  )
    {
#ifdef _DIST_
        // need rank determine owned fraction of the matrix (FOR_ME)
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);
#endif
        
        printf("Floating point elements per matrix: %i x %i\n", dimension, dimension);
        printf("Floating point elements per tile: %i x %i\n", TILESIZE, TILESIZE);

        if( m_size != dimension ) {
            delete[] m_tiles;
            m_size = dimension;
            m_dim = (m_size + TILESIZE - 1)/TILESIZE; // Size/TILESIZE rounded up
            m_tiles = new const_tile_type[m_dim*m_dim];
        }

        printf("tiles per matrix: %i x %i\n", m_dim, m_dim);
        int dim = m_dim;
        int size = m_size;

        std::cout << "dim(" << dim << ") size(" << size << ")" << std::endl;
        double e = 0.0;
        for (int I = 0; I < dim; I++) {
            for (int J = 0; J < dim; J++) {
                if( FOR_ME_OR_0( c, rank, I, J ) ) { // for the identity check we need the entire matrix on 0
                    srand( I*m_dim+J );
                    int ii = I * TILESIZE;;
                    tile_type _tile = std::make_shared< tile >();
                    for (int i = 0; i < TILESIZE; i++) {
                        int jj = J * TILESIZE;
                        for (int j = 0; j < TILESIZE; j++) {
                            if ((ii < size)&(jj < size)) e = double(rand())/RAND_MAX;
                            else if (ii == jj) e = 1; // On-diagonal padding
                            else e = 0; // Off-diagonal padding
                            //                        std::cout << "m[" << ii << "," << jj << "(" << I << "," << J << "," << i << "," << j << ")]=" << e << std::endl;
                            _tile->set(i,j,e);
                            jj++;
                        }
                        ii++;
                    }
                    //                    std::cerr << rank << " generated tile " << I << "," << J << std::endl;
                    m_tiles[dim*I + J] = _tile;
                }
            }
        }
        return m_dim;
    }
    
    
    int identity_check( double epsilon = MINPIVOT ) const
    {
        int ecount = 0;
        
        for (int i = 0; i < m_dim; i++ ) {
            for (int j = 0; j < m_dim; j++ ) {
                int tcount = 0;
                const_tile_type t = m_tiles[m_dim*i+j];

                tcount = (i == j) ?  t->identity_check(epsilon) : t->zero_check(epsilon);                
                if (tcount == 0 ) continue;
                
                std::cout << "problem in tile(" << i << "," << j << ")" << std::endl;
                ecount += tcount;
            }
        }
        return ecount;
    }
    
    bool equal( const tile_array &b ) const
    {
        if (b.m_dim != m_dim) return false;
        
        for (int i = 0; i < m_dim; i++ ) {
            for (int j = 0; j < m_dim; j++ ) {
                const_tile_type t = m_tiles[m_dim*i+j];
                if (!t->equal( *b.m_tiles[m_dim*i+j] )) return false;
            }
        }
        return true;
    }
    
    // c = this * b
    tile_array multiply(const tile_array &b) const
    {
        tile_array c(m_size);
        for (int i = 0; i < m_dim; i++) {
            for (int j = 0; j < m_dim; j++) {
                tile_type t = std::make_shared< tile >();
                t->zero();
                for (int k = 0; k < m_dim; k++) {
                    t->multiply_add_in_place(*m_tiles[m_dim*i+k], *b.m_tiles[m_dim*k+j]);
                }
                c.m_tiles[m_dim*i+j] = t;
            }
        }
        return c;
    }


    tile_array inverse()
    {
        tile_array b = *this;
        int dim = m_dim;

        for (int n = 0; n < dim; n++) {
            const_tile_type pivot_inverse = std::make_shared< const tile >( C_INVERSE, *b.m_tiles[dim*n+n] );
            b.m_tiles[dim*n+n] = pivot_inverse;
    
            for (int j = 0; j < dim; j++) {
                if (j == n) continue;
            
                const tile& tnj = *b.m_tiles[dim*n+j];
                b.m_tiles[dim*n+j] = std::make_shared< const tile >( C_MULTIPLY, *pivot_inverse, tnj );
            }
        
            for (int i = 0; i < dim; i++) {
                if (i == n) continue;

                const_tile_type tin = b.m_tiles[dim*i+n];
                b.m_tiles[dim*i+n] = std::make_shared< const tile >( C_MULTIPLY_NEGATE, *tin, *pivot_inverse );
                
                for (int j = 0; j < dim; j++) {
                    if (j == n) continue;
                    const_tile_type tnj = b.m_tiles[dim*n+j];
                    tile_type tmp = std::make_shared< tile >( *b.m_tiles[dim*i+j] );
                    tmp->multiply_subtract_in_place(*tin, *tnj);
                    b.m_tiles[dim*i+j] = tmp;
                }
            }
        }
        return b;
    }


    tile_array inverse_cnc( my_context & c )
    {
        // all participating processes execute this
        int rank = 0;
#ifdef _DIST_
        // need rank determine owned fraction of the matrix (FOR_ME)
        MPI_Comm_rank(MPI_COMM_WORLD,&rank);
#endif
        // copy what process owns from local array to item-collection
        for (int i = 0; i < m_dim; i++) {
            for (int j = 0; j < m_dim; j++) if( FOR_ME( c, rank, i, j ) ) {
                    tile_tag t( m_dim, 0, i, j);
                    c.m_tiles.put( t, m_tiles[m_dim*i+j] );
                    assert( m_tiles[m_dim*i+j].get() );
                }
        }
        // put control tags (only what this process is reponsible for)
        for (int i = 0; i < m_dim; i++) {
            for (int j = 0; j < m_dim; j++) if( FOR_ME( c, rank, i, j ) ) {
                    c.m_tags.put( tile_tag( m_dim, 0, i, j) );
                }
        }

        // "wait" is an implicit barrier,
        // no process returns until entire dsitributed graph evaluation reached quiesence
        c.wait();

        // now write back data from item-collection to local arry
        tile_array b(m_size);
        //        c.m_tiles.size(); // not dist-env-ready
        for (int i = 0; i < m_dim; i++) {
            for (int j = 0; j < m_dim; j++) if( FOR_ME_OR_0( c, rank, i, j ) ) {// we need all output on rank 0 for verification
                    const_tile_type _tmp;
                    c.m_tiles.get( tile_tag( m_dim, m_dim, i, j), _tmp );
                    b.m_tiles[m_dim*i+j] = _tmp;
                }
        }
        return b;
    }
};
    

int compute_inverse::execute( const tile_tag & tag, my_context & c ) const
{
    int n = tag.m_i0;
    int i = tag.m_i1;
    int j = tag.m_i2;

    tile_tag out_tag( tag.m_dim, n+1, i, j );

    if (i == n && j == n ) 
        {
            const_tile_type tnn;
            c.m_tiles.get( tag, tnn );
            //        tile out_nij = tnn.inverse();
            c.m_tiles.put( out_tag, std::make_shared< const tile >( C_INVERSE, *tnn ) );//out_nij );
        }
    else if ( i == n ) 
        {
            const_tile_type tnj;
            c.m_tiles.get( tag, tnj );
            assert( tnj.get() );
            const_tile_type tn1nn;
            c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, n ), tn1nn );
            //        tile out_nij = tn1nn.multiply(tnj);
            c.m_tiles.put( out_tag, std::make_shared< const tile >( C_MULTIPLY, *tn1nn, *tnj ) );
        }
    else if ( j == n ) 
        {
            const_tile_type tin;
            c.m_tiles.get( tag, tin );
            const_tile_type tn1nn;
            c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, n ), tn1nn );
            //        tile out_nij = tin.multiply_negate(tn1nn);
            c.m_tiles.put( out_tag, std::make_shared< const tile >( C_MULTIPLY_NEGATE, *tin, *tn1nn ) );//out_nij );
        }
    else 
        {
            const_tile_type tij;
            c.m_tiles.get( tag, tij );
            const_tile_type tnin;
            c.m_tiles.get( tile_tag( tag.m_dim, n, i, n ), tnin );
            const_tile_type tn1nj;
            c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, j ), tn1nj );
            tile_type tmp = std::make_shared< tile >( *tij );
            tmp->multiply_subtract_in_place( *tnin, *tn1nj );
            assert( tmp.get() );
            c.m_tiles.put( out_tag, tmp );
            //        tile out_nij = tij.multiply_subtract( tnin, tn1nj );
            //        b.m_tiles[dim*i+j] = tmp;
            //        c.m_tiles.put( out_tag, std::make_shared< const tile >( tij->multiply_subtract( *tnin, *tn1nj) ) );//out_nij );
        }
    
    if ( (n+1) < tag.m_dim ) 
        {
            c.m_tags.put( out_tag );
        }
    return CnC::CNC_Success;
}

int my_tuner::get_count( const tag_type & tt ) const
{
    int dim = tt.m_dim;
    int n = tt.m_i0;
    if( dim == n ) return CnC::NO_GETCOUNT;
    int i = tt.m_i1;
    int j = tt.m_i2;
    
    int count = 1;

    if (i == (n-1) && j == (n-1)) count += (dim-1) + (dim-1);
    if (i == (n-1) && !(j == n-1)) count += dim-1;
    if (j == n  && !(i == n)) count += dim-1;

    return count;
}


my_tuner::my_tuner( int ntiles )
    : m_tnx( 0 ),
      m_tny( 0 ),
      m_tsx( 1 ),
      m_tsy( 1 ),
      m_rows(),
      m_cols(),
      m_rowcols(),
      m_procs(),
      m_all( 1, CnC::CONSUMER_ALL )
{
    m_nP = numProcs();
#ifdef _DIST_
    int _np = m_nP * 1;
    assert( _np == 1 || ( ntiles * ntiles ) % _np == 0 );
    if( _np > 1 && ntiles ) { 
        // compute grid dimension for partitioning
        m_tnx = 2, m_tny = _np / 2;
        while( m_tnx < m_tny && m_tny % 2 == 0 ) {
            m_tnx *= 2;
            m_tny = _np / m_tnx;
        }
        assert( m_tnx * m_tny == _np );
        m_tsx = ntiles / m_tnx;
        m_tsy = ntiles / m_tny;
        std::cerr << m_tnx << "x" << m_tny << " tiles of " << m_tsx << "x" << m_tsy << std::endl;
        assert( m_tnx * m_tsx == ntiles && m_tny * m_tsy == ntiles );
#ifndef NO_CONSUMED_ON
        // setup vectors to store info for consumed_on
        m_rows.resize( m_tny );
        m_cols.resize( m_tnx );
        m_rowcols.resize( m_tnx * m_tny );
        for( int i = 0; i < m_tnx; ++i ) {
            m_cols[i].resize( m_tny );
            for( int j = 0; j < m_tny; ++j ) {
                m_rowcols[j*m_tnx+i].resize( m_tnx+m_tny );
            }
        }
        for( int j = 0; j < m_tny; ++j ) {
            m_rows[j].resize( m_tnx );
            for( int i = 0; i < m_tnx; ++i ) {
                m_rows[j][i] = m_cols[i][j] = PROC_FOR_TILE( i, j );
                for( int x = 0; x < m_tnx; ++x ) m_rowcols[j*m_tnx+i][x] = PROC_FOR_TILE( x, j );
                for( int y = 0; y < m_tny; ++y ) m_rowcols[j*m_tnx+i][m_tnx+y] = PROC_FOR_TILE( i, y );
                m_rowcols[j*m_tnx+i][i] = m_rowcols[j*m_tnx+i].back();
                m_rowcols[j*m_tnx+i].pop_back();
            }
        }
        m_procs.resize( m_nP );
        for( int i = 0; i < m_nP; ++i ) m_procs[i].resize( 1, i );
        assert( m_nP > 0 && (int)m_procs.size() == m_nP );
#endif
    }
#endif
}

int my_tuner::compute_on( const tile_tag & tag, my_context & ) const
{
    return COMPUTE_ON( tag.m_i1, tag.m_i2 );
}

int my_tuner::produced_on( const tile_tag & tag ) const
{
    return ( tag.m_i0 > 0 ? COMPUTE_ON( tag.m_i1, tag.m_i2 ) : 0 );
}

template< class dependency_consumer >
void my_tuner::depends( const tile_tag & tag, my_context & c, dependency_consumer & dC ) const
{
    int n = tag.m_i0;
    int i = tag.m_i1;
    int j = tag.m_i2;

    dC.depends( c.m_tiles, tag ); //, PROD( tag.m_dim, n-1, i, j ) );
    
    if (i == n && j == n ) {
    } else if ( i == n ) {
        dC.depends( c.m_tiles, tile_tag( tag.m_dim, n+1, n, n ) );
    } else if ( j == n ) {
        dC.depends( c.m_tiles, tile_tag( tag.m_dim, n+1, n, n ) );
    } else {
        dC.depends( c.m_tiles, tile_tag( tag.m_dim, n, i, n ) );
        dC.depends( c.m_tiles, tile_tag( tag.m_dim, n+1, n, j ) );
    }
}

#ifndef NO_CONSUMED_ON
const std::vector< int > & my_tuner::consumed_on( const tile_tag & tag ) const
{
    assert( 0 < m_procs.size() );
    int n = tag.m_i0;
    int i = tag.m_i1;
    int j = tag.m_i2;
    
    if( n - 1 == i && ( i == j || n == j ) ) {
        return m_rowcols[(j/m_tsy)*m_tnx+(i/m_tsx)];
    } else if( n - 1 == i ) {
        return m_rows[j/m_tsy];
    } else if( n == j ) {
        return m_cols[i/m_tsx];
    }
    return m_procs[COMPUTE_ON( i, j )]; 
}
#endif

void report_memory()
{
    std:: cout << "tiles created " << tiles_created << " tiles deleted " << tiles_deleted << " tiles remaining " << tiles_created - tiles_deleted << std::endl;
    tiles_created = 0;
    tiles_deleted = 0;

    static int lastr = 0;

#ifdef WIN32
	HANDLE self;
	PROCESS_MEMORY_COUNTERS pmc;
	SIZE_T resident = 0;
	self = GetCurrentProcess();
	if (GetProcessMemoryInfo(self, &pmc, sizeof(pmc))) {
		resident = pmc.WorkingSetSize;
	}
	CloseHandle(self);
#else
	FILE *f = fopen("/proc/self/statm", "r");
    int total, resident, share, trs, drs, lrs, dt;
    if( fscanf(f,"%d %d %d %d %d %d %d", &total, &resident, &share, &trs, &drs, &lrs, &dt) != 7 ) std::cerr << "error reading /proc/self/statm\n";
#endif

	std:: cout << "resident memory MB " << double(resident*4096)/1000000 << "   increment MB " << double((resident-lastr)*4096)/1000000 << std::endl;
    lastr = resident;
}


void report_time( const char * mode, int msz, double time ) 
{
    std::cout <<  mode << " Total Time: " << time << " sec" << std::endl;
	float Gflops = ((float)2*msz*msz*msz)/((float)1000000000);
	if (Gflops >= .000001) printf("Floating-point operations executed: %f billion\n", Gflops);
	if (time >= .001) printf("Floating-point operations executed per unit time: %6.2f billions/sec\n", Gflops/time);
}
    


int main(int argc, char *argv[])
{
    if (!(argc == 2 && 0 != atoi(argv[1]))) {
        std::cout << "Usage: matrix_inverse dim" << std::endl;
		return -1;
	}

    int sz = atoi(argv[1]);
    int tdim = (sz + TILESIZE - 1)/TILESIZE;

#ifdef _DIST_
    // init MPI to mimick SPMD-style interactino with CnC through MPI
    int p;
    //!! FIXME passing NULL ptr breaks mvapich1 mpi implementation
    MPI_Init_thread( 0, NULL, MPI_THREAD_MULTIPLE, &p );
    if( p != MPI_THREAD_MULTIPLE ) std::cerr << "Warning: not MPI_THREAD_MULTIPLE (" << MPI_THREAD_MULTIPLE << "), but " << p << std::endl;
    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    { // need a scope for dist_init

        // init CnC here so that we have the context/tuner 
        // the tuner defines the distribution strategy, which we use when generating the matrix
        CnC::dist_cnc_init< my_context > dc_init( MPI_COMM_WORLD, true );

        { // need a scope for the context
#endif
            tile_array in_array;
            struct my_context c( tdim );
            std::cout << "Generating matrix of size " << argv[1] << std::endl;
            // all processes do this, we are SPMD!
            tdim = in_array.generate_matrix(sz, c);

            report_memory();

            // invert serially
            {
                std::cout << "Invert serially" << std::endl;
                tbb::tick_count t0 = tbb::tick_count::now();
#ifndef _DIST_
                tile_array out_array = in_array.inverse();
#endif
                tbb::tick_count t1 = tbb::tick_count::now();
                report_time( "Serial", sz, (t1-t0).seconds() );
                report_memory();
        
#ifndef _DIST_
                tile_array test = in_array.multiply(out_array);
                test.identity_check(1e-5);
#endif
            } // end of scope releases out_array and test
#ifdef _DIST_
            // we need a barrier between CnC context creation and putting stuff
            MPI_Barrier( MPI_COMM_WORLD );
#endif
            report_memory();

            {
                std::cout << "Invert CnC steps" << std::endl;
                //debug::set_num_threads(1);
                tbb::tick_count t2 = tbb::tick_count::now();
                tile_array out_array2 = in_array.inverse_cnc(c);
                tbb::tick_count t3 = tbb::tick_count::now();
                report_time( "CnC", out_array2.size(), (t3-t2).seconds() );
                report_memory();
        
#ifdef _DIST_
                // only rank 0 has the full input matrix for the verification
                if( rank == 0 )
#endif
                    {
                        tile_array test2 = in_array.multiply(out_array2);
                        test2.identity_check(1e-5);
                    } // end of scope releases test2
            } // end of scope releases out_array2
            report_memory();
#ifdef _DIST_
            MPI_Barrier( MPI_COMM_WORLD );
        }
    }

    MPI_Finalize();
#endif
    return 0;
}
