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

#ifndef _CNC_COMM_GENERICCOMMUNICATOR_H_
#define _CNC_COMM_GENERICCOMMUNICATOR_H_

#include <cnc/internal/dist/communicator.h> // implements this interface
#include <vector>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_queue.h>

namespace CnC
{
    class serializer;

    namespace Internal
    {
        class ChannelInterface;

        /// A class that implements a generic infrastructure for communicators.
        /// As sending and receiving messages must be non-blocking, some of the
        /// necessary mechanics for communicators are not trivial. This class 
        /// does some of the more complicated stuff for you.
        /// MPI and SOCKETS implementations use this.
        ///
        /// This infrastrucutre is structured as follows:
        ///
        /// GenericCommunicator : implements the top-level CnC communicator interface
        /// 
        /// ChannelInterface    : base class which comprises the channel specific
        ///                       low-level functionality (send, recv, etc.)
        ///                       
        /// GenericCommunicator has an attribute pointer m_channel
        /// which has to provide the channel specific functionality.
        /// It must be allocated accordingly by derived classes of
        /// GenericCommunicator.
        ///     
        /// Send and receive in GenericCommunicator may be done in
        /// separate threads. Their logic is implemented by the inner
        /// classes GenericCommunicator::SendThread
        /// resp. GenericCommunicator::RecvThread and is provided by
        /// the attributes m_sendThread resp. m_recvThread of
        /// GeneralCommunicator. These objects use the above channel
        /// object m_channel for handling the low-level communication
        /// aspects.  In this way, they don't know the channel
        /// specific details; they need only the ChannelInterface. The
        /// communication logic and the send/recv event loops are
        /// implemented by m_sendThread resp. m_recvThread.
        /// 
        /// ThreadExecuter      : [implementational detail]
        ///                       provides an interface for a thread class.
        ///                       The thread function must be provided by implementing
        ///                       its "runEventLoop" virtual function in a derived class.
        /// 
        /// Settings            : [implementational detail]
        ///                       provides access to config variables
        class GenericCommunicator : public communicator
        {
        public:
            GenericCommunicator( msg_callback & distr, bool de = false );
            virtual ~GenericCommunicator() {}

            /// The flag argument is an option argument.
            /// It is not used here, it is specific to the actual communicator.
            /// For example, the MPI communicator interprets it as a MPI_Comm.
            virtual int init( int minId, long flag = 0 );
            virtual void fini();

            virtual int myPid();
            virtual int numProcs();
            virtual bool remote();

            virtual void send_msg( serializer * ser, int rcver );
            virtual void bcast_msg( serializer * ser );
            virtual bool bcast_msg( serializer * ser, const int * rcvers, int nrecvrs );

            virtual bool has_pending_messages();

            bool isDistributed() { return m_distEnv; }

        private:
            /// Callback function which will be called from the receiver 
            /// thread's event loop at the end of receiving a message.
            /// @param localSenderId [IN] local id of the sender (0,1,...)
            void recv_msg_callback( serializer & ser, int localSenderId );

            /// Sends a termination request to the given remote process.
            /// The local id of the receiver (host: 0, clients: 1,2,3,...)
            /// is expected. If given "eait" parameter is true, this operation
            /// is blocking: it does not return before the request has been
            /// sent to the remote side.
            void send_termination_request( int rcverLocalId, bool isBlocking = false );

        public:
            /// CRC checksum to be used for serializers?
            static bool use_crc() { return m_useCRC; }

        protected:
            /// ==> 1st central attribute
            /// provides the low-level communication interface
            /// Will be allocated by derived classes according to 
            /// the concrete channel (socket, Lrb, etc.)
            ChannelInterface * m_channel;

            msg_callback & m_callback;

            /// ==> 2nd central attribute
            /// Handles to send and recv threads.
            /// (The recv thread on the clients will be NULL, because
            /// the main thread performs the recv loop there.)
            class SendThread;
            class RecvThread;
            SendThread * m_sendThread;
            RecvThread * m_recvThread;

            /// my global id is m_localId + m_globalIdShift
            int m_globalIdShift;

            /// misc
            bool m_hasBeenInitialized;
            bool m_exit0CallOk;
            bool m_distEnv;
            // use CRC? - No!
            static const bool m_useCRC = false;
        };
    }
}

#endif // _CNC_COMM_GENERICCOMMUNICATOR_H_
