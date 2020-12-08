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

#include <utility>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <cassert>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "tbb/tick_count.h"
#include "atomic"

#ifdef _DIST_
#include <cnc/dist_cnc.h>
#include <cnc/internal/dist/distributor.h>
#else
#include <cnc/cnc.h>
#endif
#include "cnc/debug.h"

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

              
struct my_context : public CnC::context< my_context >
{
    my_tuner m_tuner;
    CnC::step_collection< compute_inverse, my_tuner > m_steps;
    CnC::item_collection< tile_tag, tile, my_tuner >  m_tiles;
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
        CnC::debug::trace( m_tiles );
        CnC::debug::trace( m_tags, "s");
        CnC::debug::trace( compute_inverse(), "ci");
#endif        
        CnC::debug::collect_scheduler_statistics(*this);
    }

    virtual void serialize( CnC::serializer & ser ) 
    {
        ser & m_tuner;
    }
};

class tile_array
{
    int m_dim;
    int m_size;
    tile *m_tiles;
    
  public:

    int dim() const { return m_dim; }
    int size() const { return m_size; }

    tile_array( int size = 0 ) :
        m_dim((size + TILESIZE - 1)/TILESIZE), // Size/TILESIZE rounded up
        m_size(size), 
        m_tiles( NULL )
    {
        if( m_dim ) m_tiles = new tile[m_dim*m_dim];
    }
    
    ~tile_array()
    {
        delete[] m_tiles;
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_dim & m_size;
        ser & CnC::chunk< tile >( m_tiles, m_dim*m_dim );
    }
#endif

    tile_array(const tile_array& t)
    {
        m_size = t.m_size;
        m_dim = t.m_dim;
        m_tiles = new tile[m_dim*m_dim];
        memcpy(m_tiles, t.m_tiles, m_dim*m_dim*sizeof(tile));
    }
    
    tile_array& operator=(const tile_array& t)
    {
        if (this != &t) 
        {
            delete[] m_tiles;
            m_size = t.m_size;
            m_dim = t.m_dim;
            m_tiles = new tile[m_dim*m_dim];
            memcpy(m_tiles, t.m_tiles, m_dim*m_dim*sizeof(tile));

        }
        return *this;
    }
    
    void dump( double epsilon = 1e-12 ) const {
        for (int i = 0; i < m_dim; i++ ) 
        {
            for (int j = 0; j < m_dim; j++ ) 
            {
                std::cout << "(" << i << "," << j << ")" << std::endl;
                m_tiles[m_dim*i+j].dump(epsilon);
            }
            std::cout << std::endl;
        }
    }

