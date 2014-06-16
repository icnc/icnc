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
                       const int num_blocks, const int num_procs)
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

struct context_dist_1d;

struct compute_step_dist_1d
{
    int execute( const Pair &t , context_dist_1d & c ) const;
};

struct node_gen_step_dist_1d
{
    int execute(const int &p, context_dist_1d &c) const;
};

struct tag_gen_step_dist_1d
{
    int execute(const int &p, context_dist_1d &c) const;
};

struct tuner_row_transfer:public CnC::hashmap_tuner
{
    tuner_row_transfer(context_dist_1d &c, dist_type dt, int matrix_size, int block_size)
        :m_context(c),
        m_dist( dt ),
        matrix_size(matrix_size),
        block_size(block_size)
    { }

    int consumed_on( const int &tag ) const 
    {
        return CnC::CONSUMER_ALL;
    }

    int produced_on( const int & tag) const
    {
        int num_blocks = matrix_size/block_size;
        if (tag == 0)
            return CnC::PRODUCER_UNKNOWN;

        return compute_on(m_dist, (tag)/block_size, 0, num_blocks, numProcs());
    }   
    
    int get_count(const int & tag) const 
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
    context_dist_1d & m_context;
    int matrix_size;
    int block_size;
    dist_type m_dist;
                                                             
};

struct tuner_dist_1d : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    tuner_dist_1d(context_dist_1d &c, dist_type dt, int matrix_size, int block_size)
        :m_context(c),
         m_dist( dt ),
         matrix_size(matrix_size),        
         block_size(block_size)
    {
        if( myPid() == 0 ) {
            std::cout << "Paritioning: 1d Blocks\n";
            switch( dt ) {
                default:
                case ROW_CYCLIC :
                    std::cout << "Distribution: ROW CYCLIC\n";
                    break;
                case BLOCKED_ROW :
                    std::cout << "Distribution: BLOCKED ROW\n";
                    break;
            }
        }
    }

    template< class dependency_consumer >
        void depends( const Pair & tag, context_dist_1d & c, dependency_consumer & dC ) const;

    template< class dependency_consumer >
        void depends( const int & tag, context_dist_1d & c, dependency_consumer & dC ) const { }
 
    int compute_on( const int & tag, context_dist_1d & ) const
    {
        return tag%numProcs();
    }

    int compute_on( const Pair & tag, context_dist_1d & ) const 
    { 
        int num_blocks = matrix_size/block_size;
        int node = ::compute_on(m_dist, tag.second, 0, num_blocks, numProcs());
        return node;
    }

    int priority( const Pair & tag, context_dist_1d & ) const 
    { 
        int k = tag.first, i = tag.second;
        int priority = 1;
        if (i == (k/block_size))
            priority++;
        return priority;
    }

    int priority( const int & tag, context_dist_1d & ) const 
    { 
        return 1;
    }

    std::vector<int> consumed_on( const Pair & tag ) const 
    {
        int num_blocks = matrix_size/block_size;
        int k = tag.first, i = tag.second;

        if (k == matrix_size)
            return std::vector<int>();

        return std::vector< int >( 1, compute_on(Pair(k, i), m_context));
    }

    int produced_on( const Pair & tag) const
    {
        int k = tag.first, i = tag.second;
        int num_blocks = matrix_size/block_size;
        if (k == 0)
            return CnC::PRODUCER_UNKNOWN;
        
        return ::compute_on(m_dist, i, 0, num_blocks, numProcs());
    }

    int get_count(const Pair & tag) const 
    {
        if( tag.first == matrix_size ) return CnC::NO_GETCOUNT;
        return 1;
    } 

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & matrix_size & block_size & m_dist;
    }
#endif
private:
    context_dist_1d & m_context;
    int matrix_size;
    int block_size;
    dist_type m_dist;
};

#endif // _FLOYD_TUNERS_INCLUDED_
