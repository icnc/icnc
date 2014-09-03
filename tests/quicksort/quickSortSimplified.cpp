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

#include <iostream>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include "tbb/tick_count.h"
#ifdef _DIST_
#include "cnc/dist_cnc.h"
#else
#include "cnc/cnc.h"
#endif
#include <cassert>
#include "cnc/debug.h"

#define THRESHOLD 25000

namespace {
    static void swap( int * a, int * b )
    {
        int t = * a;
        * a = * b;
        * b = t;
    }

    static void vector_dump (const int * array, const size_t size )
    {
        std::cout << array[0];
        for (size_t i = 1 ; i < size ; ++i ) {
            std::cout << ", " << array[ i ];
        }
        std::cout << std::endl;
    }

    static void vector_init ( int * array, const size_t size , size_t mod )
    {
        for (size_t i = 0 ; i < size ; ++i ) {
            array[ i ] = rand() % mod ;
        }
    }

    // these two functions are from the tstream code on the svn
    
    static size_t partition( int * array , size_t size , bool verbose )
    {
        size_t middleIndex = size / 2;
        swap( & array[ 0 ], & array[ middleIndex ] );
        
        int pivot = array[ 0 ];
    
        size_t i = 0;
        size_t j = size;
        // partition [i+1,j-1] with key
        for(;;) {
            if (verbose) {
                std::cout << "for(;;)" << std::endl;
                vector_dump( array , size );
            }
            
            assert(i < j);
            do {
                --j;
                assert( i <= j );
            } while ( array[ j ] > pivot );

            if (verbose) {
                std::cout << "i(" << i << ") j(" << j << ")" << std::endl;
            }

            do {
                assert( i <= j );
                if ( i == j ) goto done;
                ++i;
            }
            while ( array[ i ] < pivot );

            if (verbose) {
                std::cout << "i(" << i << ") j(" << j << ")" << std::endl;
            }

            if ( i == j ) goto done;
            swap( & array[ i ], & array[ j ] );
        }
        
    done:
        if (verbose) {
            std::cout << "i(" << i << ") j(" << j << ")" << std::endl;
        }

        assert( i == j );
        swap( & array[ 0 ], & array[ i ]);
    
        return i;
    }

    static void serial_quicksort_helper( int * array, size_t size, bool verbose )
    {
        if (size > 1) {
            if (size == 2) {
                if ( array[ 1 ] < array[ 0 ]) {
                    swap ( & array[ 0 ], & array [ 1 ]);
                }
            } else {
                if ( verbose ) {
                    std::cout << "qs( " << std::hex << array << std::dec << "," << size << ")" << std::endl;
                    vector_dump( array , size );
                }
                size_t i = partition( array , size , verbose );
                if ( verbose ) {
                    vector_dump( array , size );
                }
                serial_quicksort_helper( array , i , verbose );
                serial_quicksort_helper( & array[ i + 1 ], size - ( i + 1 ) , verbose );
            }
        } 
    }

}

class my_vector_type {
    public:
        my_vector_type( ) 
            : m_array ( NULL ) ,
              m_size ( 0 ) , 
              m_verbose ( false ) ,
              m_isPartitioned ( false )
        {
        }

        my_vector_type( size_t size , bool verbose ) 
            : m_array ( NULL ) ,
              m_size ( size ) , 
              m_verbose ( false ) ,
              m_isPartitioned ( false )
        {
            m_array = new int[ size ];
            vector_init ( m_array , size , size * 100 );
        }

        my_vector_type& operator=(const my_vector_type & copied)
        {
            delete [] m_array;
                copyHelper ( 0, copied.m_size , copied.m_array );
            return *this;
        }

        my_vector_type(const my_vector_type & copied) 
            : m_array ( NULL ) ,
              m_size ( copied.m_size ) , 
              m_verbose ( copied.m_verbose ) ,
              m_isPartitioned ( copied.m_isPartitioned )
        {
                copyHelper ( 0, copied.m_size , copied.m_array );
        }

        my_vector_type(const my_vector_type & leftChild, int pivot , const my_vector_type & rightChild) 
            : m_array ( NULL ) ,
              m_size ( leftChild.m_size + rightChild.m_size + 1 ) , 
              m_verbose ( rightChild.m_verbose ) ,
              m_isPartitioned ( false )
        {
            m_array = new int[ m_size ];
            size_t index = 0 ;
            size_t leftSize = leftChild.m_size;
            for ( ; index < leftSize ; ++index ) {
                m_array[ index ] = leftChild.m_array[ index ];
            }
            m_array[ index ] = pivot;
            size_t rightSize = rightChild.m_size;
            for ( size_t i = 0 ; i < rightSize ; ++i ) {
                m_array[ ++index ] = rightChild.m_array[ i ];
            }
        }