    int generate_matrix( int dimension )
    {
        printf("Floating point elements per matrix: %i x %i\n", dimension, dimension);
        printf("Floating point elements per tile: %i x %i\n", TILESIZE, TILESIZE);

        delete[] m_tiles;
        m_size = dimension;
        m_dim = (m_size + TILESIZE - 1)/TILESIZE; // Size/TILESIZE rounded up
        m_tiles = new tile[m_dim*m_dim];

        printf("tiles per matrix: %i x %i\n", m_dim, m_dim);
        int dim = m_dim;
        int size = m_size;

        std::cout << "dim(" << dim << ") size(" << size << ")" << std::endl;
        int ii = 0;
        double e = 0.0;
        for (int I = 0; I < dim; I++) 
        {
            for (int i = 0; i < TILESIZE; i++)
            {
                int jj = 0;
                for (int J = 0; J < dim; J++) 
                {
                    for (int j = 0; j < TILESIZE; j++)
                    {
                        if ((ii < size)&(jj < size)) e = double(rand())/RAND_MAX;
                        else if (ii == jj) e = 1; // On-diagonal padding
                        else e = 0; // Off-diagonal padding
                        m_tiles[dim*I + J].set(i,j,e);
                        jj++;
                    }
                }
                ii++;
            }
        }
        return m_dim;
    }

    
    int identity_check( double epsilon = MINPIVOT ) const
    {
        int ecount = 0;
        
        for (int i = 0; i < m_dim; i++ ) 
        {
            for (int j = 0; j < m_dim; j++ ) 
            {
                int tcount = 0;
                
                tile &t = m_tiles[m_dim*i+j];

                tcount = (i == j) ?  t.identity_check(epsilon) : t.zero_check(epsilon);
                
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
        
        for (int i = 0; i < m_dim; i++ ) 
        {
            for (int j = 0; j < m_dim; j++ ) 
            {
                tile &t = m_tiles[m_dim*i+j];
                if (!t.equal( b.m_tiles[m_dim*i+j])) return false;
            }
        }
        return true;
    }
    
    // c = this * b
    tile_array multiply(const tile_array &b) const
    {
        tile_array c(m_size);
        tile t;
        for (int i = 0; i < m_dim; i++) 
        {
            for (int j = 0; j < m_dim; j++)
            {
                t.zero();
                for (int k = 0; k < m_dim; k++) 
                {
                    t.multiply_add_in_place(m_tiles[m_dim*i+k], b.m_tiles[m_dim*k+j]);
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

        for (int n = 0; n < dim; n++)
        {
            tile pivot_inverse = b.m_tiles[dim*n+n].inverse();
            b.m_tiles[dim*n+n] = pivot_inverse;
    
            for (int j = 0; j < dim; j++) 
            {
                if (j == n) continue;
            
                tile& tnj = b.m_tiles[dim*n+j];
                b.m_tiles[dim*n+j] = pivot_inverse.multiply(tnj);
            }
        
            for (int i = 0; i < dim; i++)
            {
                if (i == n) continue;

                tile tin = b.m_tiles[dim*i+n];
                b.m_tiles[dim*i+n] = tin.multiply_negate(pivot_inverse);
                
                for (int j = 0; j < dim; j++) 
                {
                    if (j == n) continue;
                    tile &tnj = b.m_tiles[dim*n+j];
                    b.m_tiles[dim*i+j].multiply_subtract_in_place(tin,tnj);
                }
            }
        }
        return b;
    }


    tile_array inverse_cnc( my_context & c )
    {
        for (int i = 0; i < m_dim; i++)
        {
            for (int j = 0; j < m_dim; j++) 
            {
                tile_tag t( m_dim, 0, i, j);
                c.m_tiles.put( t, m_tiles[m_dim*i+j] );
            }
        }
        
        for (int i = 0; i < m_dim; i++)
        {
            for (int j = 0; j < m_dim; j++) 
            {
                c.m_tags.put( tile_tag( m_dim, 0, i, j) );
            }
        }

        c.wait();

        tile_array b(m_size);
        c.m_tiles.size();
        for (int i = 0; i < m_dim; i++)
        {
            for (int j = 0; j < m_dim; j++) 
            {
                c.m_tiles.get( tile_tag( m_dim, m_dim, i, j), b.m_tiles[m_dim*i+j] );
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
        tile tnn;
        c.m_tiles.get( tag, tnn );
        //        tile out_nij = tnn.inverse();
        c.m_tiles.put( out_tag, tnn.inverse() );//out_nij );
    }
    else if ( i == n ) 
    {
        tile tnj;
        c.m_tiles.get( tag, tnj );
        tile tn1nn;
        c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, n ), tn1nn );
        //        tile out_nij = tn1nn.multiply(tnj);
        c.m_tiles.put( out_tag, tn1nn.multiply(tnj) );
    }
    else if ( j == n ) 
    {
        tile tin;
        c.m_tiles.get( tag, tin );
        tile tn1nn;
        c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, n ), tn1nn );
        //        tile out_nij = tin.multiply_negate(tn1nn);
        c.m_tiles.put( out_tag, tin.multiply_negate(tn1nn) );//out_nij );
    }
    else 
    {
        tile tij;
        c.m_tiles.get( tag, tij );
        tile tnin;
        c.m_tiles.get( tile_tag( tag.m_dim, n, i, n ), tnin );
        tile tn1nj;
        c.m_tiles.get( tile_tag( tag.m_dim, n+1, n, j ), tn1nj );
        //        tile out_nij = tij.multiply_subtract( tnin, tn1nj );
        c.m_tiles.put( out_tag, tij.multiply_subtract( tnin, tn1nj) );//out_nij );
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

#define PROC_FOR_TILE( _x, _y )  ( ( ( ( (_y) * m_tnx + 31 ) + (_x) ) ) % m_nP )
#define COMPUTE_ON( _i, _j )  ( PROC_FOR_TILE( (_i) / m_tsx, (_j) / m_tsy ) )

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


void report_time( const char * mode, tile_array& a, double time ) 
{
    std::cout <<  mode << " Total Time: " << time << " sec" << std::endl;
	float Gflops = ((float)2*a.size()*a.size()*a.size())/((float)1000000000);
	if (Gflops >= .000001) printf("Floating-point operations executed: %f billion\n", Gflops);
	if (time >= .001) printf("Floating-point operations executed per unit time: %6.2f billions/sec\n", Gflops/time);
}
    


int main(int argc, char *argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< my_context > dc_init;
#endif
    tile_array in_array;
    int tdim;

    if (argc == 2 && 0 != atoi(argv[1])) 
    {
        std::cout << "Generating matrix of size " << argv[1] << std::endl;
        tdim = in_array.generate_matrix(atoi(argv[1]));
    }
    else 
    {
        std::cout << "Usage: matrix_inverse dim" << std::endl;
		return -1;
	}

    report_memory();

    // invert serially
     std::cout << "Invert serially" << std::endl;
    tbb::tick_count t0 = tbb::tick_count::now();
    tile_array out_array = in_array.inverse();
    tbb::tick_count t1 = tbb::tick_count::now();
    report_time( "Serial", out_array,(t1-t0).seconds() );
    report_memory();

    tile_array test = in_array.multiply(out_array);
    test.identity_check(1e-6);
    report_memory();

    std::cout << "Invert CnC steps" << std::endl;
    struct my_context c( tdim );
    //debug::set_num_threads(1);
    tbb::tick_count t2 = tbb::tick_count::now();
    tile_array out_array2 = in_array.inverse_cnc(c);
    tbb::tick_count t3 = tbb::tick_count::now();
    report_time( "CnC", out_array2, (t3-t2).seconds() );
    report_memory();
    
    tile_array test2 = in_array.multiply(out_array2);
    test2.identity_check(1e-6);
    report_memory();
}
