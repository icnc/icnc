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

#ifndef _CNC_COMM_CHANNELINTERFACE_H_
#define _CNC_COMM_CHANNELINTERFACE_H_

#ifdef PRE_SEND_MSGS
// works only with MPI
# include <mpi.h>
typedef MPI_Request request_type;
#else
typedef int request_type;
#endif
#include <cnc/internal/tbbcompat.h>
#include <tbb/spin_mutex.h>
#include <vector>
#include <cnc/serializer.h>

namespace CnC
{
    namespace Internal
    {

        /// A lower level interface used by GenericCommunicator to free new
        /// communicators from re-inventing insfrastructure for asynchronous communication
        /// \see CnC::Internal::GenericCommunicator
        class ChannelInterface : CnC::Internal::no_copy
        {
        public:
            typedef unsigned long long size_type;

            /// buffer size that's guaranteed to be available when calling waitForAnyClient
            enum { BUFF_SIZE = (1<<23) - Buffer::MAX_HEADER_SIZE };

            virtual ~ChannelInterface() {}

            /// Low-level communication: send given bytes to the receiver
            /// @param buffer [IN] consists of header and body (contiguously)
            /// @param headerSize [IN] size of the header at the beginning of buffer
            /// @param bodySize [IN] size of the body of buffer (after the header)
            /// @param rcverLocalId [IN] local id of the receiver process (0,1,...)
            /// \return if PRE_SEND_MSGS this value is passed to wait in the requests array, ignored otherwise
            virtual request_type sendBytes( void * buffer, size_type headerSize, size_type bodySize, int rcverLocalId ) = 0;

            /// Wait for send msg to complete
            /// Used in GenericCommunicator if PRE_SEND_MSGS is used
            /// Needed for MPI
            virtual void wait( request_type * requests, int cnt ) { CNC_ABORT( "Incomplete implementation of 2-phased sending" )}

            /// Wait until a message arrives from any client and receive the full message.
            /// Return a serializer that can be used freely until the call to waitforanyclient.
            /// @param senderLocalId [OUT] id of the sender process
            /// @return the serializer, NULL if body bytes == 0
            virtual serializer * waitForAnyClient( int & senderLocalId ) = 0;

            /// Receive body of the next message from given sender process.
            /// @param body [OUT] body bytes will be written here
            /// @param headerSize [IN] expected size (in bytes) of the body
            ///                     (body is assumed to have this size)
            /// @param senderLocalId [IN] id of the sender process from which
            ///                      the message should be received
            virtual void recvBodyBytes( void * body, size_type bodySize, int senderLocalId ) = 0;

        public:
            /// Defines the total number of processes (host + clients):
            virtual void setNumProcs( int numProcs );

            /// Defines the local id of this process:
            void setLocalId( int localId ) { m_localId = localId; }

            /// Invalidate connection to given process
            /// @param localId : id of the process to be deactivated (0 [= host], 1,...)
            virtual void deactivate( int localId );

            /// Invalidates communication channels to other clients.
            /// From then on this client can only communicate with the host.
            void invalidate_client_communication_channels();

            /// Get the local id of this process (0 for the host, > 0 for the clients)
            int localId() const { return m_localId; }

            /// Get the number of processes (as set by setNumProcs)
            int numProcs() const { return (int)m_commData.size(); }

            /// Is channel with given id active? (id = 0,1,...)
            bool isActive( int id ) const { return m_commData[id].m_isActive; }

            /// Get the send mutex for communication with process of given id
            tbb::spin_mutex & getMutex( int id ) { return m_commData[id].m_sendMutex; }

        private:
            /// Comminucation channels:
            struct CommData {
                /// misc
                tbb::spin_mutex m_sendMutex;
                /// is this channel active?
                bool m_isActive;
                /// Constructor doing initialization
                CommData() : m_isActive( true ) {}
                CommData( const CommData & cd ) : m_isActive( cd.m_isActive ), m_sendMutex() {} // mutices are not copyable
                void operator=( const CommData & cd ) { m_isActive = cd.m_isActive; } // mutices are not assignable
            };

            /// General communication data
            std::vector< CommData > m_commData;

            /// my local id: host gets 0, clients 1,2,...
            int m_localId;
        };
    }
}

#endif // _CNC_COMM_CHANNELINTERFACE_H_
