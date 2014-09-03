#include <iostream>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include "tbb/tick_count.h"
#include "cnc/cnc.h"
#include "cnc/debug.h"
#include <cassert>

#define THRESHOLD 25000

namespace {
    template <typename argument_type>
    static void swap( argument_type * a, argument_type * b )
    {
        argument_type t = * a;
        * a = * b;
        * b = t;
    }

    template <typename argument_type>
    static void vector_dump (const argument_type * array, const size_t size )
    {
        std::cout << array[0];
        for (size_t i = 1 ; i < size ; ++i ) {
            std::cout << ", " << array[ i ];
        }
        std::cout << std::endl;
    }

    template <typename T>
    static void vector_init ( T* array, const size_t size , size_t mod )
    {
        assert ( 0 );
    }

    template <>
    static void vector_init <int> ( int * array, const size_t size , size_t mod )
    {
        for (size_t i = 0 ; i < size ; ++i ) {
            array[ i ] = rand() % mod ;
        }
    }

    template <>
    static void vector_init <double> ( double * array, const size_t size , size_t mod )
    {
        for (size_t i = 0 ; i < size ; ++i ) {
            double tmp = rand() % mod ; 
            array[ i ] = tmp / mod ;
        }
    }

    // these two functions are from the tstream code on the svn
    template <typename argument_type, typename Comparator /*= std::less_equal< argument_type >*/ >
    static void serial_quicksort_helper(argument_type * array, size_t size, bool verbose, const Comparator & compareFunctor )
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
                size_t i = partition( array , size , verbose , compareFunctor );
                if ( verbose ) {
                    vector_dump( array , size );
                }
                serial_quicksort_helper( array , i , verbose , compareFunctor );
                serial_quicksort_helper( & array[ i + 1 ], size - ( i + 1 ) , verbose , compareFunctor );
            }
        } 
    }

    template <typename argument_type, typename Comparator /*= std::less_equal< argument_type >*/ >
    static size_t partition( argument_type * array , size_t size , bool verbose , const Comparator & compareFunctor)
    {
        size_t middleIndex = size / 2;
        swap( & array[ 0 ], & array[ middleIndex ] );
        
        argument_type pivot = array[ 0 ];
    
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
            //} while ( array[ j ] > pivot );
            } while ( !compareFunctor ( array[ j ] , pivot ) ); 

            if (verbose) {
                std::cout << "i(" << i << ") j(" << j << ")" << std::endl;
            }

            do {
                assert( i <= j );
                if ( i == j ) goto done;
                ++i;
            }
            //while ( array[ i ] < pivot );
            while ( compareFunctor ( array[ i ] , pivot ) && array[ i ] != pivot );

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
}

template <typename vector_member_type , typename Comparator >
class vector_base {
    public:
        vector_base(size_t size, bool verbose)
            : m_size(size),
              m_array( NULL ),
              m_verbose ( verbose ) 
        {}

        vector_base()
            : m_size( 0 ),
              m_array( NULL ),
              m_verbose ( false ) 
        {}

        virtual ~vector_base(){}

        size_t size() const
        {
            return m_size;
        }

        void serial_quicksort()
        {
            serial_quicksort_helper( m_array , m_size , m_verbose , m_compareFunctor );
        }

        void print() const
        {
            vector_dump( m_array , m_size );
        }

        bool isSorted() const
        {
            bool sortedTillNow = true;
            if( m_size > 1 ) {
                size_t size = m_size;
                argument_type last( m_array[ 0 ] );

                for ( size_t i = 1 ; i < size && sortedTillNow ; ++i ) {
                    sortedTillNow = sortedTillNow && m_compareFunctor( last , m_array[ i ] ); //m_array[ i ] >= last;
                    last = m_array[ i ];
                }
            }
            return sortedTillNow ;
        }

        enum { LEFT = 1 , RIGHT = 2 };
    protected:
        Comparator m_compareFunctor;
        typedef vector_member_type argument_type;
        argument_type * m_array;
        size_t m_size;
        bool m_verbose;
};

// there are some problems with the default template arguments, so avoid using non-default
template <class vector_member_type , typename Comparator = std::less_equal< vector_member_type > >
class my_vector_type : public vector_base < vector_member_type , Comparator > {
    typedef typename vector_base< vector_member_type , Comparator >::argument_type argument_type;
    public:
        my_vector_type( ) 
            : vector_base < vector_member_type , Comparator >( ),
              shouldBeFreed ( true ), 
              isPartitioned ( false )
        {
        }
        my_vector_type( size_t size , bool verbose ) 
            : vector_base < vector_member_type , Comparator >( size , verbose ),
              shouldBeFreed ( true ), 
              isPartitioned ( false )
        {
            this->m_array = new argument_type[ size ];
            vector_init<argument_type> ( this->m_array , size , size * 100 );
        }