        my_vector_type( my_vector_type & copied , size_t pivotIndex, int childIndex) 
            : m_array ( NULL ) ,
              m_size ( 0 ) , 
              m_verbose ( copied.m_verbose ) ,
              m_isPartitioned ( false )
        {
            assert ( copied.m_isPartitioned );  

            if ( childIndex == LEFT ) {
                copyHelper( 0 , pivotIndex , copied.m_array );
            } else if ( childIndex == RIGHT ) {
                copyHelper( pivotIndex + 1 , copied.m_size , copied.m_array );
            } else assert ( 0 );
        }

        ~my_vector_type() { delete [] m_array; }

          void serial_quicksort()
        {
            serial_quicksort_helper( m_array , m_size , m_verbose );
        }        

          size_t partitionVector ()
        {
            size_t toBeReturned = partition( m_array , m_size, m_verbose );
            m_isPartitioned = true ;
            return toBeReturned;
        }

        bool isSorted() const
        {
            bool sortedTillNow = true;
            if( m_size > 1 ) {
                size_t size = m_size;
                int last( m_array[ 0 ] );

                for ( size_t i = 1 ; i < size && sortedTillNow ; ++i ) {
                    sortedTillNow = sortedTillNow && ( m_array[ i ] >= last ); 
                    last = m_array[ i ];
                }
            }
            return sortedTillNow ;
        }

        int getIndex ( size_t index ) const { return m_array[ index ]; }
        size_t size() const { return m_size; }
        void print() const { vector_dump( m_array , m_size ); }
        enum { LEFT = 1 , RIGHT = 2 };

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_isPartitioned & m_size & m_verbose;
        if( ser.is_unpacking() ) m_array = new int[m_size];
        ser & CnC::chunk< int, CnC::no_alloc >( m_array, m_size );
    }
#endif

    private:
        bool m_isPartitioned;
        size_t m_size;
        int * m_array;
        bool m_verbose;

        void copyHelper( size_t begin, size_t end , const int * copyFrom )
          {
            m_size = end - begin;
            m_array = new int[ m_size ];
            for ( size_t i = 0 ; i < m_size ; ++i ) {
                m_array[ i ] = copyFrom [ begin + i ];
            }
          }
};

namespace {
    typedef int ancestry_path_tag_type;

    inline ancestry_path_tag_type computeChildTag( ancestry_path_tag_type parent, int childIndex)
    {
        return ( parent << 1 ) + childIndex;
    } 
    inline ancestry_path_tag_type computeParentTag( ancestry_path_tag_type child )
    {
        return ( child - 1 ) >> 1 ;
    } 
}

struct quick_sort_context;

struct quick_sort_split_step {
    int execute( ancestry_path_tag_type ancestry, quick_sort_context & graph) const;
};

struct quick_sort_concat_step {
    int execute( ancestry_path_tag_type ancestry, quick_sort_context & graph) const;
};

static int LIM = 64;

struct item_tuner : public CnC::hashmap_tuner
{
    int consumed_on( const ancestry_path_tag_type & tag ) const
    {
        int _p( computeParentTag( tag ) );
        return ( _p >= 0 ? ( _p >= LIM 
                             ? CnC::CONSUMER_LOCAL
                             : ( ( _p * 15502547 ) % 11057 ) % numProcs() )
                         : 0 );
    }
};

struct quick_sort_split_tuner : public CnC::step_tuner<>
{
    int compute_on( const ancestry_path_tag_type & tag, quick_sort_context & ) const
    {
        int _p( computeParentTag( tag ) );
        return ( _p >= 0 ? ( _p >= LIM 
                             ? CnC::COMPUTE_ON_LOCAL 
                             : ( ( _p * 15502547 ) % 11057 ) % numProcs() )
                         : 0 );
    }
};

struct quick_sort_concat_tuner : public CnC::step_tuner<>
{
    int compute_on( const ancestry_path_tag_type & tag, quick_sort_context & ) const
    {
        return tag >= LIM ? CnC::COMPUTE_ON_LOCAL : ( ( tag * 15502547 ) % 11057 ) % numProcs();
    }
};

struct pivot_item_tuner : public CnC::hashmap_tuner
{
    int consumed_on( const ancestry_path_tag_type & tag ) const
    {
        return tag >= LIM ? CnC::CONSUMER_LOCAL : ( ( tag * 15502547 ) % 11057 ) % numProcs();
    }
};


struct quick_sort_context: public CnC::context< quick_sort_context >
{
    CnC::step_collection< quick_sort_split_step, quick_sort_split_tuner > m_splitSteps;
    CnC::step_collection< quick_sort_concat_step, quick_sort_concat_tuner > m_concatSteps;
    CnC::tag_collection  < ancestry_path_tag_type, CnC::preserve_tuner< ancestry_path_tag_type > >  ancestryPathSplitTagSpace;
    CnC::tag_collection  < ancestry_path_tag_type, CnC::preserve_tuner< ancestry_path_tag_type > >  ancestryPathConcatTagSpace;
    CnC::item_collection < ancestry_path_tag_type, my_vector_type, item_tuner >   unsortedVectorSpace;
    CnC::item_collection < ancestry_path_tag_type, my_vector_type, item_tuner >   sortedVectorSpace;
    CnC::item_collection < ancestry_path_tag_type, int, pivot_item_tuner >              pivotSpace;

