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
  see Buffer.h
*/

#include <cnc/internal/dist/Buffer.h>
#include <iostream>
#include <cnc/internal/cnc_stddef.h>
#include <cstring>

/**
 * CRC stuff from Boost:
 */
#ifdef HAVE_BOOST
#include <boost/crc.hpp>      // for boost::crc_basic, boost::crc_optimal
#include <boost/cstdint.hpp>  // for boost::uint16_t
typedef boost::crc_32_type crc_type;
#endif

namespace CnC {
    namespace Internal 
    {
        
        Buffer::Buffer( const Buffer & buf )
            : m_buf( NULL ),
              m_curr( NULL ), 
              m_body( NULL ),
              m_header( NULL ),
              m_size( buf.m_size ),
              m_addCRC( buf.m_addCRC ),
              m_addSize( buf.m_addSize ) 
        {
            m_buf = (char*)scalable_malloc( m_size );
            memcpy( m_buf, buf.m_buf, (size_t)m_size );
            m_curr = m_buf + ( buf.m_curr - buf.m_buf );
            m_body = m_buf + ( buf.m_body - buf.m_buf );
            m_header = m_buf + ( buf.m_header - buf.m_buf );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void Buffer::reset( bool addCRC, bool addSize, bool forceNonEmpty )
        {
            m_addSize = addSize;
            m_addCRC = addCRC;
            
            // Make sure that the buffer is nonempty (if requested):
            if ( forceNonEmpty ) {
                m_curr = m_body;
                acquire( 2 * MAX_HEADER_SIZE );
            }
            
            // Reset position pointers (header size may have changed!):
            if ( m_buf ) {
                m_body   = m_buf + MAX_HEADER_SIZE;
                m_header = m_body - getHeaderSize();
                m_curr   = m_body;
            } else {
                CNC_ASSERT( ! m_body && ! m_header && ! m_curr && m_size == 0 );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void * Buffer::acquire( size_type len )
        {
            const size_type oldSz = m_curr - m_header;
            ensure_capacity( oldSz + len );

            char * currTmp = m_curr;
            m_curr += len;
            return currTmp;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void Buffer::ensure_capacity( size_type newSz )
        {
            newSz += m_header - m_buf;
            if( newSz <= m_size && m_size > 0 ) return;

            // New size must be >= the header size.
            const size_type headerSize = getHeaderSize();
            const size_type oldSz = m_curr - m_body;
            
            // Determine the new buffer size:
            if ( m_size <= 0 ) { m_size = 32; }
            do {
                m_size *= 2;
            } while ( m_size < newSz );
            
            // Allocate a new buffer:
            char * oldBuf = m_buf;
            m_buf = (char*)scalable_malloc( m_size );
            if( oldBuf ) memcpy( m_buf, oldBuf, (size_t)oldSz + MAX_HEADER_SIZE);
            scalable_free( oldBuf );
            m_body = m_buf + MAX_HEADER_SIZE;
            m_header = m_body - headerSize;
            m_curr = m_body + oldSz;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        size_type Buffer::reserve( size_type n )
        {
            char * _pos = reinterpret_cast< char * >( acquire( n ) );
            return _pos - m_buf;
        }

        void * Buffer::getPos( size_type n )
        {
            return m_buf + n;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void Buffer::finalizePack()
        {
            //            memset( m_buf, -1, m_body - m_buf );
            if( m_buf == NULL ) ensure_capacity( 0 );
            // Here we assume buffer had been filled and we now want to send it away/finalize it
            char * curr = m_header;
            
            // First do body CRC if needed:
            if ( m_addCRC ) {
            CNC_ABORT( "Unexpected code path taken" );
                CNC_ASSERT( curr < m_body );
#ifdef HAVE_BOOST
                pack_crc( m_body, m_curr - m_body, curr );
#endif
                curr += CRC_SIZE;
            }
            
            // Now deal with size info:
            if ( m_addSize ) {
                size_type tmp_sz = m_curr - m_body;
                if( m_addCRC ) {
                    CNC_ASSERT( curr < m_body );
#ifdef HAVE_BOOST
                    pack_crc( reinterpret_cast< const char * >( &tmp_sz ), sizeof( tmp_sz ), curr );
#endif
                    curr += CRC_SIZE;
                }
                CNC_ASSERT( curr < m_body );
                memcpy( curr, &tmp_sz, sizeof( tmp_sz ) );
                curr += sizeof( tmp_sz );
            }

            CNC_ASSERT( m_header && m_header <= m_body && m_header >= m_buf );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void Buffer::initUnpack() // throw( SizeError, CRCError )
        {
            CNC_ASSERT( m_header && m_header <= m_body && m_header >= m_buf );
            
            // Check size:
            if ( m_addSize ) {
                size_type tmp_sz = 0;
                memcpy( &tmp_sz, m_body - sizeof( size_type ), sizeof( tmp_sz ) );
                if ( tmp_sz != (size_t)(m_curr - m_body) ) {
                    CNC_ASSERT( "inconsistent buffer size\n" == NULL );
                    throw SizeError();
                }
            }
            
            // Check CRC sum:
            if ( m_addCRC ) {
            CNC_ABORT( "Unexpected code path taken" );
#ifdef HAVE_BOOST
                check_crc( m_body, m_curr - m_body, m_header );
                // throws exception in case of error
#endif
            }
            m_curr = m_body;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        size_type Buffer::unpackSizeFromHeader() const // throw( CRCError )
        {
            size_type resSize = invalid_size;
            if ( m_header && m_addSize ) {
                char * curr = m_body - sizeof( size_type );
                memcpy( &resSize, curr, sizeof( resSize ) );
                if ( m_addCRC ) {
                    CNC_ABORT( "Unexpected code path taken" );
#ifdef HAVE_BOOST
                    check_crc( reinterpret_cast< const char * >( &resSize ), sizeof( resSize ), curr - CRC_SIZE );
                    // throws exception in case of error
#endif
                }
            }
            return resSize;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef HAVE_BOOST
        void Buffer::pack_crc( const char * buf, size_type sz, char * crc )
        {
            CNC_ASSERT( sizeof( crc_type::value_type ) == CRC_SIZE );
            crc_type comp;
            comp.process_bytes( buf, (size_t)sz );
            crc_type::value_type res = comp.checksum();
            memcpy( crc, &res, sizeof( res ) );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void Buffer::check_crc( const char * buf, size_type sz, const char * crc )
            // throw( CRCError )
        {
            crc_type comp;
            // std::cout << "--> checking CRC ...\n" << std::flush;
            comp.process_bytes( buf, (size_t)sz );
            crc_type::value_type res = comp.checksum();
            if ( memcmp( &res, crc, sizeof( res ) ) != 0 ) {
                std::cerr << "ERROR: CRC mismatch.\n";
                throw CRCError();
            } 
        }
#endif

    } // namespace Internal
} // namespace CnC





