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

/*
  Implementation of CnC::item_collection
  Mostly very thin wrappers calling item_collection_base.
  The wrappers hide internal functionality from the API.
*/

#ifndef _CnC_ITEM_COLLECTION_H_
#define _CnC_ITEM_COLLECTION_H_

#include <cnc/internal/item_collection_base.h>
#include <cnc/internal/dist/distributor.h>

namespace CnC {

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// The const-iterator
    template< typename Tag, typename Item, typename Tuner >
    class item_collection< Tag, Item, Tuner >::const_iterator
    {
    public:
        typedef std::pair< Tag, const Item * > value_type;
        const_iterator() : m_coll( NULL ), m_it() {}
        const_iterator( const const_iterator & i ) : m_coll( i.m_coll ), m_it( i.m_it ), m_val( i.m_val ) {}
        const_iterator( const item_collection< Tag, Item, Tuner > * c,
                        const typename base_coll_type::const_iterator & i );
        //const_iterator operator=( const const_iterator & i ) { m_coll = i.m_coll; m_it = i.m_it; m_val = i.m_val; return *this; }
        bool operator==( const const_iterator & i ) const { return m_coll == i.m_coll && m_it == i.m_it; }
        bool operator!=( const const_iterator & i ) const { return m_coll != i.m_coll || m_it != i.m_it; }
        const value_type & operator*() const;
        const value_type * operator->() const { return &operator*(); }
        const_iterator & operator++();
        const_iterator operator++( int ) { const_iterator _tmp = *this; ++(*this); return _tmp; }
        bool valid() const { return m_coll != NULL && m_it.item() != NULL; }

    private:
        void set( const typename base_coll_type::const_iterator & i ) {
            m_val.first  = i.tag();
            m_val.second = i.item();
        }

        const item_collection< Tag, Item, Tuner > * m_coll;
        typename base_coll_type::const_iterator m_it;
        value_type m_val;
    };


    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    item_collection< Tag, Item, Tuner >::const_iterator::const_iterator( const item_collection< Tag, Item, Tuner > * c,
                                                                         const typename base_coll_type::const_iterator & i )
        : m_coll( c ),
          m_it( i ),
          m_val()
    {
        if( m_it != m_coll->m_itemCollection.end() ) set( m_it );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    const typename item_collection< Tag, Item, Tuner >::const_iterator::value_type & item_collection< Tag, Item, Tuner >::const_iterator::operator*() const
    {
        CNC_ASSERT( m_coll && m_it != m_coll->m_itemCollection.end() );
        CNC_ASSERT( m_it.item() &&  m_it.tag() == m_val.first && m_val.second == m_it.item() );
        return m_val;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    typename item_collection< Tag, Item, Tuner >::const_iterator & item_collection< Tag, Item, Tuner >::const_iterator::operator++()
    {
        CNC_ASSERT( m_coll );
        typename base_coll_type::const_iterator _e( m_coll->m_itemCollection.end() );
        if( m_coll == NULL ) return *this;
        do {
            ++m_it;
        } while( m_it != _e && m_it.item() == NULL );
        if( m_it != _e ) set( m_it );
        return *this;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    template< class Derived >
    item_collection< Tag, Item, Tuner >::item_collection( context< Derived > & context, const std::string & name )
        : m_itemCollection( reinterpret_cast< Internal::context_base & >( context ), name )
    {
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    template< class Derived >
    item_collection< Tag, Item, Tuner >::item_collection( context< Derived > & context, const std::string & name, const Tuner & tnr )
        : m_itemCollection( reinterpret_cast< Internal::context_base & >( context ), name, tnr )
    {
    } 

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    template< class Derived >
    item_collection< Tag, Item, Tuner >::item_collection( context< Derived > & context, const Tuner & tnr )
        : m_itemCollection( reinterpret_cast< Internal::context_base & >( context ), std::string(), tnr )
    {
    } 

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    item_collection< Tag, Item, Tuner >::~item_collection()
    {
        m_itemCollection.get_context().reset_distributables( true );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    void item_collection< Tag, Item, Tuner >::set_max( size_t mx )
    {
        m_itemCollection.set_max( mx );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    void item_collection< Tag, Item, Tuner >::put( const Tag & t, const Item & i )
    {
        m_itemCollection.put( t, i );
        //return 0; // FIXME error code?
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    void item_collection< Tag, Item, Tuner >::get( const Tag & t, Item & i ) const
    {
        const_cast< base_coll_type & >( m_itemCollection ).get( t, i );
    }

    template< typename Tag, typename Item, typename Tuner >
    bool item_collection< Tag, Item, Tuner >::unsafe_get( const Tag & t, Item & i ) const
    {
        return const_cast< base_coll_type & >( m_itemCollection ).unsafe_get( t, i );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    typename item_collection< Tag, Item, Tuner >::const_iterator item_collection< Tag, Item, Tuner >::begin() const
    {
        const_iterator _tmp( this, m_itemCollection.begin() );
        const_iterator _e( end() ); 
        while( _tmp != _e && ! _tmp.valid() ) ++_tmp;
        return _tmp;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    typename item_collection< Tag, Item, Tuner >::const_iterator item_collection< Tag, Item, Tuner >::end() const
    {
        return const_iterator( this, m_itemCollection.end() );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    void item_collection< Tag, Item, Tuner >::unsafe_reset()
    {
        m_itemCollection.unsafe_reset( Internal::distributor::numProcs() > 1 && m_itemCollection.get_context().subscribed() );
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    size_t item_collection< Tag, Item, Tuner >::size()
    {
        return m_itemCollection.size();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    bool item_collection< Tag, Item, Tuner >::empty()
    {
        return m_itemCollection.empty();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< typename Tag, typename Item, typename Tuner >
    void item_collection< Tag, Item, Tuner >::on_put( callback_type * cb )
    {
        m_itemCollection.on_put( cb );
    }

} // namespace CnC

#endif // _CnC_ITEM_COLLECTION_H_