        ~my_vector_type()
        {
            if ( shouldBeFreed ) {
                delete [] this->m_array;
            }
        }

        size_t partitionVector ()
        {
            size_t toBeReturned = partition( this->m_array , this->m_size, this->m_verbose , this->m_compareFunctor );
            isPartitioned = true ;
            return toBeReturned;
        }

        argument_type getIndex ( size_t index ) const
        {
            return this->m_array[ index ];
        }

        my_vector_type(const my_vector_type & leftChild, argument_type pivot , const my_vector_type & rightChild) 
            : vector_base< vector_member_type , Comparator >( leftChild.m_size + rightChild.m_size + 1 , rightChild.m_verbose),
              shouldBeFreed ( true )
        {
            this->m_array = new argument_type[ this->m_size ];
            size_t index = 0 ;
            size_t leftSize = leftChild.m_size;
            for ( ; index < leftSize ; ++index ) {
                this->m_array[ index ] = leftChild.m_array[ index ];
            }
            this->m_array[ index ] = pivot;
            size_t rightSize = rightChild.m_size;
            for ( size_t i = 0 ; i < rightSize ; ++i ) {
                this->m_array[ ++index ] = rightChild.m_array[ i ];
            }
        }

        // default DEEP COPY copy constructor for getting and putting
        my_vector_type(const my_vector_type & copied) 
            : vector_base< vector_member_type , Comparator >( copied ),
              shouldBeFreed ( true )
        {
            this->m_array = new argument_type[ this->m_size ];
            size_t size = this->m_size;
            for ( size_t i = 0 ; i < size ; ++i ) {
                this->m_array[ i ] = copied.m_array[ i ];
            }
        }

        // SHALLOW COPY copy constructor for child creation
        // the vector SHOULD already have been partitioned
        // they will be deep copied when being get or being put
        my_vector_type( my_vector_type & copied , size_t pivotIndex, int childIndex) 
            : vector_base< vector_member_type , Comparator >( copied ),
              shouldBeFreed ( false ), 
              isPartitioned ( false )
        {
            assert ( copied.isPartitioned );  

            if ( childIndex == this->LEFT ) {
                this->m_size = pivotIndex ;
            } else if ( childIndex == this->RIGHT ) {
                this->m_size = copied.m_size - pivotIndex - 1;
                this->m_array += ( pivotIndex + 1 );
            } else assert ( 0 );
        }

        my_vector_type& operator=(const my_vector_type & copied)
        {
            delete [] this->m_array;
            this->m_size = copied.m_size;
            this->m_array = new argument_type[ this->m_size ];
            size_t size = this->m_size;
            for ( size_t i = 0 ; i < size ; ++i ) {
                this->m_array[ i ] = copied.m_array[ i ];
            }
            return *this;
        }

    private:
        bool shouldBeFreed;
        bool isPartitioned;
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

template < typename vector_member_type >
struct quick_sort_context;

template < typename vector_member_type >
struct quick_sort_split_step {
    typedef quick_sort_context < vector_member_type > template_quick_sort_context_type;
    typedef typename template_quick_sort_context_type::vector_type vector_type;

    int execute( ancestry_path_tag_type ancestry, template_quick_sort_context_type & graph) const;
};

template < typename vector_member_type >
struct quick_sort_concat_step {
    typedef quick_sort_context < vector_member_type > template_quick_sort_context_type;
    typedef typename template_quick_sort_context_type::vector_type vector_type;

    int execute( ancestry_path_tag_type ancestry, template_quick_sort_context_type & graph) const;
};

template < typename vector_member_type >
struct quick_sort_context: public CnC::context< quick_sort_context < vector_member_type > > {

    typedef my_vector_type < vector_member_type > vector_type ;

    CnC::tag_collection  < ancestry_path_tag_type >                 ancestryPathSplitTagSpace;
    CnC::tag_collection  < ancestry_path_tag_type >                 ancestryPathConcatTagSpace;
    CnC::item_collection < ancestry_path_tag_type , vector_type >   unsortedVectorSpace;

    CnC::item_collection < ancestry_path_tag_type , vector_type >   sortedVectorSpace;
    CnC::item_collection < ancestry_path_tag_type , vector_member_type > pivotSpace;

