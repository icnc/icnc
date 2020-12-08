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
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "tbb/tick_count.h"
#include "atomic"
#include "cnc/cnc.h"
#include "cnc/debug.h"
#include "../tile.h"

using namespace CnC;


class tile_array;

struct tile_tag {
    tile_array *m_array;
    int m_i0, m_i1, m_i2;
    
    tile_tag( tile_array *array, int i0, int i1, int i2 ) :
        m_array(array), m_i0(i0), m_i1(i1), m_i2(i2) {};

    bool operator==( const tile_tag & t ) const {
        return m_array == t.m_array &&
            m_i0 == t.m_i0 && m_i1 == t.m_i1 && m_i2 == t.m_i2;
    }
};


template <>
struct cnc_hash< tile_tag >
{
    size_t operator()(const tile_tag& tt) const
    {
        unsigned long int h = reinterpret_cast<unsigned long int>(tt.m_array);
        
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
        return os << "(" << hex << tt.m_array << ":" <<
            tt.m_i0 << "," << tt.m_i1 << "," << tt.m_i2 << ")";
    }
}

struct my_context;

struct compute_inverse
{
    int execute( const tile_tag & t, my_context & c ) const;
};


struct my_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    typedef tile_tag tag_type;
    bool preschedule() const { return true; }
    int get_count( const tag_type & tag ) const;
};

              
struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< compute_inverse, my_tuner > m_steps;
    CnC::item_collection< tile_tag, tile, my_tuner >  m_tiles;
    CnC::tag_collection< tile_tag >                   m_tags;

    my_context()
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_tiles( *this ),
          m_tags( *this )
    {
        m_tags.prescribes( m_steps, *this );
#if 0
        CnC::debug::trace( m_tiles, "t");
        CnC::debug::trace( m_tags, "s");
        CnC::debug::trace( compute_inverse(), "ci");
#endif        
        CnC::debug::collect_scheduler_statistics(*this);
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
        m_size(size)
    {
        m_tiles = new tile[m_dim*m_dim];
    }
    
    ~tile_array()
    {
        delete[] m_tiles;
    }

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

    void generate_matrix( int dimension )
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
                tile_tag t(this,0,i,j);
                c.m_tiles.put( t,  m_tiles[m_dim*i+j] );
            }
        }

        
        for (int i = 0; i < m_dim; i++)
        {
            for (int j = 0; j < m_dim; j++) 
            {
                c.m_tags.put( tile_tag(this,0,i,j) );
            }
        }

        c.wait();

        tile_array b(m_size);
        for (int i = 0; i < m_dim; i++)
        {
            for (int j = 0; j < m_dim; j++) 
            {
                c.m_tiles.get( tile_tag(this,m_dim,i,j), b.m_tiles[m_dim*i+j] );
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

    tile_tag out_tag( tag.m_array, n+1, i, j );

    if (i == n && j == n ) 
    {
        tile tnn;
        c.m_tiles.unsafe_get( tag, tnn );
        c.flush_gets();
        tile out_nij = tnn.inverse();
        c.m_tiles.put( out_tag, out_nij );
    }
    else if ( i == n ) 
    {
        tile tnj, tn1nn;
        c.m_tiles.unsafe_get( tag, tnj );
        c.m_tiles.unsafe_get( tile_tag( tag.m_array, n+1, n, n ), tn1nn );
        c.flush_gets();
        tile out_nij = tn1nn.multiply(tnj);
        c.m_tiles.put( out_tag, out_nij );
    }
    else if ( j == n ) 
    {
        tile tin, tn1nn;
        c.m_tiles.unsafe_get( tag, tin );
        c.m_tiles.unsafe_get( tile_tag( tag.m_array, n+1, n, n ), tn1nn );
        c.flush_gets();
        tile out_nij = tin.multiply_negate(tn1nn);
        c.m_tiles.put( out_tag, out_nij );
    }
    else 
    {
        tile tij, tnin, tn1nj;
        c.m_tiles.unsafe_get( tag, tij );
        c.m_tiles.unsafe_get( tile_tag( tag.m_array, n, i, n ), tnin );
        c.m_tiles.unsafe_get( tile_tag( tag.m_array, n+1, n, j ), tn1nj );
        c.flush_gets();
        tile out_nij = tij.multiply_subtract(tnin, tn1nj);
        c.m_tiles.put( out_tag, out_nij );
    }
    

    if ( (n+1) < tag.m_array->dim() ) 
    {
        c.m_tags.put( out_tag );
    }
    return CnC::CNC_Success;
}


int my_tuner::get_count( const tag_type & tt ) const
{
    int dim = tt.m_array->dim();
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
    tile_array in_array;

    if (argc == 2 && 0 != atoi(argv[1])) 
    {
        std::cout << "Generating matrix of size " << argv[1] << std::endl;
        in_array.generate_matrix(atoi(argv[1]));
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
    struct my_context c;
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
