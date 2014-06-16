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

#ifndef _CNC_COMM_SOCKETHOST_H_
#define _CNC_COMM_SOCKETHOST_H_

#include "pal_socket.h"

#include <string>

namespace CnC
{
    namespace Internal
    {
        class SocketChannelInterface;
        
        /// Inits host process, clients use SocketClientInitializer.
        /// Optionally, ths host starts the clients with a provided script
        /// (CNC_SOCKET_HOST). The scrpt must work in 2 modes
        ///   If called with -n it must return the nubmer of clients to start.
        ///   Otherwise it is supposed to start clients.
        ///
        /// Afterwards it waits for all clients to become available
        /// and exchanges setup options, like #threads.
        ///
        /// Also takes care of ITAC initialization if requested.
        ///
        /// \todo docu needed, in particular about starting clients
        class SocketHostInitializer
        {
        public:
            SocketHostInitializer( SocketChannelInterface & obj );
            ~SocketHostInitializer();

            /// Start remote processes and initialize communication with them:
            void init_socket_comm();
            
        private:
            /// Initialize ITAC:
            void init_itac_comm();
        
            /// Set environment variables for client starter scripts:
            void setClientStarterEnvironment();

            /// Start client processes and setup socket connections:
            void start_client_and_setup_connection( int firstClient, int nClients );
            void start_client_and_setup_connection_impl( int firstClient, int nClients, 
                                                         int nRemainingClients );

            /// Host and client agree on setup data:
            void exchange_setup_info( int client );

            /// Coordinate building the socket connections between 
            /// the given client and all other clients with a larger id.
            void build_client_connections( int client );

        private:
            /// The communication object which is to be filled up
            SocketChannelInterface & m_channel;
            
            /// Initial socket via which the connections to the clients
            /// are established. Will also be needed for restarting a client.
            PAL_Socket_t m_initialSocket;
            
            /// Contact string needed by clients to connect to the initial host socket:
            std::string m_contactString;
            
            /// Name of startup script, stored for a possible restart
            std::string m_clientStartupScript;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CNC_COMM_SOCKETHOST_H_