    quick_sort_context < vector_member_type > ()
        : CnC::context< quick_sort_context < vector_member_type > >(),
          ancestryPathSplitTagSpace ( *this ), 
          ancestryPathConcatTagSpace ( *this ), 
          sortedVectorSpace ( *this ), 
          pivotSpace ( *this ), 
          unsortedVectorSpace ( *this ) 
    {
        prescribe(ancestryPathSplitTagSpace , quick_sort_split_step < vector_member_type > () );
        prescribe(ancestryPathConcatTagSpace , quick_sort_concat_step < vector_member_type > () );
    }
};

template < typename vector_member_type >
int quick_sort_split_step < vector_member_type > ::execute(ancestry_path_tag_type ancestry, template_quick_sort_context_type & graph) const
{
    vector_type currentVector ( graph.unsortedVectorSpace.get( ancestry ) );
    size_t currentVectorSize = currentVector.size();

    if ( currentVectorSize <= THRESHOLD ) {
        currentVector.serial_quicksort();
        graph.sortedVectorSpace.put( ancestry , currentVector );
        if ( ancestry > 0 ) { // concat parent, may be pushed twice but no problem
            graph.ancestryPathConcatTagSpace.put( computeParentTag ( ancestry ) ); 
        }
    } else {
        size_t pivotIndex = currentVector.partitionVector();
        graph.pivotSpace.put ( ancestry , currentVector.getIndex(pivotIndex) );

        // ATTENTION! in order to avoid unnecessary copying when creating a child 
        // this copy constructor uses SHALLOW COPYING, destructor does not free memory 
        vector_type rightChildVector( currentVector, pivotIndex, vector_type::RIGHT );      
        ancestry_path_tag_type rightChildTag = computeChildTag( ancestry, vector_type::RIGHT );

        graph.unsortedVectorSpace.put( rightChildTag , rightChildVector );
        graph.ancestryPathSplitTagSpace.put( rightChildTag );

        vector_type leftChildVector( currentVector, pivotIndex, vector_type::LEFT );      
        ancestry_path_tag_type leftChildTag = computeChildTag( ancestry, vector_type::LEFT );

        graph.unsortedVectorSpace.put( leftChildTag , leftChildVector );
        graph.ancestryPathSplitTagSpace.put( leftChildTag );
    }

    return CnC::CNC_Success;
}

template < typename vector_member_type >
int quick_sort_concat_step < vector_member_type > ::execute(ancestry_path_tag_type ancestry, template_quick_sort_context_type & graph) const
{
    vector_type rightChildVector ( graph.sortedVectorSpace.get( computeChildTag( ancestry , vector_type::RIGHT ) ) );
    vector_type leftChildVector ( graph.sortedVectorSpace.get( computeChildTag( ancestry , vector_type::LEFT ) ) );
    vector_member_type pivot ( graph.pivotSpace.get( ancestry ) );

    vector_type currentSortedVector ( leftChildVector , pivot , rightChildVector ) ;
    graph.sortedVectorSpace.put ( ancestry , currentSortedVector ) ;
    if ( ancestry > 0 ) { // concat parent, may be pushed twice but no problem
        graph.ancestryPathConcatTagSpace.put( computeParentTag ( ancestry ) ); 
    }

    return CnC::CNC_Success;
}

int main(int argc, char* argv[])
{
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

        // my_vector_type < double , std::greater< double > > integerVector ( size_t ( size ) , verbose );
        // default template parameters are problematic for now, so go with the default less than or equal to 
        my_vector_type < double > integerVector ( size_t ( size ) , verbose );

        tbb::tick_count t0 = tbb::tick_count::now();

        quick_sort_context < double > graph;

        graph.unsortedVectorSpace.put( ancestry_path_tag_type ( 0 ) , integerVector ); // 0 is the root tag
        graph.ancestryPathSplitTagSpace.put( 0 ); 

        // default template parameters are problematic for now, so go with the default less than or equal to 
        // my_vector_type < double , std::greater< double > > integerVector ( size_t ( size ) , verbose );
        // my_vector_type < double , std::greater< double > > sorted ( graph.sortedVectorSpace.get( 0 ) ) ; // 0 is the root tag
        my_vector_type < double > sorted ( graph.sortedVectorSpace.get( 0 ) ) ; // 0 is the root tag
        graph.wait();
        tbb::tick_count t1 = tbb::tick_count::now();
        // execution finished 
        assert ( sorted.size() == integerVector.size() );
        assert ( sorted.isSorted() );

        printf("Computed in %g seconds\n", (t1-t0).seconds());
    }
    return toBeReturned;
}
