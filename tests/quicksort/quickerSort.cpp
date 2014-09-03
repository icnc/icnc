#include <iostream>
#include <functional>
#include <stdio.h>
#include <stdlib.h>
#include "tbb/tick_count.h"
#include "cnc/cnc.h"
#include "cnc/debug.h"
#include <cassert>

#define SERIAL_THRESHOLD 100000

void * operator new ( size_t size ) throw ( std::bad_alloc )
{
    if ( size == 0 ) size = 1;
    if ( void * ptr = scalable_malloc( size ) )
        return ptr;
    throw std::bad_alloc();
}


void * operator new[] ( size_t size ) throw ( std::bad_alloc )
{
    return operator new ( size );
}

void* operator new ( size_t size , const std::nothrow_t & ) throw ()
{
    if ( size == 0 ) size = 1;
    if ( void* ptr = scalable_malloc( size ) )
        return ptr;
    return NULL;
}

void * operator new[] ( size_t size , const std::nothrow_t & ) throw ( )
{
    return operator new ( size , std::nothrow );
}

void operator delete ( void * ptr ) throw()
{
    if( ptr != 0 ) scalable_free ( ptr ) ;
} 

void operator delete[] ( void * ptr ) throw()
{
    operator delete ( ptr );
} 

void operator delete ( void * ptr , const std::nothrow_t & ) throw()
{
    if( ptr != 0 ) scalable_free ( ptr ) ;
} 

void operator delete[] ( void * ptr , const std::nothrow_t & ) throw()
{
    operator delete ( ptr , std::nothrow );
} 

namespace {
    template <typename argument_type>
    inline void swap( argument_type * a, argument_type * b )
    {
        argument_type t = * a;
        * a = * b;
        * b = t;
    }

    template <typename argument_type>
    inline void vector_dump (const argument_type * array, const size_t size )
    {
        std::cout << array[0];
        for (size_t i = 1 ; i < size ; ++i ) {
            std::cout << ", " << array[ i ];
        }
        std::cout << std::endl;
    }

    template <typename T>
    inline void vector_init ( T* array, const size_t size , size_t mod )
    {
        assert ( 0 );
    }

    template <>
    inline void vector_init< int >( int * array, const size_t size , size_t mod )
    {
        for (size_t i = 0 ; i < size ; ++i ) {
            array[ i ] = rand() % mod ;
        }
    }

    template <>
    inline void vector_init< double >( double * array, const size_t size , size_t mod )
    {
        for (size_t i = 0 ; i < size ; ++i ) {
            double tmp = rand() % mod ; 
            array[ i ] = tmp / mod ;
        }
    }

    template <typename argument_type >
    inline size_t partition( argument_type * array , size_t size , bool verbose )
    {
        size_t middleIndex = size / 2;
        swap( & array[ 0 ], & array[ middleIndex ] );
        
        argument_type pivot = array[ 0 ];
    
        int i = 0;
        int j = size;
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
            //} while ( !compareFunctor ( array[ j ] , pivot ) ); 

            if (verbose) {
                std::cout << "i(" << i << ") j(" << j << ")" << std::endl;
            }

            do {
                assert( i <= j );
                if ( i == j ) goto done;
                ++i;
            }
            while ( array[ i ] < pivot );
            //while ( compareFunctor ( array[ i ] , pivot ) && array[ i ] != pivot );

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

    // these two functions are from the tstream code on the svn
    template < typename argument_type >
    inline void serial_quicksort_helper(argument_type * array, size_t size, bool verbose )
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

template <typename vector_member_type >
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

        void print() const
        {
            vector_dump( m_array , m_size );
        }

        template < typename comparator_type >
        bool isSorted() const
        {
            bool sortedTillNow = true;
                comparator_type comparator;
            if( m_size > 1 ) {
                size_t size = m_size;
                argument_type last( m_array[ 0 ] );

                for ( size_t i = 1 ; i < size && sortedTillNow ; ++i ) {
                    sortedTillNow = sortedTillNow &&  comparator ( last , m_array[ i ] ); //m_array[ i ] >= last;
                    last = m_array[ i ];
                }
            }
            return sortedTillNow ;
        }

        enum { LEFT = 1 , RIGHT = 2 };
    protected:
        typedef vector_member_type argument_type;
        argument_type * m_array;
        size_t m_size;
        bool m_verbose;
};

template <class vector_member_type >
class my_vector_type : public vector_base < vector_member_type > {
    typedef vector_base < vector_member_type > base_type;
    typedef typename base_type::argument_type argument_type;
    public:
        my_vector_type( ) 
            : base_type ( )
        {
            m_array_to_copy_from = this->m_array;
        }