    quick_sort_context ()
        : CnC::context< quick_sort_context >(),
          m_splitSteps( *this ),
          m_concatSteps( *this ),
          ancestryPathSplitTagSpace ( *this ), 
          ancestryPathConcatTagSpace ( *this ), 
          sortedVectorSpace ( *this ), 
          pivotSpace ( *this ), 
          unsortedVectorSpace ( *this ) 
    {
        ancestryPathSplitTagSpace.prescribes( m_splitSteps, *this );
        ancestryPathConcatTagSpace.prescribes( m_concatSteps, *this );
        //CnC::debug::trace( ancestryPathSplitTagSpace, "ancestryPathSplitTagSpace" );
        //CnC::debug::trace( ancestryPathConcatTagSpace, "ancestryPathConcatTagSpace" );
        // CnC::debug::trace( unsortedVectorSpace, "unsortedVectorSpace" );
        // CnC::debug::trace( sortedVectorSpace, "sortedVectorSpace" );
        //CnC::debug::trace( pivotSpace, "pivotSpace" );
        // CnC::debug::trace( quick_sort_split_step(), "quick_sort_split_step" );
        //CnC::debug::trace( quick_sort_concat_step(), "quick_sort_concat_step" );
    }
};

int quick_sort_split_step::execute(ancestry_path_tag_type ancestry, quick_sort_context & graph) const
{
    VT_FUNC( "quick_sort_split_step::execute" );
    my_vector_type currentVector;
    graph.unsortedVectorSpace.get( ancestry, currentVector );
    size_t currentVectorSize = currentVector.size();

    if ( currentVectorSize <= THRESHOLD ) {
        currentVector.serial_quicksort();
        graph.sortedVectorSpace.put( ancestry , currentVector );
    } else {
        size_t pivotIndex = currentVector.partitionVector();
        graph.pivotSpace.put ( ancestry , currentVector.getIndex(pivotIndex) );

        my_vector_type rightChildVector( currentVector, pivotIndex, my_vector_type::RIGHT );      
        ancestry_path_tag_type rightChildTag = computeChildTag( ancestry, my_vector_type::RIGHT );

        graph.unsortedVectorSpace.put( rightChildTag , rightChildVector );
        graph.ancestryPathSplitTagSpace.put( rightChildTag );

        my_vector_type leftChildVector( currentVector, pivotIndex, my_vector_type::LEFT );      
        ancestry_path_tag_type leftChildTag = computeChildTag( ancestry, my_vector_type::LEFT );

        graph.unsortedVectorSpace.put( leftChildTag , leftChildVector );
        graph.ancestryPathSplitTagSpace.put( leftChildTag );

        // put tag for concat-step
        graph.ancestryPathConcatTagSpace.put( ancestry );
    }

    return CnC::CNC_Success;
}

int quick_sort_concat_step::execute(ancestry_path_tag_type ancestry, quick_sort_context & graph) const
{
    VT_FUNC( "quick_sort_concat_step::execute" );
    my_vector_type leftChildVector;
    graph.sortedVectorSpace.get( computeChildTag( ancestry , my_vector_type::LEFT ), leftChildVector );
    my_vector_type rightChildVector;
    graph.sortedVectorSpace.get( computeChildTag( ancestry , my_vector_type::RIGHT ), rightChildVector );
    int pivot;
    graph.pivotSpace.get( ancestry, pivot );

    my_vector_type currentSortedVector ( leftChildVector , pivot , rightChildVector ) ;
    graph.sortedVectorSpace.put ( ancestry , currentSortedVector ) ;

    return CnC::CNC_Success;
}

int main(int argc, char* argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< quick_sort_context > dc_init;
#endif
    int toBeReturned = 0;
    setbuf( stdout , NULL );
    setbuf( stderr , NULL );
    if ( argc < 2 ) {
        std::cerr<<"usage: quickSort $(array length) [-v]\n";
        toBeReturned = 1;
    } else {
        srand(0xdeadbeef);

        int size =  atoi ( argv[ 1 ] );
        bool verbose = argc == 3 && ! strcmp( "-v" , argv[ 2 ] );

        my_vector_type integerVector ( size_t ( size ) , verbose );

        tbb::tick_count t0 = tbb::tick_count::now();

        quick_sort_context graph;

        graph.unsortedVectorSpace.put( ancestry_path_tag_type ( 0 ) , integerVector ); // 0 is the root tag
        graph.ancestryPathSplitTagSpace.put( 0 ); 

        graph.wait();
        my_vector_type sorted;
        graph.sortedVectorSpace.get( 0, sorted ) ; // 0 is the root tag
        tbb::tick_count t1 = tbb::tick_count::now();
        // execution finished 
        assert ( sorted.size() == integerVector.size() );
        assert ( sorted.isSorted() );

        printf("Computed in %g seconds\n", (t1-t0).seconds());
    }
    return toBeReturned;
}
