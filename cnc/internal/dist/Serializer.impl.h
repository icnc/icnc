/* *******************************************************************************
 *  Copyright (c) 2007-2021, Intel Corporation
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
  Implementation of marshalling features.
  FIXME: documentation needed.
*/

#ifndef _CNC_SERIALIZER_IMPL_H_
#define _CNC_SERIALIZER_IMPL_H_

#include <cnc/serializer.h>
#include <cnc/internal/dist/Buffer.h>
#include <cnc/internal/cnc_stddef.h>
#include <string.h>  // memcpy
#include <algorithm> // swap

namespace CnC {

    template< class T, class Allocator >
    serializer::construct_array< T, Allocator >::construct_array( T *& arrVar, size_type num )
    {
        typedef typename Allocator::template rebind< T >::other TAllocator;
        TAllocator a;
        arrVar = a.allocate( static_cast< typename TAllocator::size_type >( num ) );
        T t;
        for( size_type i = 0; i < num; ++i ) {
            a.construct( &arrVar[i], t );
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T, class Allocator >
    serializer::destruct_array< T, Allocator >::destruct_array( T *& arrVar, size_type num )
    {
        if( arrVar ) {
            typedef typename Allocator::template rebind< T >::other TAllocator;
            TAllocator a;
            for( size_type i = 0; i < num; ++i ) {
                a.destroy( &arrVar[i] );
            }
            a.deallocate( arrVar, static_cast< typename TAllocator::size_type >( num ) );
        }
        arrVar = NULL;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T >
    serializer& serializer::operator&( T & var )
    {
        switch ( m_mode ) {
            case MODE_PACKED_SIZE:
                m_packedSize += packed_size( &var, 1 );
                break;
            case MODE_PACK:
                pack( &var, 1 );
                break;
            case MODE_UNPACK:
                unpack( &var, 1 );
                break;
            case MODE_CLEANUP:
                cleanup( &var, 1 );
                break;
        }
        return *this;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    serializer& serializer::operator&( const T & var )
    {
        CNC_ASSERT( ( ! is_unpacking() && ! is_cleaning_up() )
                || "cannot unpack into a const object; maybe use an explicit cast: const_cast< T & >( yourObj )" == NULL );
        return operator&( const_cast< T & >( var ) );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T, class Allocator >
    serializer& serializer::operator&( chunk< T, Allocator > arr )
    {
        switch ( m_mode ) {
            case MODE_PACKED_SIZE:
                m_packedSize += packed_size( &arr.m_len, 1 );
                m_packedSize += packed_size( arr.m_arrVar, arr.m_len );
                break;
            case MODE_PACK:
                pack( &arr.m_len, 1 );
                pack( arr.m_arrVar, arr.m_len );
                break;
            case MODE_UNPACK:
                size_type arrLen;
                unpack( &arrLen, 1 );
                CNC_ASSERT( arrLen >= 0 );
                //CNC_ASSERT( arr.m_arrVar == 0 );
                CNC_ASSERT( arr.m_len == arrLen );
                construct_array< T, Allocator >( arr.m_arrVar, arrLen );
                unpack( arr.m_arrVar, arrLen );
                break;
            case MODE_CLEANUP:
                cleanup( arr.m_arrVar, arr.m_len );
                destruct_array< T, Allocator >( arr.m_arrVar, arr.m_len );
                break;
        }
        return *this;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    size_type serializer::packed_size( const T * arr, size_type len, bitwise_serializable ) const
    {
        return len * sizeof( T );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::pack( const T * arr, size_type len, bitwise_serializable cat )
    {
        CNC_ASSERT( m_mode == MODE_PACK );
        size_t tmp = static_cast<size_t>( len ) * sizeof( T );
        void * buf = m_buf->acquire( tmp );
        memcpy( buf, arr, tmp );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::unpack( T * arr, size_type len, bitwise_serializable )
    {
        CNC_ASSERT( m_mode == MODE_UNPACK );
        size_t tmp = static_cast<size_t>( len ) * sizeof( T );
        void * buf = m_buf->acquire( tmp );
        memcpy( arr, buf, tmp );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::cleanup( T * arr, size_type len, bitwise_serializable )
    {}

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    size_type serializer::packed_size( const T * objArr, size_type len, explicitly_serializable ) const
    {
        serializer tmpBuf( false );
        tmpBuf.set_mode_packed_size();
        for ( size_type j = 0; j < len; ++j ) {
            serialize( tmpBuf, const_cast< T& >( objArr[j] ) );
        }
        return tmpBuf.m_packedSize;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::pack( const T * objArr, size_type len, explicitly_serializable cat )
    {
        CNC_ASSERT( m_mode == MODE_PACK );
        for ( size_type j = 0; j < len; ++j ) {
            serialize( *this, const_cast< T& >( objArr[j] ) );
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::unpack( T * objArr, size_type len, explicitly_serializable )
    {
        CNC_ASSERT( m_mode == MODE_UNPACK );
        for ( size_type j = 0; j < len; ++j ) {
            serialize( *this, objArr[j] );
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    void serializer::cleanup( T * objArr, size_type len, explicitly_serializable )
    {
        //CNC_ASSERT( m_mode == MODE_CLEANUP );
        for ( size_type j = 0; j < len; ++j ) {
            serialize( *this, objArr[j] );
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    class serializer::reserved
    {
    public:
        reserved() : m_pos( (size_type)-1 ), m_sz ((size_type)-1 ) {}
    private:
        reserved( size_type p, size_type s ) : m_pos( p ), m_sz ( s ) {}
        size_type   m_pos;
        size_type   m_sz;
        friend class serializer;
    };

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class T > inline
    serializer::reserved serializer::reserve( const T & obj )
    {
        CNC_ASSERT( m_mode == MODE_PACK );
        size_t _tmp = packed_size( &obj, 1, serializer_category( &obj ) );
        size_t _pos = m_buf->reserve( _tmp );
        return reserved( _pos, _tmp );
    }
    
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    namespace {
        template< class T > inline
        void cpy( Internal::Buffer * buf, size_t pos, size_t sz,  const T & obj, bitwise_serializable )
        {
            void * _buf =  buf->getPos( pos );
            memcpy( _buf, &obj, sz );
        }
    }

    template< class T > inline
    void serializer::complete( const serializer::reserved & r, const T & obj )
    {
        CNC_ASSERT( m_mode == MODE_PACK );
        CNC_ASSERT( packed_size( &obj, 1, serializer_category( &obj ) ) == r.m_sz );
        cpy( m_buf, r.m_pos, r.m_sz, obj, serializer_category( &obj ) );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    serializer::serializer( bool addCRC, bool addSize )
        : m_buf( new Internal::Buffer( addCRC, addSize ) ),
          m_packedSize( 0 ),
          m_mode( MODE_PACKED_SIZE )
    {}

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    serializer::serializer( const serializer & ser )
        : m_buf( NULL ),
          m_packedSize( ser.m_packedSize ),
          m_mode( ser.m_mode )
    {
        if( ser.m_buf != NULL ) m_buf = new Internal::Buffer( *ser.m_buf );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    serializer::~serializer()
    {
        delete m_buf;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    size_type serializer::get_header_size() const
    {
        return m_buf->getHeaderSize();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    size_type serializer::get_body_size() const
    {
        return m_buf->getBodySize();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    size_type serializer::get_total_size() const
    {
        return m_buf->getAllSize();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void * serializer::get_header() const
    {
        return m_buf->getHeader();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void * serializer::get_body() const
    {
        return m_buf->getBody();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    size_type serializer::unpack_header() const
    {
        return m_buf->unpackSizeFromHeader();
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_packed_size()
    {
        m_packedSize = 0;
        m_mode = MODE_PACKED_SIZE;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_pack()
    {
        m_buf->reset( false );
        m_mode = MODE_PACK;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_pack( bool addCRC, bool addSize )
    {
        m_buf->reset( addCRC, addSize, false );
        m_mode = MODE_PACK;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_unpack()
    {
        m_buf->reset( true );
        m_mode = MODE_UNPACK;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_unpack( bool addCRC, bool addSize )
    {
        m_buf->reset( addCRC, addSize, true );
        m_mode = MODE_UNPACK;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void serializer::set_mode_cleanup()
    {
        m_mode = MODE_CLEANUP;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    inline void swap( serializer & s1, serializer & s2 )
    {
        std::swap( s1.m_buf,          s2.m_buf );
        std::swap( s1.m_mode,         s2.m_mode );
        std::swap( s1.m_packedSize,   s2.m_packedSize );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    //%%%%%%%%%%%%%%%%%
    //                %
    //  BufferAccess  %
    //                %
    //%%%%%%%%%%%%%%%%%

    namespace Internal
    {
        class BufferAccess
        {
        public:
            static void * acquire( serializer & ser, size_type n ) {
                return ser.m_buf->acquire( n );
            }
            static size_type capacity( const serializer & ser ) {
                return ser.m_buf->capacity();
            }
            static void ensure_capacity( serializer & ser, size_type n ) {
                ser.m_buf->ensure_capacity( n );
            }
            static void finalizePack( serializer & ser ) {
                CNC_ASSERT( ser.is_packing() );
                ser.m_buf->finalizePack();
            }
            static void initUnpack( serializer & ser ) {
                CNC_ASSERT( ser.is_unpacking() );
                ser.m_buf->initUnpack();
            }
        };

    } // namespace Internal

} // namespace CnC

#include <memory>
namespace CnC {
    template< class T, class A >
    void serialize( CnC::serializer & ser, std::vector< T, A > & obj )
    {
        typename std::vector< T, A >::size_type _sz = obj.size();
        ser & _sz;
        if( ser.is_unpacking() ) obj.resize( _sz );
        T * _tmp( &obj[0] );
        ser & CnC::chunk< T, CnC::no_alloc >( _tmp, _sz );
        CNC_ASSERT( _tmp == &obj[0] );
    }

    inline void serialize( CnC::serializer & ser, std::string & str )
    {
        std::string::size_type _sz = str.size()+1;
        ser & _sz;
        if( ser.is_unpacking() ) {
            char * _s = new char[_sz];
            ser & CnC::chunk< char, CnC::no_alloc >( _s, _sz );
            str = _s;
            delete [] _s;
        } else {
            char * _s = const_cast< char * >( str.c_str() );
            ser & CnC::chunk< char, CnC::no_alloc >( _s, _sz );
        }
    }

    inline void serialize( CnC::serializer & ser, const char * & str )
    {
        if( ser.is_unpacking() ) {
            int _sz = 0;
            ser & _sz;
            str = new char[_sz];
            char * _s = const_cast< char * >( str );
            ser & CnC::chunk< char, CnC::no_alloc >( _s, _sz );
        } else if( ! ser.is_cleaning_up() ) {
            size_t _sz = strlen( str ) + 1;
            ser & _sz;
            char * _s = const_cast< char * >( str );
            ser & CnC::chunk< char, CnC::no_alloc >( _s, _sz );
        } else {
            delete [] const_cast< char * >( str );
        }
    }

// this is a hack to make the use of shared_ptr more convenient
#if defined( _SHARED_PTR_H ) || defined( _TR1_SHARED_PTR_H ) || ( defined( __GXX_EXPERIMENTAL_CXX0X__ ) && __GXX_EXPERIMENTAL_CXX0X__ == 1 ) || ( defined( _HAS_CPP0X ) && _HAS_CPP0X == 1 )
    template< typename T >
    void serialize( CnC::serializer & ser, std::shared_ptr< const T > & ptr )
    {
        T * _tmp = const_cast< T * >( ptr.get() );
        if( ser.is_unpacking() ) {
            CNC_ASSERT( _tmp == NULL );
            _tmp = new T;
        }
        if( ! ser.is_cleaning_up() ) ser & CnC::chunk< T, CnC::no_alloc >( _tmp, 1 );
        if( ser.is_unpacking() ) {
            CNC_ASSERT( ptr.get() == NULL );
            ptr.reset( _tmp );
        }
    }

    template< typename T >
    void serialize( CnC::serializer & ser, std::shared_ptr< T > & ptr )
    {
        T * _tmp = ptr.get();
        if( ser.is_unpacking() ) {
            CNC_ASSERT( _tmp == NULL );
            _tmp = new T;
        }
        if( ! ser.is_cleaning_up() ) ser & CnC::chunk< T, CnC::no_alloc >( _tmp, 1 );
        if( ser.is_unpacking() ) {
            CNC_ASSERT( ptr.get() == NULL );
            ptr.reset( _tmp );
        }
    }
#endif
}

#if (defined(_GLIBCXX_ARRAY) && _GLIBCXX_ARRAY != 0) || defined(_WIN32)
#include <array>
namespace CnC {
    template< class T, size_t N >
    inline void serialize( CnC::serializer & ser, std::array< const T, N > & a ) {
        T * _tmp = const_cast< T * >( a.data() );
        ser & CnC::chunk< T, CnC::no_alloc >( _tmp, N );
    }

    template< class T, size_t N >
    inline void serialize( CnC::serializer & ser, std::array< T, N > & a ) {
        T * _tmp = a.data();
        ser & CnC::chunk< T, CnC::no_alloc >( _tmp, N );
    }
}
#endif

#endif // _CNC_SERIALIZER_IMPL_H_