        void print() const
        {
            vector_dump( m_array_to_copy_from , this->m_size );
        }
        void serial_quicksort()
        {
            serial_quicksort_helper( m_array_to_copy_from , this->m_size , this->m_verbose );
        }

        my_vector_type( size_t size , bool verbose ) 
            : base_type ( size , verbose )
        {
            m_array_to_copy_from = this->m_array = new argument_type[ size ];
            vector_init<argument_type> ( this->m_array , size , size * 100 );
        }

        ~my_vector_type()
        {
            delete [] this->m_array;
        }

        void filter( int childIndex )
        {
            size_t pivotIndex = partition( this->m_array , this->m_size, this->m_verbose );
            switch (childIndex) {
                case base_type::LEFT:
                    this->m_size = pivotIndex; 
                    break;
                case base_type::RIGHT: 
                    this->m_size -= ( 1 + pivotIndex ); 
                    m_array_to_copy_from += ( 1 + pivotIndex );
                    break;
            };
        }

        argument_type operator[]( size_t index ) const
        {
            return this->m_array[ index ];
        }

        inline argument_type getPivot( int childIndex ) const
        {
            // may seem like a segmentation fault, is not
            int pivotIndex = 0;
                switch (childIndex) {
                    case base_type::LEFT:
                        pivotIndex = this->m_size; break;
                    case base_type::RIGHT: 
                        pivotIndex = -1; break;
                };
            return this->m_array_to_copy_from[ pivotIndex ];
        }

        my_vector_type(const my_vector_type & copied) 
            : base_type ( copied )
        {
            *this = copied;
        }

        my_vector_type& operator=(const my_vector_type & copied)
        {
            assert( this->m_array == NULL || this->m_array == copied.m_array );
            size_t size = this->m_size = copied.m_size;
            argument_type * my_array = this->m_array = m_array_to_copy_from = new argument_type[ size ];
                argument_type * copied_array = copied.m_array_to_copy_from;

            for ( size_t i = 0 ; i < size ; ++i ) {
                my_array[ i ] = copied_array[ i ];
            }
            return *this;
        }

