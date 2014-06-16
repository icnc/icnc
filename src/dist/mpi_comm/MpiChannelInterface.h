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

#ifndef _CNC_COMM_MPI_CHANNELINTERFACE_H_
#define _CNC_COMM_MPI_CHANNELINTERFACE_H_

#include <mpi.h>
#include "../generic_comm/ChannelInterface.h"

namespace CnC
{
    namespace Internal
    {
        class MpiHostInitializer;
        class MpiClientInitializer;

        /// Implements the ChannelInterface feature needed on host and clients.
        /// Mainly takes care for data sends and receives.
        ///
        /// Initialization aspects are implemented in MpiHostInitializer and MpiClientInitializer.
        ///
        /// Most importantly it handles "listening" for incoming mesasges from any client.
        /// To allow MPI to overlap communication with message processing we use
        /// a double buffer exchange (2 serializers).
        ///
        /// We usually send only one message, by pre-allocating large enough buffers.
        /// To support larger messages, we use a special message tag to indicate that
        /// a large message will follow.
        ///
        /// In all other cases we use non-lbokcing communication.
        ///
        /// \see CnC::Internal::GenericCommunicator
        class MpiChannelInterface : public ChannelInterface
        {
            friend class MpiHostInitializer;   // initializer for the host
            friend class MpiClientInitializer; // initializer for a client
        public:
            MpiChannelInterface( bool useCRC, MPI_Comm cm );
            ~MpiChannelInterface();

            /// Implementation of ChannelInterface methods:
            virtual int sendBytes( void * buffer, size_type headerSize, size_type bodySize, int rcverLocalId );
            virtual void wait( int * requests, int cnt );
            virtual serializer * waitForAnyClient( int & senderLocalId );
            virtual void recvBodyBytes( void * body, size_type bodySize, int senderLocalId );
        private:
            serializer * m_ser1;
            serializer * m_ser2;
            MPI_Comm     m_communicator;
            MPI_Request  m_request;

            // fields for 1-msg mechanism
            enum { FIRST_MSG = 111, SECOND_MSG = 222 };
        };
    }
}

#endif // _CNC_COMM_MPI_CHANNELINTERFACE_H_
