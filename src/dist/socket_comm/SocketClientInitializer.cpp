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
  see SocketClientInitializer.h
*/

#include "SocketClientInitializer.h"
#include "SocketChannelInterface.h"
#include "Settings.h"
#include "itac_internal.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <iostream>
#include <cnc/internal/cnc_stddef.h>

#define HERE __FILE__, __LINE__

namespace CnC
{
    namespace Internal
    {
        SocketClientInitializer::SocketClientInitializer( SocketChannelInterface & obj, const char hostContactStr[] )
        :
            m_channel( obj ),
            m_hostContactStr( hostContactStr )
        {}

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        SocketClientInitializer::~SocketClientInitializer()
        {}

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketClientInitializer::init_socket_comm()
        {
            VT_FUNC_I( "Dist::Socket::init_socket_comm" );

            UINT32 nBytes;
            PAL_SockRes_t ret;

             // Initialize socket layer (must be done only once ...)
            PAL_SockInit( HERE );

            // Create socket connection with the host.
            // Resulting communicators are put into m_channel.m_socketCommData[0].
            // Socket connections to the other clients will be set up
            // later (in build_client_connections).

            PAL_Socket_t newSocket;

             // Predefined client id?
            int myClientId = 0;
            const char* cncSocketClientId = getenv( "CNC_SOCKET_CLIENT_ID" );
                        // envvar to be set from the client starter script,
                        // don't make it a config setting!
            if ( cncSocketClientId ) {
                myClientId = atoi( cncSocketClientId );
                CNC_ASSERT( myClientId >= 1 );
            }

             // Create first socket connection to the host:
            ret = PAL_Connect( HERE, m_hostContactStr, -1, &newSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );
            int clientIdArr[2];
            clientIdArr[0] = ( ! cncSocketClientId ) ? 0 : 1; // defines 1st stage AND whether client id was set on the client
            clientIdArr[1] = myClientId;
            ret = PAL_Send( HERE, newSocket, clientIdArr, 2 * sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( clientIdArr ) );
            const int myClientIdOrig = myClientId;
            int arr[2];
            ret = PAL_Recv( HERE, newSocket, arr, 2 * sizeof( int ), &nBytes, -1, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( arr ) );
            myClientId  = arr[0];
            int numProcsTotal = arr[1];

             // Check client id:
            CNC_ASSERT( ! cncSocketClientId || myClientId == myClientIdOrig );
            CNC_ASSERT( myClientId >= 0 );

             // ==> Now the number of relevant processes is known.
             //     Prepare the data structures accordingly !!!
            m_channel.setLocalId( myClientId );
            m_channel.setNumProcs( numProcsTotal );
            m_channel.m_socketCommData[0].m_recvSocket = newSocket;

             // Create second socket connection to the host:
            ret = PAL_Connect( HERE, m_hostContactStr, -1, &m_channel.m_socketCommData[0].m_sendSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );
            clientIdArr[0] = 2; // defines 2nd stage
            clientIdArr[1] = myClientId; // must now be >= 0
            ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, clientIdArr, 2 * sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( clientIdArr ) );

             // Agree with the host on setup data like the number of worker threads.
            exchange_setup_info();

             // Setup connections between clients. Host will be coordinating this.
            build_client_connections();

