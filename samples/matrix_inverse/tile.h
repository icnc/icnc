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


//
// A tile is a square matrix, used to tile a larger array
//
#ifndef TILESIZE
# define TILESIZE 90
#endif
#define MINPIVOT 1E-12

static tbb::atomic<int> tiles_created;
static tbb::atomic<int> tiles_deleted;

enum construct_type {
    C_INVERSE,
    C_MULTIPLY,
    C_MULTIPLY_NEGATE
};

class tile 
{
    double m_tile[TILESIZE*TILESIZE];
    
  public:
    

    tile()
    {
        tiles_created++;
    }

    tile( construct_type ct, const tile & op )
    {
        switch( ct ) {
        case C_INVERSE:
            op.inverse_( *this );
            break;
        default:
            std::cerr << "Wrong argument to construtor\n";
            exit(1);
            break;
        }
        tiles_created++;
    }

    tile( construct_type ct, const tile obj, const tile & arg )
    {
        switch( ct ) {
        case C_MULTIPLY:
            obj.multiply_( arg, *this );
            break;
        case C_MULTIPLY_NEGATE:
            obj.multiply_negate_( arg, *this );
            break;
        default:
            std::cerr << "Wrong argument to construtor\n";
            exit(1);
            break;
        }
        tiles_created++;
    }

    ~tile()
    {
        tiles_deleted++;
    }
    
    tile(const tile& t)
    {
        tiles_created++;
        memcpy(m_tile, t.m_tile, sizeof(m_tile));
    }
    
    tile& operator=(const tile& t)
    {
        if (this != &t)
        {
            memcpy(m_tile, t.m_tile, sizeof(m_tile));
        }
        return *this;
    }
    
    inline void set(const int i, const int j, double d) {  m_tile[j*TILESIZE+i] = d; }
    inline double get(const int i, const int j) const { return m_tile[j*TILESIZE+i]; }

    void dump( double epsilon = MINPIVOT ) const {
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                std::cout.width(10);
                double t = get(i,j);
                if (fabs(t) < MINPIVOT) t = 0.0;
                std::cout << t << " ";
            }
            std::cout << std::endl;
        }
    }

    int identity_check( double epsilon = MINPIVOT ) const {
        int ecount = 0;
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                double t = get(i,j);
                if ( i == j && ( fabs(t-1.0) < epsilon ) ) continue;
                if ( fabs(t) < epsilon) continue;
                
                std::cout << "(" << i << "," << j << "):" << t << std::endl;
                ecount++;
            }
        }
        return ecount;
    }

    int zero_check( double epsilon = MINPIVOT ) const
    {
        int ecount = 0;
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                double t = get(i,j);
                if ( fabs(t) < epsilon) continue;
                std::cout << "(" << i << "," << j << "):" << t << std::endl;
                ecount++;
            }
        }
        return ecount;
    }
    
    int equal( const tile& t ) const
    {
        for (int i = 0; i < TILESIZE; i++ ) 
        {
            for (int j = 0; j < TILESIZE; j++ ) 
            {
                if (get(i,j) != t.get(i,j)) return false;
            }
        }
        return true;
    }

    //b = inverse(*this)
    void inverse_( tile& b ) const
    {
        b = *this;

        for (int n = 0; n < TILESIZE; n++)
        {
            double pivot = b.get(n,n);
            if (fabs(pivot) < MINPIVOT)
            {
                std::cout <<"Pivot too small! Pivot( " << pivot << ")" << std::endl;
                b.dump();
                exit(0);
            }
            
            double pivot_inverse = 1/pivot;
            double row[TILESIZE];
            
            row[n]= pivot_inverse;
            b.set(n,n,pivot_inverse);
            for (int j = 0; j < TILESIZE; j++) 
            {
                if (j == n) continue;
                row[j] = b.get(n,j) * pivot_inverse;
                b.set(n,j,row[j]);
            }
        
            for (int i = 0; i < TILESIZE; i++)
            {
                if (i == n) continue;
                double tin = b.get(i,n);
                b.set(i,n, -tin*row[n]); 
                for (int j = 0; j < TILESIZE; j++) 
                {
                    if (j == n) continue;
                    b.set(i,j, b.get(i,j) - tin*row[j]);
                }
            }
        }
	}

    //b = inverse(*this)
    tile inverse() const
    {
        tile b;
        inverse_( b );
        return b;
    }

    // c = this * b
    void multiply_( const tile &b, tile & c ) const
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = 0.0;
                for (int k = 0; k < TILESIZE; k++)
                    t += get(i,k) * b.get(k,j);

                c.set(i,j,t);
            }
        }
    }

    tile multiply( const tile &b ) const
    {
        tile c;
        multiply_( b, c );
        return c;
    }

    // c = -(this * b)
    void multiply_negate_( const tile& b, tile & c ) const
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = 0.0;
                for (int k = 0; k < TILESIZE; k++)
                    t -= get(i,k) * b.get(k,j);
                c.set(i,j,t);
            }
        }
    }

    tile multiply_negate( const tile& b ) const
    {
        tile c;
        multiply_negate_( b, c );
        return c;
    }
    
    // d = this - (b * c)
    tile multiply_subtract(const tile& b, const tile& c ) const
    {
        tile d;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t -= b.get(i,k) * c.get(k,j);
                d.set(i,j,t);
            }
        }
        return d;
    }

    // this =  this - (b * c)
    void multiply_subtract_in_place( const tile& b, const tile& c )
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t -= b.get(i,k) * c.get(k,j);
                set(i,j,t);
            }
        }
    }

    // d = this + (b * c)
    tile multiply_add( const tile& b, const tile& c ) const
    {
        tile d;
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t += b.get(i,k) * c.get(k,j);
                d.set(i,j,t);
            }
        }
        return d;
    }

    // this = this + (b * c)
    void multiply_add_in_place( const tile& b, const tile& c )
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                double t = get(i,j);
                for (int k = 0; k < TILESIZE; k++)
                    t += b.get(i,k) * c.get(k,j);
                set(i,j,t);
            }
        }
    }

    // this = 0.0;
    void zero()
    {
        for (int i = 0; i < TILESIZE; i++) 
        {
            for (int j = 0; j < TILESIZE; j++)
            {
                set(i,j,0.0);
            }
        }
    }
};

#ifdef _DIST_
CNC_BITWISE_SERIALIZABLE( tile );
#endif
