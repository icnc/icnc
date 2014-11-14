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

#ifndef _CnC_COMMUNICATOR_H_
#define _CnC_COMMUNICATOR_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/traceable.h>
#include <tbb/scalable_allocator.h>
//#include <cnc/serializer.h>
#include <cnc/internal/scalable_vector.h>

namespace CnC {

    class serializer;

    namespace Internal {

        class CNC_API msg_callback;
        typedef void (*communicator_loader_type)(msg_callback &);

        /// There might be several communication systems.
        /// Each one must derive from this and implement its interface.
        /// Communicators are supposed to reside in an extra library.
        /// The communicator library get dynamically loaded, it must provide
        /// a function extern "C" load_communicator_( msg_callback & d, tracing_mutex_type & )
        /// in which the communicator must get registered with the msg_callback
        /// by calling msg_callback.set_communicator(c).
        /// \see CnC::Internal::dist_cnc
        class communicator
        {
        public:
            virtual ~communicator() {}

            /// initialize communication infrastructure
            /// start up communication system to accept incoming messages
            /// incoming messages are handed over to the msg_callback
            /// sender/rcvr ids in this network start with min_id
            /// created processes will be identified with ids in the range [min_id, (min_id+N)]
            /// currently there is only one communicator supported
            /// \return number of sender/rcvrs in this network (N)
            virtual int init( int min_id, long flag = 0 ) = 0;

            /// shut down communication network
            virtual void fini() = 0;

            /// distributors call this to send messages
            /// send_msg must not block - message must be sent asynchroneously;
			/// hence the given serializer must be deleted by the communicator 
            virtual void send_msg( serializer *, int rcver ) = 0;

            /// distributors call this to send the same message to all other processes.
            /// bcast_msg must not block - message must be sent asynchroneously;
			/// hence the given serializer must be deleted by the communicator 
            virtual void bcast_msg( serializer * ) = 0;

            /// distributors call this to send the same message to all given processes.
            /// bcast_msg must not block - message must be sent asynchroneously;
			/// hence the given serializer must be deleted by the communicator 
            /// \return whether calling process was in the list of receivers
            virtual bool bcast_msg( serializer * ser, const int * rcvers, int nrecvrs ) = 0;

            /// return process id of calling thread/process
            virtual int myPid() = 0;

            /// return number of processes (#clients + host)
            virtual int numProcs() = 0;

            /// return true if calling thread is on a remote process
            virtual bool remote() = 0;

            /// \return true if at least one message is pending (waiting for being sent)
            virtual bool has_pending_messages() = 0;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CnC_COMMUNICATOR_H_