        my_vector_type(const my_vector_type & leftChild, argument_type pivot , const my_vector_type & rightChild) 
            : vector_base< vector_member_type >( leftChild.m_size + rightChild.m_size + 1 , rightChild.m_verbose)
        {
            this->m_array = m_array_to_copy_from = new argument_type[ this->m_size ];
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
     private:
          argument_type * m_array_to_copy_from;
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
struct quick_sort_directed_split_helper {
    typedef quick_sort_context < vector_member_type > templated_context_type;
    typedef typename templated_context_type::vector_type  vector_type ;

    int helper( const ancestry_path_tag_type & ancestry, int childIndex , templated_context_type & graph) const;
};

template < typename vector_member_type >
struct quick_sort_left_split_step : public quick_sort_directed_split_helper < vector_member_type > {
    typedef quick_sort_context < vector_member_type > templated_context_type;
    typedef typename templated_context_type::vector_type  vector_type ;

    int execute( const ancestry_path_tag_type & ancestry, templated_context_type & graph) const;
};

template < typename vector_member_type >
struct quick_sort_right_split_step : public quick_sort_directed_split_helper < vector_member_type > {
    typedef quick_sort_context < vector_member_type > templated_context_type;
    typedef typename templated_context_type::vector_type  vector_type ;

    int execute( const ancestry_path_tag_type & ancestry, templated_context_type & graph) const;
};

template < typename vector_member_type >
struct quick_sort_concat_step {
    typedef quick_sort_context < vector_member_type > templated_context_type;
    typedef typename templated_context_type::vector_type vector_type;

    int execute( const ancestry_path_tag_type & ancestry, templated_context_type & graph) const;
};


template< int GC >
struct gc_tuner : public CnC::hashmap_tuner
{
    int get_count( const ancestry_path_tag_type & ) const
    {
        return GC;
    }
};

template < typename vector_member_type >
struct quick_sort_context: public CnC::context< quick_sort_context < vector_member_type > > {

    typedef my_vector_type < vector_member_type > vector_type ;

    CnC::step_collection< quick_sort_left_split_step< vector_member_type > >  m_leftSplitSteps;
    CnC::step_collection< quick_sort_right_split_step< vector_member_type > > m_rightSplitSteps;
    CnC::step_collection< quick_sort_concat_step< vector_member_type >  >     m_concatSteps;

    CnC::tag_collection< ancestry_path_tag_type >                                                ancestryPathSplitTagSpace;
    CnC::tag_collection< ancestry_path_tag_type, CnC::preserve_tuner< ancestry_path_tag_type > > ancestryPathConcatTagSpace;

    CnC::item_collection< ancestry_path_tag_type, vector_type, gc_tuner< 2 > >        unsortedVectorSpace;
    CnC::item_collection< ancestry_path_tag_type, vector_type, gc_tuner< 1 > >        sortedVectorSpace;
    CnC::item_collection< ancestry_path_tag_type, vector_member_type, gc_tuner< 1 > > pivotSpace;

    quick_sort_context < vector_member_type > ()
        : CnC::context< quick_sort_context < vector_member_type > >(),
          m_leftSplitSteps( *this ), 
          m_rightSplitSteps( *this ), 
          m_concatSteps( *this ), 
          ancestryPathSplitTagSpace ( *this ), 
          ancestryPathConcatTagSpace ( *this ), // preserve tags
          sortedVectorSpace ( *this ), 
          pivotSpace ( *this ), 
          unsortedVectorSpace ( *this ) 
    {
        ancestryPathSplitTagSpace.prescribes( m_leftSplitSteps, *this );
        ancestryPathSplitTagSpace.prescribes( m_rightSplitSteps, *this );
        ancestryPathConcatTagSpace.prescribes( m_concatSteps, *this );
    }
};

template < typename vector_member_type >
int quick_sort_directed_split_helper< vector_member_type > ::helper(const ancestry_path_tag_type & ancestry , int childIndex, templated_context_type & graph) const
{
    vector_type currentVector;
    graph.unsortedVectorSpace.get( ancestry, currentVector );
    currentVector.filter ( childIndex );
    graph.pivotSpace.put ( ancestry , currentVector.getPivot(childIndex ) );

    size_t currentVectorSize = currentVector.size();
    ancestry_path_tag_type childTag = computeChildTag( ancestry, childIndex );

    if ( currentVectorSize <= SERIAL_THRESHOLD ) {
        currentVector.serial_quicksort();
        graph.sortedVectorSpace.put( childTag, currentVector );
        if ( ancestry > 0 ) { // concat parent, may be pushed twice but no problem
            graph.ancestryPathConcatTagSpace.put( ancestry ); 
        }
    } else {

        graph.unsortedVectorSpace.put( childTag , currentVector ); //only the child part will be copied
        graph.ancestryPathSplitTagSpace.put( childTag );
    }

    return CnC::CNC_Success;
}

template < typename vector_member_type >
int quick_sort_left_split_step < vector_member_type > ::execute(const ancestry_path_tag_type & ancestry , templated_context_type & graph) const
{
    return this->helper(ancestry, vector_type::LEFT , graph) ;
}

template < typename vector_member_type >
int quick_sort_right_split_step < vector_member_type > ::execute(const ancestry_path_tag_type & ancestry , templated_context_type & graph) const
{
    return this->helper(ancestry, vector_type::RIGHT , graph) ;
}

template < typename vector_member_type >
int quick_sort_concat_step < vector_member_type >::execute( const ancestry_path_tag_type & ancestry, templated_context_type & graph) const
{
    vector_type rightChildVector;
    graph.sortedVectorSpace.get( computeChildTag( ancestry , vector_type::RIGHT ), rightChildVector );
    vector_type leftChildVector;
    graph.sortedVectorSpace.get( computeChildTag( ancestry , vector_type::LEFT ), leftChildVector );
    vector_member_type pivot;
    graph.pivotSpace.get( ancestry, pivot );

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

        //quick_sort_context < double , std::greater< double > > graph;
        quick_sort_context < double > graph;
    
#if 0
    CnC::debug::trace( graph.sortedVectorSpace , "sortedVectorSpace " );
    CnC::debug::trace( graph.unsortedVectorSpace , "unsortedVectorSpace " );
    CnC::debug::trace( graph.pivotSpace , "pivotSpace" );

    CnC::debug::trace( graph.ancestryPathSplitTagSpace, "ancestryPathSplitTagSpace" );
    CnC::debug::trace( graph.ancestryPathConcatTagSpace, "ancestryPathConcatTagSpace" );
#endif

        graph.unsortedVectorSpace.put( ancestry_path_tag_type ( 0 ) , integerVector ); // 0 is the root tag
        graph.ancestryPathSplitTagSpace.put( 0 ); 

        // default template parameters are problematic for now, so go with the default less than or equal to 
        //my_vector_type < double , std::greater< double > > sorted ( graph.sortedVectorSpace.get( 0 ) ) ; // 0 is the root tag
        my_vector_type < double > sorted; // 0 is the root tag
        graph.sortedVectorSpace.get( 0, sorted );
        graph.wait();
        tbb::tick_count t1 = tbb::tick_count::now();
        // execution finished 
        assert ( sorted.size() == integerVector.size() );
//          sorted.print();
        assert ( sorted.isSorted< std::less_equal<double> >() );

        printf("Computed in %g seconds\n", (t1-t0).seconds());
    }
    return toBeReturned;
}