             // Final step of initialization: setup of ITAC connections (VTcs):
            init_itac_comm();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// \todo why is this in comments?
        void SocketClientInitializer::exchange_setup_info()
        {
//             UINT32 nBytes;
//             PAL_SockRes_t ret;
//
//              // Agree with the host on setup data like the number of worker threads.
//              // Client suggests, host decides.
//              // First pack suggestion into a buffer:
//             serializer ser;
//             ser.set_mode_pack( m_channel.m_useCRC, true );
//             IClient::packGeneralSetupData( "SOCKET", ser );
//             BufferAccess::finalizePack( ser );
//
//              // Now send this buffer to the host:
//             ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, ser.get_header(), (UINT32)ser.get_total_size(), &nBytes, -1 );
//             CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == ser.get_total_size() );
//
//              // Receive host's response:
//             ser.set_mode_unpack( m_channel.m_useCRC, true );
//             ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_sendSocket, ser.get_header(), (UINT32)ser.get_header_size(), &nBytes, -1, false );
//             CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == ser.get_header_size() );
//             size_type bodySize = ser.unpack_header();
//             void * body = BufferAccess::acquire( ser, bodySize );
//             ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_sendSocket, body, (UINT32)bodySize, &nBytes, -1, false );
//             CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == bodySize );
//
//              // Unpack and set setup data from host's response:
//             BufferAccess::initUnpack( ser );
//             IClient::unpackAndSetGeneralSetupData( ser );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketClientInitializer::build_client_connections()
        {
            PAL_SockRes_t ret;
            UINT32 nBytes;

            int okay = 0;
            do {
                 // Receive desired mode:
                int mode;
                ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_recvSocket, &mode, sizeof( int ), &nBytes, -1, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( mode ) );

                 // Perform the mode's actions:
                switch ( mode ) {
                    case SocketChannelInterface::SETUP_SEND_CONTACT_STRING: {
                        accept_client_connections();
                        break;
                    }
                    case SocketChannelInterface::SETUP_RECV_CONTACT_STRING: {
                        connect_to_other_client();
                        break;
                    }
                    case SocketChannelInterface::SETUP_DONE: {
                        okay = 1;
                        ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, &okay, sizeof( int ), &nBytes, -1 );
                        CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( okay ) );
                        break;
                    }
                }
            } while ( ! okay );

