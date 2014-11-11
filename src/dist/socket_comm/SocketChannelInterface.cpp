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
  see SocketChannelInterface.h
*/

#include "SocketChannelInterface.h"
#include "itac_internal.h"

#include <tbb/atomic.h>

#define HERE __FILE__, __LINE__

namespace CnC
{
    namespace Internal
    {
        SocketChannelInterface::SocketChannelInterface( bool useCRC, double timeout )
            : m_ser(),
              m_socketCommData(),
              m_recvSocketTmp(),
              m_timeout( timeout )
        {
            m_ser.set_mode_unpack( useCRC, true );
        }

        SocketChannelInterface::~SocketChannelInterface()
        {
             // Cleanup sockets:
            for ( int j = 0; j < (int)m_socketCommData.size(); ++j ) {
                closeSocket( m_socketCommData[j].m_recvSocket );
                closeSocket( m_socketCommData[j].m_sendSocket );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketChannelInterface::closeSocket( PAL_Socket_t & sock )
        {
            if ( sock != PAL_INVALID_SOCKET ) {
                try {
                    PAL_Close( HERE, sock );
                }
                catch ( ... ) {}
                sock = PAL_INVALID_SOCKET;
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketChannelInterface::setNumProcs( int numProcs )
        {
            // Generic layer:
            ChannelInterface::setNumProcs( numProcs );
            
            // Socket specific stuff:
            m_socketCommData.resize( numProcs );
            m_recvSocketTmp.resize( numProcs );
        }
        
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int SocketChannelInterface::sendBytes( void * data, size_type headerSize, size_type bodySize, int rcverLocalId )
        {
            VT_FUNC_I( "Dist::Socket::sendBytes" );
#ifdef WITHOUT_SENDER_THREAD
            // !!! Protect access to sendSocket by mutex !!!
            // This is not necessary if there is a separate sender thread
            // which does all sends (exclusively).
            tbb::spin_mutex::scoped_lock lock( getMutex( rcverLocalId ) );
#endif
            CNC_ASSERT( 0 <= rcverLocalId && rcverLocalId < numProcs() );
            
             // we send header and body at once:
            UINT32 nBytesSent;
            PAL_SockRes_t ret = PAL_Send( HERE, m_socketCommData[rcverLocalId].m_sendSocket,
                                          data, (UINT32)( headerSize + bodySize ),
                                          &nBytesSent, m_timeout );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytesSent == (UINT32)( headerSize + bodySize ) );
            return 0;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        serializer * SocketChannelInterface::waitForAnyClient( int & senderLocalId )
        {
            VT_FUNC_I( "Dist::Socket::waitForAnyClient" );

             // Prepare temporary socket array for use in PAL_Select:
            int nClients = (int)m_socketCommData.size();
            for ( int iClient = 0; iClient < nClients; ++iClient ) {
                m_recvSocketTmp[iClient] = ( isActive( iClient ) )
                                            ? m_socketCommData[iClient].m_recvSocket
                                            : PAL_INVALID_SOCKET;
            }

             // Wait for next message from a client:
            try {
                PAL_SockRes_t ret = PAL_Select( HERE, &m_recvSocketTmp[0], nClients, NULL, 0, m_timeout );
                CNC_ASSERT( ret == PAL_SOCK_OK );
            }
            catch( ErroneousFds errSockets ) {
                // transform PAL_Select exception to ClientId exception:
                // just send first eroneous Id, ignore all others.
                int nErrSockets = (int)errSockets.items.size();
                int errClientId = -1; // start with invalid id
                for ( int j = 0; j < nErrSockets; ++j ) {
                    int id = errSockets.items[j].second;
                    if ( isActive( id ) ) {
                        errClientId = id;
                        break;
                    }
                }
                CNC_ASSERT( errSockets.items.size() > 0 && errClientId >= 0 );
                //throw ClientId( errClientId );
            }
            catch( Timeout ) {
                // hm, all clients unavailable. Throw invalid client id (?)
                //throw ClientId( nClients );
            }
            // don't handle all other exceptions, as we do not know them, they must be weired

             // Try to be fair with picking the next message to be received:
            static tbb::atomic< int > prevClient;
            int client = prevClient;
            while (    client < nClients
                    && (    m_recvSocketTmp[client] != PAL_INVALID_SOCKET
                         || ! isActive( client ) )
                  ) ++client;
            if ( client >= nClients ) {
                client = 0;
                while (    client < nClients
                        && (    m_recvSocketTmp[client] != PAL_INVALID_SOCKET
                             || ! isActive( client ) )
                      ) ++client;
            }
            CNC_ASSERT( client < nClients );
            CNC_ASSERT( isActive( client ) );
            prevClient = ( prevClient + 1 ) % nClients;

            senderLocalId = client;

            m_ser.set_mode_unpack();
            UINT32 nBytesReceived;
            PAL_SockRes_t ret;
            ret = PAL_Recv( HERE, m_socketCommData[senderLocalId].m_recvSocket,
                            m_ser.get_header(), (UINT32)m_ser.get_header_size(), &nBytesReceived, m_timeout, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytesReceived == m_ser.get_header_size() );

            size_type _bodySize = m_ser.unpack_header(); // throws an exception in case of error
            if( _bodySize != 0 ) {
                CNC_ASSERT( _bodySize != Buffer::invalid_size );
                CNC_ASSERT( _bodySize > 0 );
                BufferAccess::acquire( m_ser, _bodySize );
                ret = PAL_Recv( HERE, m_socketCommData[senderLocalId].m_recvSocket,
                                m_ser.get_body(), (UINT32)_bodySize, &nBytesReceived, m_timeout, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytesReceived == _bodySize );
                return &m_ser;
            }
            return NULL; //termination
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketChannelInterface::recvBodyBytes( void * body, size_type bodySize, int senderLocalId )
        {
            VT_FUNC_I( "Dist::Socket::recvBodyBytes" );
            CNC_ASSERT( 0 <= senderLocalId && senderLocalId < numProcs() );

            UINT32 nBytesReceived;
            PAL_SockRes_t ret;
            ret = PAL_Recv( HERE, m_socketCommData[senderLocalId].m_recvSocket,
                            body, (UINT32)bodySize, &nBytesReceived, m_timeout, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytesReceived == bodySize );
        }

    } // namespace Internal
} // namespace CnC
