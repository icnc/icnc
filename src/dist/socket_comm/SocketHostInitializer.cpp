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
  see SocketHostInitializer.h
*/

#include "pal_config.h" // _CRT_SECURE_NO_WARNINGS (on Windows)

#include "SocketHostInitializer.h"
#include "SocketChannelInterface.h"
#include "Settings.h"
#include "pal_util.h" // PAL_MakeQuotedPath, PAL_StartProcessInBackground
#include "itac_internal.h"

#include <cstdlib> // atoi
#include <cstdio> // popen
#include <cnc/internal/cnc_stddef.h>
#include <string>
#include <iostream>
#include <sstream>

#define HERE __FILE__, __LINE__

namespace CnC
{
    namespace Internal
    {
        SocketHostInitializer::SocketHostInitializer( SocketChannelInterface & obj )
        :
            m_channel( obj ),
            m_initialSocket( PAL_INVALID_SOCKET )
        {}

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        SocketHostInitializer::~SocketHostInitializer()
        {
            SocketChannelInterface::closeSocket( m_initialSocket );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /**
         * UTILITY for init_socket_comm:
         * calls the given script with command line option "-n" and
         * determines the number of clients from its output.
         * If an error occurs, -1 is returned.
         */
        static int determine_nclients_from_script( const char scriptName[], bool & startClientsInOrder )
        {
#ifdef _WIN32
#           define popen  _popen
#           define pclose _pclose
#endif
            int nClients = -1;
            std::string cmd = PAL_MakeQuotedPath( scriptName );
            cmd += " -n";
            FILE* myScript = popen( cmd.c_str(), "r" );
            if ( myScript ) {
                const int bufLen = 128;
                char buf[bufLen];
                char* res = fgets( buf, bufLen, myScript );
                if ( res ) {
                    CNC_ASSERT( isdigit( res[0] ) || res[0] == '+' );
                    nClients = atoi( res );
                    // Check whether the clients should be started one after the other.
                    // This may be useful if several clients are to be started under a debugge
                    // enabled if number of clients is prepended with +
                    if( res[0] == '+' ) startClientsInOrder = false;
                }
                pclose( myScript );
            }
            return nClients;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::init_socket_comm()
        {
            VT_FUNC_I( "Dist::Socket::init_socket_comm" );

             // Initialize socket layer (must be done only once ...)
            PAL_SockInit( HERE );

             // How many clients are to be expected?
             // Default is 1, may be changed via environment variable.
            int nClientsExpected = 1;
            bool startClientsInOrder = true;
            const char* cncSocketHost = Internal::Settings::get_string( "CNC_SOCKET_HOST", NULL );
            const char* cncSocketHostname = Internal::Settings::get_string( "CNC_SOCKET_HOSTNAME", NULL );
            UINT32 cncSocketPort = Settings::get_int( "CNC_SOCKET_PORT", 0 );
            if ( cncSocketHost ) {
                const char* ptr = cncSocketHost;
                while ( *ptr != '\0' && isdigit( *ptr ) ) { ++ptr; }
                bool isNumber = ( *ptr == '\0' ) ? true : false;
                if ( isNumber ) {
                    CNC_ASSERT( m_clientStartupScript.empty() );
                    nClientsExpected = atoi( cncSocketHost );
                } else {
                     // Assume that a script is given which
                     // - tells the number of expected clients (via option -n)
                     // - starts the clients
                    m_clientStartupScript = cncSocketHost;
                    nClientsExpected = determine_nclients_from_script( cncSocketHost, startClientsInOrder );
                    if ( nClientsExpected < 0 ) {
                        std::ostringstream oss;
                        oss << "*** given script does not specify the number of clients via -n option.\n"
                            << "    " << cncSocketHost << '\n' << std::flush;
                        CNC_ABORT( oss.str() );
                    }
                }
            }
            CNC_ASSERT( nClientsExpected > 0 );

             // ==> Now the number of relevant processes is known.
             //     Prepare the data structures accordingly !!!
            m_channel.setLocalId( 0 );
            m_channel.setNumProcs( 1 + nClientsExpected );

             // Listen:
            PAL_SockRes_t ret = PAL_Listen( HERE, 0, 2 * nClientsExpected, cncSocketHostname, cncSocketPort, m_contactString, &m_initialSocket );
            CNC_ASSERT( ret == PAL_SOCK_OK );

             // Start clients, print contact string:
            if ( m_clientStartupScript.empty() ) {
                std::cout << "start clients manually with DIST_CNC=SOCKETS CNC_SOCKET_CLIENT=" << m_contactString << std::endl;
            } else {
                std::cout << "starting clients via script:\n"
                          << m_clientStartupScript << " <client_id> " << m_contactString
                          << '\n' << std::flush;
            }

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            // if something went wrong (exception) up to here,
            // there is nothing we can do to recover ...

            // Specify name of host executable and working directory to
            // client starter script: via environment variables!
            if ( ! m_clientStartupScript.empty() ) {
                setClientStarterEnvironment();
            }

            // Start clients and setup socket connections to them.
            // The function aborts if a client could not be started.
            start_client_and_setup_connection( 1, nClientsExpected, startClientsInOrder );

             // Initial socket is no longer needed
            SocketChannelInterface::closeSocket( m_initialSocket );

            // Agree with the client on setup info (e.g. the number of workers).
            for ( int iClient = 1; iClient <= nClientsExpected; ++iClient ) {
                exchange_setup_info( iClient );
            }

            // Coordinate building up the inter-client socket connections:
            for ( int iClient = 1; iClient <= nClientsExpected; ++iClient ) {
                build_client_connections( iClient );
            }

            // Initalize ITAC:
            init_itac_comm();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::setClientStarterEnvironment()
        {
            // name of host executable:
            std::string hostArgs;
            std::string hostExeName = PAL_GetProgname( &hostArgs );
            if ( ! hostExeName.empty() ) {
                PAL_SetEnvVar( "CNC_HOST_EXECUTABLE", hostExeName.c_str() );
                PAL_SetEnvVar( "CNC_HOST_ARGS", hostArgs.c_str() );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::start_client_and_setup_connection(
                             int firstClient, // 1,2,...
                             int nClients,
                             bool startClientsInOrder )
        {
            if ( startClientsInOrder ) {
                int nRemainingClients = nClients;
                for ( int client = firstClient; client < firstClient + nClients; ++client ) {
                    start_client_and_setup_connection_impl( client, 1, nRemainingClients );
                    --nRemainingClients;
                }
            } else {
                 // The clients are started all at once.
                 // Thereafter, the connections to each are set up.
                start_client_and_setup_connection_impl( firstClient, nClients, nClients );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::start_client_and_setup_connection_impl(
                             int firstClient, // 1,2,...
                             int nClients,
                             int nRemainingClients )
        {
            VT_FUNC_I( "Dist::Socket::start_client_and_setup_connection_impl" );

            PAL_SockRes_t ret;
            UINT32 nBytes;

            // 1. Start all the requested clients (unless this is done manually):
            if ( ! m_clientStartupScript.empty() ) {
                int lastClient = firstClient + nClients;
                for ( int client = firstClient; client < lastClient; ++client )
                {
                    char clientIdStr[10];
                    sprintf( clientIdStr, "%d", client );
                    const char* argv[] = { clientIdStr, m_contactString.c_str(), NULL };
                    int ret = PAL_StartProcessInBackground( m_clientStartupScript.c_str(), argv );
                    if ( ret == -1 ) {
                        std::ostringstream oss;
                        oss << "*** client starter script could not be run:\n"
                            << m_clientStartupScript << " " << clientIdStr
                            << " " << m_contactString << '\n' << std::flush;
                        CNC_ABORT( oss.str() );
                    }
                }
            }

            // 2. Establish the expected connections.
            //    For each client, two connections are set up:
            const int nConnections = 2 * nClients;
            int nextClientId = firstClient - 1;
            for ( int iConn = 0; iConn < nConnections; ++iConn )
            {
                 // Accept next connection.
                 // It may be the 1st or the 2nd connection to the respective client.
                PAL_Socket_t newSocket;
                ret = PAL_Accept( HERE, m_initialSocket, -1, &newSocket );
                CNC_ASSERT( ret == PAL_SOCK_OK );
                int clientIdArr[2];
                ret = PAL_Recv( HERE, newSocket, clientIdArr, 2 * sizeof( int ), &nBytes, m_channel.m_timeout, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( clientIdArr ) );
                int mode = clientIdArr[0]; // may be 0 or 1 (-> 1st stage),
                                           //          or 2 (-> 2nd stage)
                int remoteClientId = clientIdArr[1];
                CNC_ASSERT( mode == 0 || mode == 1 || mode == 2 );
                const int numProcsTotal = (int)m_channel.m_socketCommData.size();
                if ( mode == 0 || mode == 1 ) {
                     // It is the first connection to this client.
                     // The client may have predefined its id (via CNC_SOCKET_CLIENT_ID),
                     // otherwise the id is generated here.
                     // Send back the id to the client, and the total number
                     // of involved processes (host and clients):
                    ++nextClientId;
                    if ( mode == 0 ) { // was not defined on the client
                        remoteClientId = nextClientId;
                    }
                    CNC_ASSERT(    firstClient <= remoteClientId
                            && remoteClientId < firstClient + nClients );
                    int arr[2] = { remoteClientId, numProcsTotal };
                    ret = PAL_Send( HERE, newSocket, arr, 2 * sizeof( int ), &nBytes, -1 );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( arr ) );
                    m_channel.m_socketCommData[remoteClientId].m_sendSocket = newSocket;
                } else {
                     // It is the second connection to this client.
                    CNC_ASSERT( remoteClientId < (int)m_channel.m_socketCommData.size() );
                    m_channel.m_socketCommData[remoteClientId].m_recvSocket = newSocket;
#if 1
                     // Print info:
                    --nRemainingClients;
                    if ( nRemainingClients > 0 ) {
                        std::cerr << "--> established socket connection " << remoteClientId << ", "
                                  << nRemainingClients << " still missing ...\n" << std::flush;
                    } else {
                        std::cerr << "--> established all socket connections to the host.\n" << std::flush;
                    }
#endif
                }
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::exchange_setup_info( int client ) // 1,2,3,...
        {
            // nothing special to do here
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::build_client_connections( int client )
        {
            PAL_SockRes_t ret;
            UINT32 nBytes;

             // Initial check:
            CNC_ASSERT( 0 < client && client < (int)m_channel.m_socketCommData.size() );

             // Are there any clients left which have to set up
             // a connection to the given client?
            int nClientsContacting = (int)m_channel.m_socketCommData.size() - client - 1;
            if ( nClientsContacting > 0 )
            {
                 // Progress message:
                std::cout << "--> establishing client connections to client " << client
                          << " ..." << std::flush;

                 // Ask client for its contact string.
                 // Send to it the number of remaining clients which are
                 // going to connect to it.
                int mode = SocketChannelInterface::SETUP_SEND_CONTACT_STRING;
                ret = PAL_Send( HERE, m_channel.m_socketCommData[client].m_sendSocket, &mode, sizeof( int ), &nBytes, m_channel.m_timeout );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( mode ) );
                ret = PAL_Send( HERE, m_channel.m_socketCommData[client].m_sendSocket, &nClientsContacting, sizeof( int ), &nBytes, m_channel.m_timeout );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( nClientsContacting ) );
                int contactStrLen; // including trailing zero
                ret = PAL_Recv( HERE, m_channel.m_socketCommData[client].m_recvSocket, &contactStrLen, sizeof( int ), &nBytes, m_channel.m_timeout, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( contactStrLen ) );
                char* contactStr = new char[contactStrLen];
                ret = PAL_Recv( HERE, m_channel.m_socketCommData[client].m_recvSocket, contactStr, contactStrLen, &nBytes, m_channel.m_timeout, false );
                CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == (UINT32)contactStrLen );

                 // Inform the remaining clients that they should contact
                 // the given client. Send its contact string to them.
                for ( int j = 1; j <= nClientsContacting; ++j ) {
                    CNC_ASSERT( client + j < (int)m_channel.m_socketCommData.size() );
                    int mode = SocketChannelInterface::SETUP_RECV_CONTACT_STRING;
                    ret = PAL_Send( HERE, m_channel.m_socketCommData[client+j].m_sendSocket, &mode, sizeof( int ), &nBytes, m_channel.m_timeout );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( mode ) );
                    int arr[] = { client, contactStrLen };
                    ret = PAL_Send( HERE, m_channel.m_socketCommData[client+j].m_sendSocket, arr, 2 * sizeof( int ), &nBytes, m_channel.m_timeout );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( arr ) );
                    ret = PAL_Send( HERE, m_channel.m_socketCommData[client+j].m_sendSocket, contactStr, contactStrLen, &nBytes, m_channel.m_timeout );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == (UINT32)contactStrLen );
                }

                 // Cleanup:
                delete [] contactStr;
            }

             // Send the client the info that it is done now:
            int mode = SocketChannelInterface::SETUP_DONE;
            ret = PAL_Send( HERE, m_channel.m_socketCommData[client].m_sendSocket, &mode, sizeof( int ), &nBytes, m_channel.m_timeout );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( mode ) );

             // Receive final response from the client:
            int okay = 0;
            ret = PAL_Recv( HERE, m_channel.m_socketCommData[client].m_recvSocket, &okay, sizeof( int ), &nBytes, m_channel.m_timeout, false );
            CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( okay ) );
            CNC_ASSERT( okay == 1 );

             // Finalize progress message:
            if ( nClientsContacting > 0 ) {
                std::cout << " done" << std::endl;
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketHostInitializer::init_itac_comm()
        {
#ifdef CNC_WITH_ITAC
            // NOTE: this need not be fail-safe. We do not support
            //       fail safety together with ITAC tracing.

             // Loop over all socket clients and collect their contact strings:
            PAL_SockRes_t ret;
            UINT32 nBytes;
            std::vector< std::string > vtContactStrings;
            for ( int iClient = 1; iClient < (int)m_channel.m_socketCommData.size(); ++iClient )
            {
                try {
                    // Send local + global rank to client:
                    int itacRank[2];
                    itacRank[0] = iClient; // local rank in this channel (1,2,...)
                    itacRank[1] = (int)vtContactStrings.size() + 1; // global rank
                    ret = PAL_Send( HERE, m_channel.m_socketCommData[iClient].m_sendSocket, itacRank, 2 * sizeof( int ), &nBytes, m_channel.m_timeout );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( itacRank ) );

                    // Receive contact string:
                    int length;
                    ret = PAL_Recv( HERE, m_channel.m_socketCommData[iClient].m_recvSocket, &length, sizeof( int ), &nBytes, m_channel.m_timeout, false );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == sizeof( length ) );
                    const int maxLength = 128;
                    CNC_ASSERT( length <= maxLength );
                    char contactStr[maxLength];
                    ret = PAL_Recv( HERE, m_channel.m_socketCommData[iClient].m_recvSocket, contactStr, length, &nBytes, m_channel.m_timeout, false );
                    CNC_ASSERT( ret == PAL_SOCK_OK && nBytes == length );

                    // Append contact string to given vector:
                    CNC_ASSERT( contactStr );
                    vtContactStrings.push_back( std::string( contactStr ) );
                }
                catch( ... ) {
                    std::ostringstream oss;
                    oss << "Could not properly initialize socket client " << iClient << "\n" << std::flush;
                    CNC_ABORT( oss.str() );
                }
            }

             // Initialize VTcs server with these contact strings:
            const int nClientsExpected = (int)vtContactStrings.size();
            std::vector< const char* > vtContacts( nClientsExpected + 1 );
            const char* myContact;
            VT_CLIENT_INIT( 0, NULL, &myContact );
            CNC_ASSERT( myContact );
            vtContacts[0] = myContact;
            for ( int j = 0; j < nClientsExpected; ++j ) {
                vtContacts[j + 1] = vtContactStrings[j].c_str();
            }
            VT_SERVER_INIT( NULL, nClientsExpected + 1, &vtContacts[0] );
            VT_INIT();

#endif // CNC_WITH_ITAC
        }

    } // namespace Internal
} // namespace CnC