             // Check whether all relevant sockets have been created:
            for ( int j = 0; j < (int)m_channel.m_socketCommData.size(); ++j ) {
                CNC_ASSERT( m_channel.m_socketCommData[j].m_sendSocket != PAL_INVALID_SOCKET || j == m_channel.localId() );
                CNC_ASSERT( m_channel.m_socketCommData[j].m_recvSocket != PAL_INVALID_SOCKET || j == m_channel.localId() );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketClientInitializer::accept_client_connections()
        {
            PAL_SockRes_t ret;
            UINT32 nBytes;

            int nClientsContacting;
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_recvSocket, &nClientsContacting, sizeof( int ), &nBytes, -1, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( nClientsContacting ) );
            CNC_ASSERT( nClientsContacting > 0 );
            int nConnectionsExpected = 2 * nClientsContacting;

             // Create contact string:
            std::string contactStr;
            const char* CnCSocketHostname = Internal::Settings::get_string( "CNC_SOCKET_HOSTNAME", NULL );
            UINT32 CnCSocketPort = Settings::get_int( "CNC_SOCKET_PORT", 0 );
            PAL_Socket_t initialSocket;
            ret = PAL_Listen( HERE, 0, nConnectionsExpected, CnCSocketHostname, CnCSocketPort, contactStr, &initialSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );

             // Send contact string back to host:
            int contactStrLen = (int)contactStr.size() + 1;
            ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, &contactStrLen, sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( contactStrLen ) );
            ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, contactStr.c_str(), contactStrLen, &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == (UINT32)contactStrLen );

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

             // Wait for other clients to connect to this client.
             // For each client, two connections are set up:
            for ( int iConn = 0; iConn < nConnectionsExpected; ++iConn )
            {
                 // Accept next connection.
                 // It may be the 1st or the 2nd connection to the respective client.
                PAL_Socket_t newSocket;
                int remoteClientId;
                ret = PAL_Accept( HERE, initialSocket, -1, &newSocket );
                CNC_ASSERT( ret == PAL_SOCK_OK );
                ret = PAL_Recv( HERE, newSocket, &remoteClientId, sizeof( int ), &nBytes, m_channel.m_timeout, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( remoteClientId ) );
                if ( remoteClientId < 0 ) {
                     // It is the first connection to this client.
                     // Send back the id to the client, and the total number
                     // of involved processes (host and clients):
                    remoteClientId = -remoteClientId;
                    CNC_ASSERT( remoteClientId < (int)m_channel.m_socketCommData.size() );
                    ret = PAL_Send( HERE, newSocket, &remoteClientId, sizeof( int ), &nBytes, -1 );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( remoteClientId ) );
                    CNC_ASSERT( m_channel.m_socketCommData[remoteClientId].m_sendSocket == PAL_INVALID_SOCKET );
                    m_channel.m_socketCommData[remoteClientId].m_sendSocket = newSocket;
                } else {
                     // It is the second connection to this client.
                    CNC_ASSERT( remoteClientId < (int)m_channel.m_socketCommData.size() );
                    CNC_ASSERT( m_channel.m_socketCommData[remoteClientId].m_recvSocket == PAL_INVALID_SOCKET );
                    m_channel.m_socketCommData[remoteClientId].m_recvSocket = newSocket;
// #if 1
//                      // Print info:
//                     if ( ! restarting ) {
//                         if ( nRemainingClients > 0 ) {
//                             std::cerr << "--> established socket connection " << client + 1 << ", "
//                                       << nRemainingClients << " still missing ...\n" << std::flush;
//                         } else {
//                             std::cerr << "--> established all socket connections.\n" << std::flush;
//                         }
//                     }
// #endif
                }
            }

             // Cleanup:
            SocketChannelInterface::closeSocket( initialSocket );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketClientInitializer::connect_to_other_client()
        {
            PAL_SockRes_t ret;
            UINT32 nBytes;

             // Receive the contact string and the id of the client
             // which I should connect to:
            int arr[2];
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_recvSocket, arr, 2 * sizeof( int ), &nBytes, m_channel.m_timeout, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( arr ) );
            int clientId = arr[0];
            int contactStrLen = arr[1];
            CNC_ASSERT( 0 < clientId && clientId < (int)m_channel.m_socketCommData.size() );
            CNC_ASSERT( contactStrLen > 0 );
            char* contactStr = new char[contactStrLen];
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_recvSocket, contactStr, contactStrLen, &nBytes, m_channel.m_timeout, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == (UINT32)contactStrLen );

             // Create first socket connection to the remote client:
            CNC_ASSERT( m_channel.m_socketCommData[clientId].m_recvSocket == PAL_INVALID_SOCKET );
            ret = PAL_Connect( HERE, contactStr, -1, &m_channel.m_socketCommData[clientId].m_recvSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );
            int myClientId = -m_channel.localId();
            ret = PAL_Send( HERE, m_channel.m_socketCommData[clientId].m_recvSocket, &myClientId, sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( myClientId ) );
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[clientId].m_recvSocket, &myClientId, sizeof( int ), &nBytes, -1, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( myClientId ) );
            CNC_ASSERT( myClientId == m_channel.localId() );

             // Create second socket connection to the host:
            CNC_ASSERT( m_channel.m_socketCommData[clientId].m_sendSocket == PAL_INVALID_SOCKET );
            ret = PAL_Connect( HERE, contactStr, -1, &m_channel.m_socketCommData[clientId].m_sendSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );
            ret = PAL_Send( HERE, m_channel.m_socketCommData[clientId].m_sendSocket, &myClientId, sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( myClientId ) );

             // Cleanup:
            delete [] contactStr;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketClientInitializer::init_itac_comm()
        {
#ifdef CNC_WITH_ITAC
            VT_FUNC_I( "Dist::Socket::init_itac_comm" );

            UINT32 nBytes;
            PAL_SockRes_t ret;

             // Receive client ranks (local and global):
            int itacRank[2];
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[0].m_recvSocket, itacRank, 2 * sizeof( int ), &nBytes, -1, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( itacRank ) );
            int localRank  = itacRank[0];
            int globalRank = itacRank[1];

             // Define process name in ITAC tracefile:
            char clientName[128];
            sprintf( clientName, "sClient%d", localRank );

             // Determine VTcs contact string:
            const char* vtContact;
            VT_CLIENT_INIT( globalRank, NULL, &vtContact );

             // Send VTcs contact string to host:
            int length = (int)strlen( vtContact ) + 1;
            ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, &length, sizeof( int ), &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( length ) );
            ret = PAL_Send( HERE, m_channel.m_socketCommData[0].m_sendSocket, vtContact, length, &nBytes, -1 );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == length );

             // Construct ITAC thread name:
            char thrName[64];
            sprintf( thrName, "RECV%d", m_channel.localId() );

             // Initialize VTcs:
            VT_INIT();
            VT_THREAD_NAME( thrName );

#endif // CNC_WITH_ITAC
        }

    } // namespace Internal
} // namespace CnC
