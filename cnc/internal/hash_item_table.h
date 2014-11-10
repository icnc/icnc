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

#ifndef _CnC_HASH_ITEM_TABLE_H_
#define _CnC_HASH_ITEM_TABLE_H_

#include <tbb/concurrent_hash_map.h>
//#include <tbb/spin_mutex.h>
#include <cnc/internal/item_properties.h>
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/cnc_tag_hash_compare.h>

namespace CnC {
    namespace Internal {

        /// Stores items in a hash_table.
        /// Implements the required interface used by item_collection_base.
        /// Uses tbb::concurrent_hash_map, e.g. synchronization/race-protection is 
        /// handled through accesssors (read or read-write) and appropriate
        /// find/insert methods.
        /// \see CnC::Internal::vec_item_table
        template< typename Tag, typename ItemT, typename Coll >
        class hash_item_table
        {
        public:
            typedef ItemT item_type;
        private:
            typedef cnc_tag_hash_compare< Tag > hash_compare;
            /// the data-structure that a tag actually maps to (item and properties)
            /// no mutex required, all goes through TBB's accessors.
            struct data_t
            {
                data_t() : m_item( NULL ), m_prop() {}
                data_t( const ItemT * i ) : m_item( i ), m_prop() {}
                data_t( const ItemT * i, const item_properties & p ) : m_item( i ), m_prop( p ) {}
                ~data_t() {}
                const ItemT     * m_item;
                item_properties   m_prop; // the getcount in here is an atomic var!
            };
            // /// the internally used table type
            typedef typename tbb::concurrent_hash_map< Tag, data_t, hash_compare > map_t;
            /// template class facilitates its implementation
            template< typename parent_iterator > class t_const_iterator;

        public:
            /// returned by get_item_or_accessor, item is not always enough
            typedef std::pair< const ItemT *, bool > item_and_gc_type;

            /// accessor, if set, it protects the entry from being altered by another thread until released/freed
            class accessor;

            /// iterator for reading only
            typedef t_const_iterator< typename map_t::const_iterator > const_iterator;

            /// constructor, argument is ignored
            hash_item_table( const Coll * = NULL, size_t = 0 );

            /// dummy, NOP
            void resize( size_t sz ) {}

            /// Puts an item into table if not present and acquires the accessor/lock for it.
            /// \return true if inserted, false if already there
            bool put_item( const Tag &, const ItemT *, const int getcount, const int owner, accessor & );

            /// Returns item for given tag (for reading only).
            /// No other thread is allowed to write or delete
            /// the returned item as long the caller holds a pointer to it.
            const ItemT * get_item( const Tag &  ) const;

            /// If item is present, returns item and get-count, accessor will be empty
            /// If item not present, creates and protects the entry with the given accesssor/lock.
            /// No other thread is allowed to write or delete
            /// the returned item as long the caller holds a pointer to it.
            item_and_gc_type get_item_or_accessor( const Tag &, accessor & );

            /// If item is present, returns item and get-count
            /// If item not present, creates and protects the entry.
            /// The given accesssor locks entry; caller must unlock/release
            item_and_gc_type get_item_and_accessor( const Tag &, accessor & );

            /// return accessor for entry, assumes entry was already inserted
            /// must not be const, otherwise the compiler makes it a const_accessor
            bool get_accessor( const Tag &, accessor & );

            /// erase an entry identified by accessor.
            ItemT * erase( accessor & );

            /// \return number of (un-erased) items that have been put.
            size_t size() const;

            /// \return true if size() == 0
            bool empty() const;

