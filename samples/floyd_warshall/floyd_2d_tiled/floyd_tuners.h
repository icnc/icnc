/*Copyright (c) 2013-, Ravi Teja Mullapudi, CSA Indian Institue of Science
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of the Indian Institute of Science nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _FLOYD_TUNERS_INCLUDED_
#define _FLOYD_TUNERS_INCLUDED_

inline int compute_on( const dist_type dist, const int i, const int j, 
                       const int num_blocks, const int num_procs )
{
    switch( dist ) {
        default:
        case ROW_CYCLIC :
            return i % num_procs;
            break;
        case COLUMN_CYCLIC :
            return j % num_procs;
            break;
        case BLOCKED_CYCLIC :
            return ( (i) * num_blocks + (j) ) % num_procs;
            break;
        case BLOCKED_ROW :
            int blocks_per_proc = num_blocks/num_procs;
            return i/blocks_per_proc;
            break;
    }
}

struct context_dist_2d;

struct compute_step_dist_2d
{
    int execute( const Triple &t , context_dist_2d & c ) const;
};

struct node_gen_step_dist_2d
{
    int execute(const int &p, context_dist_2d &c) const;
};

struct tag_gen_step_dist_2d
{
    int execute(const int &p, context_dist_2d &c) const;
};

template< bool COL >
struct tuner_colrow_transfer:public CnC::hashmap_tuner
{
    tuner_colrow_transfer(context_dist_2d &c, dist_type dt, int matrix_size, int block_size)
        :m_context(c),
        m_dist( dt ),
        matrix_size(matrix_size),
        block_size(block_size)
    { }

    std::vector<int> consumed_on( const Pair & tag ) const 
    {
        int num_blocks = matrix_size/block_size;
        int k = tag.first, i = tag.second;

        if (k == matrix_size)
            return std::vector<int>();

        bool * _d;
        _d = new bool[numProcs()];
        memset( _d, 0, numProcs() * sizeof( *_d ) );

        for (int p = 0; p < num_blocks; p++)
            _d[compute_on(m_dist, COL?i:p, COL?p:i, num_blocks, numProcs())] = true;

        std::vector< int > _v;
        if( _d[myPid()] ) _v.push_back( myPid() );
        for( int i = 0; i < numProcs(); ++i ) {
            if( _d[i] && i != myPid() ) _v.push_back( i );
        }

        return _v;
    }

    int produced_on( const Pair & tag) const
    {
        int k = tag.first, i = tag.second;
        int num_blocks = matrix_size/block_size;
        if (k == 0)
            return CnC::PRODUCER_UNKNOWN;

        return compute_on(m_dist, COL?i:(k)/block_size, COL?(k)/block_size:i, num_blocks, numProcs());
    }   
    
    int get_count(const Pair & tag) const 
    {
        int num_blocks = matrix_size/block_size;
        return num_blocks-1; 
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & matrix_size & block_size & m_dist;
    }
#endif
private:
    context_dist_2d & m_context;
    int matrix_size;
    int block_size;
    dist_type m_dist;
                                                             
};

typedef tuner_colrow_transfer< true > tuner_col_transfer;
typedef tuner_colrow_transfer< false > tuner_row_transfer;

struct tuner_dist_2d : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    tuner_dist_2d(context_dist_2d &c, dist_type dt, int matrix_size, int block_size)
        :m_context(c),
         m_dist( dt ),
         matrix_size(matrix_size),        
         block_size(block_size)
    {
        if( myPid() == 0 ) {
            printf("Paritioning: 2d Blocks\n");
            switch( dt ) {
                default:
                case ROW_CYCLIC :
                    printf("Distribution: ROW CYCLIC\n");
                    break;
                case COLUMN_CYCLIC :
                    printf("Distribution: COLUMN CYCLIC\n");
                    break;
                case BLOCKED_CYCLIC :
                    printf("Distribution: BLOCKED CYCLIC\n");
                    break;
            }
        }
    }

    template< class dependency_consumer >
        void depends( const Triple & tag, context_dist_2d & c, dependency_consumer & dC ) const;

    template< class dependency_consumer >
        void depends( const int & tag, context_dist_2d & c, dependency_consumer & dC ) const { }

    int compute_on( const int & tag, context_dist_2d & ) const
    {
        return tag%numProcs();
    }

    int compute_on( const Triple & tag, context_dist_2d & ) const 
    { 
        int num_blocks = matrix_size/block_size;
        int node = ::compute_on(m_dist, tag[1], tag[2], num_blocks, numProcs());
        return node;
    }

    int priority( const Triple & tag, context_dist_2d & ) const 
    {
        int k = tag[0], i = tag[1], j = tag[2];
        int priority = 1;
        if (i == (k/block_size))
            priority++;
        if (j == (k/block_size))
            priority++;
        return priority;
    }

    int priority( const int & tag, context_dist_2d & ) const 
    { 
        return 1;
    }

    int consumed_on( const Triple & tag ) const
    {
        int k = tag[0], i = tag[1], j = tag[2];
        if (k == matrix_size)
            return 0;
        return compute_on(tag, m_context);
    }

    int produced_on( const Triple & tag) const
    {
        int k = tag[0], i = tag[1], j = tag[2];
        int num_blocks = matrix_size/block_size;
        if (k == 0)
            return CnC::PRODUCER_UNKNOWN;
        
        return ::compute_on(m_dist, i, j, num_blocks, numProcs());
    }

    int get_count(const Triple & tag) const 
    {
        if( tag[0] == matrix_size ) return CnC::NO_GETCOUNT;
        return 1;
    } 

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & matrix_size & block_size & m_dist;
    }
#endif
private:
    context_dist_2d & m_context;
    int matrix_size;
    int block_size;
    dist_type m_dist;
};

#endif // _FLOYD_TUNERS_INCLUDED_
