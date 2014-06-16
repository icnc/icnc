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


#ifndef _CNC_BUFFER_H_
#define _CNC_BUFFER_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/scalable_object.h>
#include <cstdlib>

//#include <CnC/internal/declarations.h> // size_type
typedef unsigned long long size_type;

#include <exception>

namespace CnC {
    namespace Internal {

        /// The buffer used by the serializer to store serialized data.
        /// \todo documentation needed
        /// \see CnC::Internal::dist_cnc
        class CNC_API Buffer : public scalable_object
        {
        public:
            static const int CRC_SIZE = 4;
            static const int MAX_HEADER_SIZE = sizeof( size_type ) + 2 * CRC_SIZE;
            static const size_type invalid_size = static_cast< size_type >( -1 );

            /// Exceptions:
            struct Error : public std::exception {};
            struct SizeError : public Error {
                virtual const char * what () const throw() { return "CnC::Internal::Buffer::SizeError"; }
                ~SizeError() throw() {}
            };
            struct CRCError : public Error {
                virtual const char * what () const throw() { return "CnC::Internal::Buffer::CRCError"; }
                ~CRCError() throw() {}
            };

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            /// Construction:
            Buffer( bool addCRC = false, bool addSize = false )
                : m_buf( NULL ), m_curr( NULL ), m_body( NULL ), m_header( NULL ),
                  m_size( 0 ),
                  m_addCRC( addCRC ), m_addSize( addSize ) {}

            Buffer( const Buffer & buf );
            ~Buffer() { scalable_free( m_buf ); }

            /// resets curr pointer, header etc,
            void reset( bool addCRC, bool addSize, bool forceNonEmpty );
            void reset( bool forceNonEmpty ) { reset( m_addCRC, m_addSize, forceNonEmpty ); }

            /// make sure at least n many *additional* bytes can be stored in buffer
            /// current position is moved forward
            /// return previous current position or NULL if failed
            void * acquire( size_type n );

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            /// same as acquire, but returns offset to beginning of buffer
            /// data is stored at a later point in time with a call to getPos()
            /// don't use this unless you know what you are doing
            size_type reserve( size_type n );

            /// get buffer position that correpsonds to what was retrieved through reserve
            /// don't use this unless you know what you are doing
            void * getPos( size_type n );

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            /// return capacity
            size_type capacity() const { return m_size - ( m_header - m_buf ); }

            void ensure_capacity( size_type n );

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            /// \return start of msg header
            void * getHeader() { return m_header; }

            /// \return start of msg body
            void * getBody() const { return m_body; }

            /// \return start of useful data of buffer
            void * getAll() { return getHeader(); }

            /// \return current position in Buffer
            void * getCurr() const { return m_curr; }

            /// \return size of msg header in bytes
            size_type getHeaderSize() const {
                const int crcSize = ( m_addCRC ) ? CRC_SIZE : 0;
                return ( m_addSize )
                       ? crcSize + crcSize + sizeof( size_type )
                       : crcSize;
            }

            /// \return size of msg body in bytes
            size_type getBodySize() const {
                return m_curr - m_body;
            }

            /// \return size of entire msg in bytes
            size_type getAllSize() const {
                return getHeaderSize() + getBodySize();
            }

            /// Check internal consistency:
            bool verifyIntegrity() const { return ( m_curr <= m_buf + m_size ); }

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            // resets curr pointer, header etc, but keeps memory, addSize and CRC setting
            /// \return pointer to header
            void finalizePack();

            /// requires that header has been received
            /// unpacks size from header and checks its CRC
            ///
            /// \return size of body or Buffer::invalid_size if no header is available
            size_type unpackSizeFromHeader() const; // throw( CRCError )

            /// check whether body CRC and/or size are consistent.
            /// Throws exception if not.
            void initUnpack(); // throw( SizeError, CRCError );

        private:
#ifdef HAVE_BOOST
            /// CRC routines. Throw an exception in case of error.
            static void pack_crc( const char * buf, size_type sz, char * crc );
            static void check_crc( const char * buf, size_type sz, const char * crc ); // throw( CRCError );
#endif
        private:
            char     * m_buf;
            char     * m_curr;
            char     * m_body;
            char     * m_header;
            size_type  m_size;

            bool       m_addCRC;
            bool       m_addSize;
        };

    } // namespace Internal
} // namespace CnC

#endif  /* _CNC_BUFFER_H_ */
