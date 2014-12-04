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

#ifndef _CnC_VEC_ITEM_TABLE_H_
#define _CnC_VEC_ITEM_TABLE_H_

#include <cnc/internal/tbbcompat.h>
#include <cnc/internal/item_properties.h>
#include <tbb/spin_mutex.h>
#include <tbb/atomic.h>
#include <cnc/internal/scalable_vector.h>
#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    namespace Internal {

        /// Stores items in a scalable_vector. Implements the required interface to be used by item_collection_base.
        /// Uses TBBs spin lock to protect entries (within its accessor).
        /// The item is stored in a atomic variable. When inserting (put_item) it first updates the 
        /// getcount (also atomic) and then the item. This allows get_item to work without a lock/accessor:
        /// The presence of getcount and item are guaranteed when the item pointer is != NULL.
        /// Concurrent reads are non-critical, they only access getcount and item. Concurrent writes to
        /// other properties are safe because we always hold the lock/accessor when returning from put_item.
        /// \see CnC::Internal::hash_item_table
        template< typename Tag, typename ItemT, typename Coll >
        class vec_item_table : CnC::Internal::no_copy
        {
        private:
            typedef ItemT item_type;
            typedef tbb::spin_mutex   mutex_t;
            /// the data-structure that a tag actually maps to (item and properties).
            /// we need an explicit lock/mutex for every entry, which is used by the accessor.
            struct data_t
            {
                data_t() : m_item(), m_prop( ), m_mutex() { m_item = NULL;}
                data_t( ItemT * i, const item_properties & p ) : m_item(), m_prop( p ), m_mutex() { m_item = i;}
                data_t( const data_t & dt ) : m_item( dt.m_item ), m_prop( dt.m_prop ), m_mutex() {} // mutices are not copyable
                ~data_t() { erase(); }
                void operator=( const data_t & dt ) { m_item = dt.m_item; m_prop = dt.m_prop; } // mutices are not assignable
                void erase()
                {
                    m_item = NULL;
                }
                tbb::atomic< const ItemT * > m_item; /// atomic var allows lock-free access in certain cases
                item_properties              m_prop; /// the getcount in here is an atomic var!
                mutex_t                      m_mutex;
            private:
            };

            /// the internally used table type
            typedef scalable_vector_p< data_t > map_t; // GRR not a pointer, but a good work-around to make gcc4.6 play nicely
            /// template class facilitates its implementation
            template< typename parent_iterator > class t_const_iterator;

        public:
            /// returned by get_item_or_accessor, item is not always enough
            typedef std::pair< const ItemT *, bool > item_and_gc_type;

            /// accessor, if set, it protects the entry from being altered by another thread until released/freed
            class accessor;

            /// iterator for reading only
            typedef t_const_iterator< typename map_t::const_iterator > const_iterator;

            /// constructor, if sz != NULL, reserves that many entries in table.
            vec_item_table( const Coll * = NULL, size_t sz = 0 );

            /// if constructed without explicit table-size, table needs to be sized before its first use.
            void resize( size_t sz );

            /// Puts an item into table if not present and acquires the accessor/lock for it.
            /// \return true if inserted, false if already there
            bool put_item( const Tag &, const ItemT *, const int getcount, const int owner, accessor & );

            /// Returns item for given tag (for reading only).
            /// No other thread is allowed to write or delete
            /// the returned item as long the caller holds a pointer to it.
            const ItemT * get_item( const Tag &  ) const;

            /// If item is present, returns item and get-count, accessor will be empty.
            /// If item not present, creates and protects the entry with the given accesssor/lock.
            /// Assumes that the getcount is always updated before the item (in get_item).
            /// No other thread is allowed to write or delete
            /// the returned item as long the caller holds a pointer to it.
            item_and_gc_type get_item_or_accessor( const Tag &, accessor & );

            /// If item is present, returns item and get-count
            /// If item not present, creates and protects the entry.
            /// The given accesssor locks entry; caller must unlock/release
            item_and_gc_type get_item_and_accessor( const Tag &, accessor & );

            /// return accessor for entry, assumes entry was already inserted
            bool get_accessor( const Tag &, accessor & );

            /// erase an entry identified by accessor.
            ItemT * erase( accessor & );

            /// \return number of (un-erased) items that have been put.
            size_t size() const;

            /// \return true if size() == 0
            bool empty() const;

            /// delete all entries in table
            /// to delete, call da.uncreate()
            template< class DA >
            void clear( const DA & da );

            /// first entry
            const_iterator begin() const;

            /// end of iteration space, returned iterator must be dereferenced.
            const_iterator end() const;

            /// execute given functor with const_iterator and given Arg for each inserted element.
            /// concurrent insertion/deletion might happen
            template< typename Func, typename Arg >
            void for_all( Func & f, Arg & a );

        private:
            map_t                 m_map;
            tbb::atomic< size_t > m_size;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        class vec_item_table< Tag, ItemT, Coll >::accessor
        {
        public:
            accessor()
                : m_data( NULL ),
                  m_lock()
            {
            }

            void release()
            {
                m_lock.release();
                m_data = NULL;
            }
            
            const ItemT * item() const
            {
                return m_data->m_item;
            }

            item_properties * properties()
            {
                return &m_data->m_prop;
            }
            
        private:
            void acquire( data_t * d )
            {
                m_lock.acquire( d->m_mutex );
                m_data = d;
            }

            ItemT * erase()
            {
                CNC_ASSERT( m_data );
                const ItemT * _exists = m_data->m_item;
                m_data->erase();
                return const_cast< ItemT * >( _exists );
            }

            data_t               * m_data;
            mutex_t::scoped_lock   m_lock; /// dextructor will release potentially hold mutex
            friend class vec_item_table< Tag, ItemT, Coll >;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        // templates facilitate implementation
        template< typename Tag, typename ItemT, typename Coll >
        template< typename parent_iterator >
        class vec_item_table< Tag, ItemT, Coll >::t_const_iterator
        {
        public:
            typedef std::pair< Tag, const ItemT * > value_type;

            t_const_iterator()
	      : m_it(), m_begin(), m_val( 0, (const ItemT *)NULL )
            {}

            t_const_iterator( const const_iterator & i )
                : m_it( i.m_it ), m_begin( i.m_begin ), m_val( i.m_val )
            {}

            t_const_iterator( const parent_iterator & i, const parent_iterator & b )
	      : m_it( i ), m_begin( b ), m_val( 0, (const ItemT *)NULL )
            {
            }
            
            bool operator==( const const_iterator & i ) const
            {
                return m_it == i.m_it;
            }

            bool operator!=( const const_iterator & i ) const
            {
                return m_it != i.m_it;
            }

            const value_type & operator*() const
            {
                m_val = value_type( &(*m_it) - &(*m_begin), (*m_it).m_item );
                return m_val;
            }

            const Tag tag() const
            {
                return &(*m_it) - &(*m_begin);
            }
            
            const ItemT * item() const
            {
                return (*m_it).m_item;
            }

            const item_properties * properties() const
            {
                return &(*m_it).m_prop;
            }

            const_iterator operator++( int )
            {
                const_iterator _tmp = *this;
                ++(*this);
                return _tmp;
            }

            const_iterator & operator++()
            {
                ++m_it;
                return *this;
            }

        private:
            parent_iterator    m_it;
            parent_iterator    m_begin;
            mutable value_type m_val;
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        vec_item_table< Tag, ItemT, Coll >::vec_item_table( const Coll *, size_t sz )
            : m_map( sz ),
              m_size()
        {
            m_size = 0;
        }
                
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        void vec_item_table< Tag, ItemT, Coll >::resize( size_t sz )
        {
            CNC_ASSERT( m_map.size() == 0 );
            m_map.resize( sz );
        }
                
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename ItemT, typename Coll >
        bool vec_item_table< Tag, ItemT, Coll >::put_item( const Tag & t, const ItemT * item, const int getcount, const int owner, accessor & a )
        {
            CNC_ASSERT( t >= 0 && static_cast< typename map_t::size_type >( t ) < m_map.size() );
            data_t * _data = &m_map[t];
            a.acquire( _data );
            if( _data->m_item == NULL ) {
                // first update getcount
                // this is a concurrency issue: in dist-mode we might receive a decrement request prior to the actual item!
                if( getcount != item_properties::NO_GET_COUNT ) {
                    _data->m_prop.set_or_increment_get_count( getcount );
                } else {
                    _data->m_prop.set_get_count( item_properties::NO_GET_COUNT );
                }
                _data->m_prop.set_owner( owner );
                // update item after getcount to guarantee that a get_item does not return getcount and item before
                // both are actually updated (get_count only checks item).
                if( _data->m_item.compare_and_swap( item, NULL ) != NULL ) return false;
                m_size++;
                return true;
            } // else
            CNC_ASSERT( _data->m_prop.owner() == owner );
            return false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        const ItemT * vec_item_table< Tag, ItemT, Coll >::get_item( const Tag & t ) const
        {
            return m_map[t].m_item;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename vec_item_table< Tag, ItemT, Coll >::item_and_gc_type vec_item_table< Tag, ItemT, Coll >::get_item_or_accessor( const Tag & t, accessor & a )
        {
            // in a valid program nobody will alter the entry if a get is pending, so we don't fear a race with "erase"
            data_t * _data = &m_map[t];
            // getcount must have been updated before item! (see put_item).
            if( _data->m_item != NULL ) return item_and_gc_type( _data->m_item, _data->m_prop.get_count() != item_properties::NO_GET_COUNT );
            a.acquire( _data );
            // did someone put the item in the meanwhile?
            if( _data->m_item != NULL ) {
                a.release();
                return item_and_gc_type( _data->m_item, _data->m_prop.get_count() != item_properties::NO_GET_COUNT );
            }
            // ok, item is unavailable -> return empty, but pretected (accessor) entry
            return item_and_gc_type( (const ItemT *)NULL, false );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename vec_item_table< Tag, ItemT, Coll >::item_and_gc_type vec_item_table< Tag, ItemT, Coll >::get_item_and_accessor( const Tag & t, accessor & a )
        {
            // in a valid program nobody will alter the entry if a get is pending, so we don't fear a race with "erase"
            data_t * _data = &m_map[t];
            a.acquire( _data );
            // getcount must have been updated before item! (see put_item).
            if( _data->m_item != NULL ) return item_and_gc_type( _data->m_item, _data->m_prop.get_count() != item_properties::NO_GET_COUNT );
            // ok, item is unavailable -> return empty, but pretected (accessor) entry
            return item_and_gc_type( (const ItemT *)NULL, false );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        bool vec_item_table< Tag, ItemT, Coll >::get_accessor( const Tag & t, accessor & a )
        {
            data_t * _data =  &m_map[t];
            a.acquire( _data );
            if( _data->m_item != NULL || _data->m_prop.has_content() ) return true;
            a.release();
            return false;
        }
                
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        ItemT * vec_item_table< Tag, ItemT, Coll >::erase( accessor & a )
        {
            ItemT * _exists = a.erase();
            if( _exists != NULL ) --m_size;
            return _exists;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        size_t vec_item_table< Tag, ItemT, Coll >::size() const
        {
            return m_size;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        bool vec_item_table< Tag, ItemT, Coll >::empty() const
        {
            return m_size == 0;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        template< class DA >
        void vec_item_table< Tag, ItemT, Coll >::clear( const DA & da )
        {
            for( typename map_t::iterator i = m_map.begin(); i != m_map.end(); ++i ) {
                const ItemT * item = (*i).m_item;
                da.uncreate( const_cast< ItemT * >( item ) );
                i->erase();
            }
            m_size = 0;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename vec_item_table< Tag, ItemT, Coll >::const_iterator vec_item_table< Tag, ItemT, Coll >::begin() const
        {
            return const_iterator( m_map.begin(), m_map.begin() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename vec_item_table< Tag, ItemT, Coll >::const_iterator vec_item_table< Tag, ItemT, Coll >::end() const
        {
            return const_iterator( m_map.end(), m_map.begin() );
        }        

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        template< typename Func, typename Arg >
        void vec_item_table< Tag, ItemT, Coll >::for_all( Func & f, Arg & arg )
        {
            for( unsigned int i = 0; i != m_map.size(); ++i ) {
                data_t * _data = &m_map[i];
                // getcount must have been updated before item! (see put_item).
                //                if( _data->m_item != NULL ) {
                accessor ac; 
                ac.acquire( _data );
                f( i, ac.item(), ac.properties(), arg );
                //                }
            }
        }

    } // namespace Internal
} // namespace CnC

#endif // _CnC_VEC_ITEM_TABLE_H_
