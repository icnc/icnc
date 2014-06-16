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

#ifndef _CNC_COMM_SOCKETCLIENTINITIALIZER_H_
#define _CNC_COMM_SOCKETCLIENTINITIALIZER_H_

namespace CnC
{
    class serializer;

    namespace Internal
    {
        class SocketChannelInterface;

        /// Inits client processes, host use SocketHostInitializer.
        /// Connects to host, receives config and inits ITAC if needed.
        /// \see CnC::Internal::GenericCommunicator
        class SocketClientInitializer
        {
        public:
            SocketClientInitializer( SocketChannelInterface & obj, const char hostContactStr[] );
            ~SocketClientInitializer();

            void init_socket_comm();

        private:
            void exchange_setup_info();
            void build_client_connections();
            void init_itac_comm();

        private:
            /// Auxiliaries for "build_client_connections":
            void accept_client_connections();
            void connect_to_other_client();

        private:
            /// The communication object which is to be filled up
            SocketChannelInterface & m_channel;

            /// the host's contact string
            const char * m_hostContactStr;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CNC_COMM_SOCKETCLIENTINITIALIZER_H_