            /// delete all entries in table
            /// to delete, call da.unceate()
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
            //            typedef tbb::spin_mutex mutex_type;
            map_t                 m_map;
            tbb::atomic< size_t > m_size;
            //            mutable mutex_type    m_mutex;
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        class hash_item_table< Tag, ItemT, Coll >::accessor
        {
        public:
            accessor()
                : m_acc()
            {
            }

            ~accessor()
            {
            }

            void release()
            {
                m_acc.release();
            }
            
            const ItemT * item() const
            {
                return m_acc->second.m_item;
            }

            const Tag & tag() const
            {
                return m_acc->first;
            }

            item_properties * properties()
            {
                return &m_acc->second.m_prop;
            }

        private:
            typename map_t::accessor & acc()
            {
                return m_acc;
            }
        public:
            typename map_t::accessor m_acc;
            friend class hash_item_table< Tag, ItemT, Coll >;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename ItemT, typename Coll >
        template< typename parent_iterator >
        class hash_item_table< Tag, ItemT, Coll >::t_const_iterator
        {
        public:
            typedef std::pair< Tag, const data_t * > value_type;

            t_const_iterator()
                : m_it()
            {}

            t_const_iterator( const const_iterator & i )
                : m_it( i.m_it )
            {}

            t_const_iterator( const parent_iterator & i )
                : m_it( i )
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

            const Tag & tag() const
            {
                return m_it->first;
            }
            
            const ItemT * item() const
            {
                return m_it->second.m_item;
            }

            const item_properties * properties() const
            {
                return &m_it->second.m_prop;
            }

            const_iterator & operator++()
            {
                ++m_it;
                return *this;
            }

            const_iterator operator++( int )
            {
                const_iterator _tmp = *this;
                ++(*this);
                return _tmp;
            }

        private:
            parent_iterator    m_it;
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        hash_item_table< Tag, ItemT, Coll >::hash_item_table( const Coll *, size_t )
        : m_map(),
          m_size()
          // , m_mutex()
        {
            m_size = 0;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename Tag, typename ItemT, typename Coll >
        bool hash_item_table< Tag, ItemT, Coll >::put_item( const Tag & t, const ItemT * item, const int getcount, const int owner, accessor & a )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            m_map.insert( a.acc(), t );
            if( a.item() != NULL ) return false;
            a.acc()->second.m_item = item;
            // this is a concurrency issue: in dist-mode we might receive a decrement request prior to the actual item!
            if( getcount != item_properties::NO_GET_COUNT ) {
                a.acc()->second.m_prop.set_or_increment_get_count( getcount );
            } else {
                a.acc()->second.m_prop.set_get_count( item_properties::NO_GET_COUNT );
            }
            a.acc()->second.m_prop.set_owner( owner );
            ++m_size;
            return true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        const ItemT * hash_item_table< Tag, ItemT, Coll >::get_item( const Tag & t ) const
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            typename map_t::const_accessor a;
            return m_map.find( a, t ) ? a->second.m_item : NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename hash_item_table< Tag, ItemT, Coll >::item_and_gc_type
        hash_item_table< Tag, ItemT, Coll >::get_item_and_accessor( const Tag & t, accessor & a )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            {
                if( m_map.find( a.acc(), t ) ) {
                    const ItemT * _i = a.acc()->second.m_item;
                    if( _i != NULL ) return item_and_gc_type( _i, a.acc()->second.m_prop.get_count() != item_properties::NO_GET_COUNT );
                }
            }
            if( m_map.insert( a.acc(), t ) ) {
                CNC_ASSERT( a.item() == NULL && a.properties()->get_count() == item_properties::UNSET_GET_COUNT );
            }
            const ItemT * _i = a.item();
            if( _i != NULL ) {
                int _gc = a.properties()->get_count();
                return item_and_gc_type( _i, _gc != item_properties::NO_GET_COUNT );
            }
            return item_and_gc_type( (const ItemT *)NULL, false );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename hash_item_table< Tag, ItemT, Coll >::item_and_gc_type
        hash_item_table< Tag, ItemT, Coll >::get_item_or_accessor( const Tag & t, accessor & a )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            {
                typename map_t::const_accessor ar;
                if( m_map.find( ar, t ) ) {
                    const ItemT * _i = ar->second.m_item;
                    if( _i != NULL ) return item_and_gc_type( _i, ar->second.m_prop.get_count() != item_properties::NO_GET_COUNT );
                }
            }
            if( m_map.insert( a.acc(), t ) ) {
                CNC_ASSERT( a.item() == NULL && a.properties()->get_count() == item_properties::UNSET_GET_COUNT );
            }
            const ItemT * _i = a.item();
            if( _i != NULL ) {
                int _gc = a.properties()->get_count();
                a.release();
                return item_and_gc_type( _i, _gc != item_properties::NO_GET_COUNT );
            }
            return item_and_gc_type( (const ItemT *)NULL, false );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        bool hash_item_table< Tag, ItemT, Coll >::get_accessor( const Tag & t, accessor & a )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            return m_map.find( a.acc(), t );
        }
                
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        ItemT * hash_item_table< Tag, ItemT, Coll >::erase( accessor & a )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            // we can have an entry because of a local get or a remote request!
            const ItemT * _item = a.item();
            if( _item != NULL ) {
                --m_size;
            }
            bool _tmp = m_map.erase( a.acc() );
            CNC_ASSERT( _tmp );
            return const_cast< ItemT * >( _item );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        size_t hash_item_table< Tag, ItemT, Coll >::size() const
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            return m_size;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        bool hash_item_table< Tag, ItemT, Coll >::empty() const
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            return m_map.empty();
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        template< class DA >
        void hash_item_table< Tag, ItemT, Coll >::clear( const DA & da )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            for( typename map_t::iterator i = m_map.begin(), e = m_map.end(); i != e; ++i )
                if( i->second.m_item ) da.uncreate( const_cast< ItemT * >( i->second.m_item ) );
            m_map.clear();
            m_size = 0;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename hash_item_table< Tag, ItemT, Coll >::const_iterator hash_item_table< Tag, ItemT, Coll >::begin() const
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            return const_iterator( m_map.begin() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        typename hash_item_table< Tag, ItemT, Coll >::const_iterator hash_item_table< Tag, ItemT, Coll >::end() const
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );
            return const_iterator( m_map.end() );
        }  

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< typename Tag, typename ItemT, typename Coll >
        template< typename Func, typename Arg >
        void hash_item_table< Tag, ItemT, Coll >::for_all( Func & f, Arg & arg )
        {
            //            typename mutex_type::scoped_lock _lock( m_mutex );

            for( typename map_t::iterator i = m_map.begin(), e = m_map.end(); i != e; ++i ) {
                //                if( i->second.m_item != NULL ) {
                f( i->first, i->second.m_item, &i->second.m_prop, arg );
                //                }
            }
        }
        
    } // namespace Internal
} // namespace CnC

#endif // _CnC_HASH_ITEM_TABLE_H_
