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

#ifndef _CNC_COMM_SOCKETCHANNELINTERFACE_H_
#define _CNC_COMM_SOCKETCHANNELINTERFACE_H_

#include "../generic_comm/ChannelInterface.h"
#include <vector>
#include "pal_socket.h" // PAL_Socket_t

namespace CnC
{
    namespace Internal
    {
        class SocketHostInitializer;
        class SocketClientInitializer;

        /// Implements the ChannelInterface feature needed on host and clients.
        /// Mainly takes care for data sends and receives.
        ///
        /// Initialization aspects are implemented in SocketHostInitializer and SocketClientInitializer.
        ///
        /// Most importantly it handles "listening" for incoming mesasges from any client.
        ///
        /// In all other cases we use non-lbokcing communication.
        ///
        /// \see CnC::Internal::GenericCommunicator
        /// \todo docu needed
        class SocketChannelInterface : public ChannelInterface
        {
            friend class SocketHostInitializer;   // initializer for the host
            friend class SocketClientInitializer; // initializer for a client
        public:
            SocketChannelInterface( bool useCRC, double timeout );
            ~SocketChannelInterface();

            /// Implementation of ChannelInterface methods:
            virtual request_type sendBytes( void * buffer, size_type headerSize, size_type bodySize, int rcverLocalId );
            virtual serializer * waitForAnyClient( int & senderLocalId );
            virtual void recvBodyBytes( void * body, size_type bodySize, int senderLocalId );

            /// Defines the total number of processes (host + clients):
            virtual void setNumProcs( int numProcs );

        public:
            /// Mode constants for setup of connections between clients
            enum SocketSetupProtocol {
                SETUP_SEND_CONTACT_STRING,
                SETUP_RECV_CONTACT_STRING,
                SETUP_DONE
            };

            /// Gracefully close the given socket.
            /// Resets the socket handle to PAL_INVALID_SOCKET.
            static void closeSocket( PAL_Socket_t & sock );

        private:
            /// Comminucation channels:
            struct SocketCommData {
                /// socket for sending messages from here to this remote process
                PAL_Socket_t m_sendSocket;
                /// socket for receving messages from remote process
                PAL_Socket_t m_recvSocket;
                /// Constructor doing initialization
                SocketCommData() : m_sendSocket( PAL_INVALID_SOCKET ),
                                   m_recvSocket( PAL_INVALID_SOCKET ) {}
            };

            serializer m_ser;

            /// General communication data
            std::vector< SocketCommData > m_socketCommData;

            /// Temporary array of sockets, needed in receive
            std::vector< PAL_Socket_t > m_recvSocketTmp;

            /// (global) timeout
            double m_timeout;
        };
    }
}

#endif // _CNC_COMM_SOCKETCHANNELINTERFACE_H_
