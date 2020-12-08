/* *******************************************************************************
 *  Copyright (c) 2007-2014, Intel Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

#if 0

#ifndef CONCURRENT_PRIORITY_QUEUE_H__
#define CONCURRENT_PRIORITY_QUEUE_H__

#include <iostream>

#include <cnc/internal/tbbcompat.h>
#include <tbb/queuing_mutex.h>
#include <atomic>
#include <tbb/concurrent_vector.h>

namespace CnC {
    namespace Internal {


        template< class Body >
        class heap_range;

        template< class T >
        class concurrent_priority_queue {
        private:
            class HeapNode;
            typedef tbb::queuing_mutex mutex_t;
            typedef int intptr_t;
            std::atomic< int > counter;
            static const int ROOT = 0;
            mutex_t heapLock;
            tbb::concurrent_vector< HeapNode > heap;
            int vector_size;

            enum tag_state{
                EMPTY = 0,
                AVAILABLE
            };
            //---------class HeapNode------------
            class HeapNode{
                typedef int intptr_t;
                intptr_t tag;
                int priority;
                typedef tbb::queuing_mutex HeapNodeMutex_t;
                HeapNodeMutex_t lock;
                T item;
            public:
                void init( const T & myItem, int myPriority, const intptr_t & my_id ){
                    item = myItem;
                    priority = myPriority;
                    tag = my_id;
                }
                HeapNode() : tag( EMPTY ){}
                friend class concurrent_priority_queue;
                friend class heap_range<T>;
            };
            //---------class HeapNode------------

        public:
            typedef T argument_type;
            friend class heap_range<T>;
            concurrent_priority_queue() : vector_size( 64 ) {
                counter = 0;
                heap.grow_to_at_least( vector_size );
            }
            ~concurrent_priority_queue();
            int size() const{
                return counter;
            }
            bool empty() const{
                return ( size() == 0 );
            }
            void push( const T & item, int priority );
            bool pop_if_present( T & res );
            bool try_pop( T & res );
            const tbb::concurrent_vector< typename concurrent_priority_queue< T >::HeapNode >* contents() ;
        private:
            void checkHeapSize( int size );
            void swap( int a, int b );
            void swap( typename HeapNode::HeapNodeMutex_t::scoped_lock *& a, typename HeapNode::HeapNodeMutex_t::scoped_lock *& b ){
                typename HeapNode::HeapNodeMutex_t::scoped_lock * va = a;
                a = b;
                b = va;
            }
            inline int leftChild( int parent ) { return ( parent << 1 ) + 1; }
            inline int rightChild( int parent ) { return ( ( parent << 1 ) + 2 ); }
            inline int getParent( int child ) { return ( ( child - 1 ) >> 1 ); }
            static inline intptr_t hash_addr( void *x ) {union {intptr_t xi;void *xp;} v;v.xp=x;return v.xi;}
        };

        //!!TODO: T == StepInstance *, how to free memory that stored elements point at ?
        template< class T >
        concurrent_priority_queue< T >::~concurrent_priority_queue(){
            //for ( int i = ROOT; i < next; i++ ){
            //    delete heap[i].item;
            //}
        }

        template< class T >
        void concurrent_priority_queue< T >::swap( int a, int b ){
            T tmp = heap[a].item;
            heap[a].item = heap[b].item;
            heap[b].item = tmp;

            int t1 = heap[a].tag;
            heap[a].tag = heap[b].tag;
            heap[b].tag = t1;

            int p1 = heap[a].priority;
            heap[a].priority = heap[b].priority;
            heap[b].priority = p1;
        }

        template< class T >
        void concurrent_priority_queue< T >::push( const T & item, int priority ){
            intptr_t my_id; my_id = hash_addr( &my_id ); // task identifier
            int child;
            {
                mutex_t::scoped_lock lock( heapLock );
                child = counter.fetch_and_add( 1 );
        
                checkHeapSize( child );
                typename HeapNode::HeapNodeMutex_t::scoped_lock nodeLock( heap[child].lock );

                heap[child].init( item, priority, my_id );
            }
            while ( child > ROOT ) {
                int parent = getParent( child );
                HeapNode & hparent = heap[parent];
                HeapNode & hchild = heap[child];
                typename HeapNode::HeapNodeMutex_t::scoped_lock parentLock( hparent.lock );
                typename HeapNode::HeapNodeMutex_t::scoped_lock childLock( hchild.lock );
                if ( hchild.tag != my_id ){
                    child = parent;
                    continue;
                }
                if ( hchild.tag == EMPTY ){
                    return;
                }
                if ( hparent.tag != AVAILABLE && hparent.tag != EMPTY ){
                    if ( parent == ROOT && hparent.priority <= hchild.priority ){
                        hchild.tag = AVAILABLE;
                        return;
                    }
                    parentLock.release();
                    childLock.release();
                    continue;
                }
                if ( hchild.priority < hparent.priority || hparent.tag == EMPTY ){
                    swap( child, parent );
                    child = parent;
                } else {
                    hchild.tag = AVAILABLE;
                    return;
                }
            }
            if ( child == ROOT ){
                typename HeapNode::HeapNodeMutex_t::scoped_lock rootLock( heap[ROOT].lock );
                if ( heap[ROOT].tag == my_id ){
                    heap[ROOT].tag = AVAILABLE;
                }
            }
        }

        template< class T >
        bool concurrent_priority_queue< T >::pop_if_present( T & res ) {
            // int and pointers have different sizes depending on platform
            // so reinterpret cast does not work. Replace with simple hack
            // needs rethinking later
            intptr_t my_id; my_id = hash_addr( &my_id ); // task identifier
            mutex_t::scoped_lock globalLock( heapLock );
            int qsize = size();
            if ( qsize == 0 ){
                return false;
            } else if ( qsize == 1 ){
                counter.fetch_and_add( -1 );
                HeapNode & hRoot = heap[ROOT];
                typename HeapNode::HeapNodeMutex_t::scoped_lock rootLock( hRoot.lock );
                res = heap[ROOT].item;
                heap[ROOT].tag = EMPTY;
                return true;
            }
            int bottom = ( counter.fetch_and_add( -1 )-1 );
            HeapNode & hRoot = heap[ROOT];
            HeapNode & hBottom = heap[bottom];
            typename HeapNode::HeapNodeMutex_t::scoped_lock rootLock( hRoot.lock );
            typename HeapNode::HeapNodeMutex_t::scoped_lock bottomLock( hBottom.lock );
            int prevSize = vector_size;
            globalLock.release();
            res = hRoot.item;

            hRoot.item = hBottom.item;
            hRoot.priority = hBottom.priority;
            hRoot.tag = AVAILABLE;
            hBottom.tag = EMPTY;
    
            bottomLock.release();

            if ( hRoot.tag == EMPTY ) {
                rootLock.release();
                return true;
            }

            int child = -1;
            int parent = ROOT;
            typename HeapNode::HeapNodeMutex_t::scoped_lock * firstLock = &rootLock;
            typename HeapNode::HeapNodeMutex_t::scoped_lock leftLock;
            typename HeapNode::HeapNodeMutex_t::scoped_lock rightLock;
            typename HeapNode::HeapNodeMutex_t::scoped_lock * secondLock = &leftLock;
            typename HeapNode::HeapNodeMutex_t::scoped_lock * thirdLock = &rightLock;

            bool go_down = true;
            while ( go_down ) {
                int left = leftChild( parent );
                int right = rightChild( parent );
                if ( right >= prevSize ){
                    mutex_t::scoped_lock glLock( heapLock );
                    checkHeapSize( right );
                    prevSize = vector_size;
                    glLock.release();
                }
                HeapNode & hleft = heap[left];
                HeapNode & hright = heap[right];

                secondLock->acquire( hleft.lock );
                thirdLock->acquire( hright.lock );
                if ( hleft.tag == EMPTY && hright.tag == EMPTY ){
                    secondLock->release();
                    thirdLock->release();
                    go_down = false;
                    break;
                }
                if ( hright.tag == EMPTY || ( hleft.tag != EMPTY && hleft.priority <= hright.priority ) ){
                    thirdLock->release();
                    child = left;
                } else {
                    secondLock->release();
                    child = right;
                    swap( secondLock, thirdLock );
                }
                const HeapNode & hp = heap[parent];
                const HeapNode & hc = heap[child];
                if ( hc.priority < hp.priority ) {
                    swap( parent, child );
                    parent = child;
                    firstLock->release();
                    swap( firstLock, secondLock );
                } else {
                    secondLock->release();
                    break;
                }
            }
            firstLock->release();
            return true;
        }
        
        template< class T >
        bool concurrent_priority_queue< T >::try_pop( T & res ) {
            return pop_if_present( res );
        }

        template< class T >
        const tbb::concurrent_vector< typename concurrent_priority_queue< T >::HeapNode >* concurrent_priority_queue< T >::contents() {
            return &heap;
        }

        template< class T >
        void concurrent_priority_queue< T >::checkHeapSize( int size ) {
            if ( size >= vector_size - 1 ){
                vector_size <<= 1;
                heap.grow_to_at_least( vector_size );
            }
        }

        template< typename Body >
        class heap_range
        {
            public:

                typedef tbb::concurrent_vector< typename concurrent_priority_queue< Body >::HeapNode > heap_range_vector;

                heap_range(const heap_range_vector & queued, const int size, const int threshold = 16)
                    : m_threshold( threshold ),
                      m_size( size ),
                      m_singleParent( false ),
                      m_indexOfOnlyChild( -1 ),
                      m_rootIndex( 0 ),
                      m_queue( queued )
                      
                {
                    if( m_size == 2 ) { 
                        m_singleParent   = true;
                        m_indexOfOnlyChild = 1;
                    }
                }

                heap_range( heap_range & rhs, tbb::split )
                    : m_threshold( rhs.m_threshold ),
                      m_size( 0 ),
                      m_singleParent( false ),
                      m_indexOfOnlyChild( -1 ),
                      m_rootIndex( 0 ),
                      m_queue( rhs.m_queue )
                { 
                    if( !rhs.m_singleParent ){

                        m_rootIndex = ( rhs.m_rootIndex << 1 )+ 1; //left subtree
                        m_size = heap_range::sizeOfLeftChild( rhs.m_size );
                        if( m_size != 2 ){
                            m_singleParent = false;
                        } else {
                            m_singleParent = true;
                            m_indexOfOnlyChild = 1;
                        }
                        rhs.m_indexOfOnlyChild = ( rhs.m_rootIndex << 1 )+ 2; //root's only child is its right one

                    } else {

                        m_rootIndex = rhs.m_indexOfOnlyChild;
                        m_singleParent= true;
                        m_indexOfOnlyChild = (m_rootIndex << 1 ) + 1; //only child is the left one
        
                        m_size = heap_range::sizeOfLeftChild( rhs.m_size-1 ) + 1;//only child's left child size + only child
                        rhs.m_indexOfOnlyChild = ( m_rootIndex << 1 )+ 2;
                    }

                    rhs.m_size -= m_size;
                    rhs.m_singleParent= true;
                }

                inline bool empty() const { return m_size == 0; }

                bool is_divisible() const 
                {
                    bool _toBeReturned = true;
                    _toBeReturned = _toBeReturned && m_size >= (size_t)m_threshold;
                    int _rightChildPrevParentIndex = m_singleParent ? m_indexOfOnlyChild : m_rootIndex ; 
                    bool _hasRightChild = (_rightChildPrevParentIndex << 1 ) + 2 < (int)m_queue.size();
                    _toBeReturned = _toBeReturned && _hasRightChild;
                    return _toBeReturned ;
                }

                inline static int sizeOfLeftChild( int givenSize ) 
                {
                    int depth = 0, tmp = givenSize;
                    while( tmp /= 2 ) ++depth;
                    int expDepth_1 = 0x1 << (depth-1); //2^(depth-1)
                    int toBeReturned = expDepth_1;
                    if( givenSize < 3 * expDepth_1 ) { //2^depth + 2^(depth-1)
                        toBeReturned += givenSize & ( expDepth_1 - 1 );
                    } else {
                        toBeReturned += expDepth_1 - 1;
                    }
                    
                    return toBeReturned;
                }

                inline Body getItemAt( size_t index ) const {
                    return m_queue[index].item;
                }

                inline size_t getSize() const {
                    return m_size;
                }

                inline size_t getRootIndex() const {
                    return m_rootIndex;
                }

                inline bool isSingleParent() const {
                    return m_singleParent;
                }

                inline int getIndexOfOnlyChild() const {
                    return m_indexOfOnlyChild;
                }

            private:
                int                      m_threshold;
                size_t                   m_size;
                bool                     m_singleParent;
                int                      m_indexOfOnlyChild;
                size_t                   m_rootIndex;
                const heap_range_vector& m_queue;
        } ;

    } //    namespace Internal
} // namespace CnC
#endif // CONCURRENT_PRIORITY_QUEUE_H__

#endif // 0
