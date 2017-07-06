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

#ifndef _CnC_DISTRIBUTOR_H_
#define _CnC_DISTRIBUTOR_H_

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    // Workaround for overzealous compiler warnings 
    #pragma warning (push)
    #pragma warning (disable: 4251 4275)
#endif

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/msg_callback.h>
#include <cnc/internal/dist/communicator.h>
#include <cnc/internal/dist/factory.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/atomic.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_queue.h>


namespace CnC {
    
    class serializer;

    namespace Internal {

        class communicator;
        class distributable_context;

        /// A clone of this sits on every process. The actual object is a static member of the class.
        /// It takes care of distributing distributable_contexts and exchanging their communication.
        /// It also provides information about the distributable status (on/off) of the application.
        /// The actual communication is done by a communicator, also a static member of distributor.
        /// \see CnC::Internal::dist_cnc
        /// FIXME: combining different communicators is possible, but not implemented.
        class CNC_API distributor : public msg_callback
        {
        public:

            distributor();

            /// register a context
            /// currently limited to be called on the host only
            /// might clone object on other processes
            /// returns global id of context
            static int distribute( distributable_context *  );

            /// mark given context as deleted
            /// currently limited to be called on the host only
            /// clones on other processes will be deleted
            static void undistribute( distributable_context * );

            /// distributable_contexts send their messages through the global distributor
            /// Non-blocking call; the given serializer will be deleted automatically.
            static void send_msg( serializer *, int rcver );

            /// distributable_contexts send their messages through the global distributor
            /// Non-blocking call; the given serializer will be deleted automatically.
            static void bcast_msg( serializer * );

            /// distributable_contexts send their messages through the global distributor
            /// Non-blocking call; the given serializer will be deleted automatically.
            /// \return whether calling process was in the list of receivers
            static bool bcast_msg( serializer * serlzr, const int * rcvers, int nrecvrs );
            
            /// The communicator calls this for incoming messages.
            /// Ignore the second argument unless you implement a single-process communicator.
            /// The serializer is freed by the communicator.
            virtual void recv_msg( serializer *, int pid = 0 );
            virtual void set_communicator( communicator * c ) { m_communicator = c; }

            /// distributable_contexts must get a serializer through the distributor.
            /// If the returned serializer is not handed to send_msg or bcast_msg
            /// it must be deleted with deleted by the caller.
            static serializer * new_serializer( const distributable_context * );

            /// return process id of calling trhead/process
            static int myPid() { return active() ? theDistributor->m_communicator->myPid() : 0; } // FIXME more than one communicator

            /// return number of processes (#clients + host)
            static int numProcs() { return active() ? theDistributor->m_communicator->numProcs() : 1; } // FIXME more than one communicator

            /// return true if running on a remote process
            static bool remote() { return active() ? theDistributor->m_communicator->remote() : false; } // FIXME more than one communicator

            /// return true if access from a distributed environment is requested/allowed
            static bool distributed_env() { return active() ? theDistributor->theDistributor->m_distEnv : false; }

            /// initialization must be called in a safe state and only by master thread
            static void init( communicator_loader_type loader, bool dist_env = false );
            /// finalization must be called in a safe state and only by master thread
            static void fini();

            /// start distributed system
            static void start( long flag = 0 );

            /// stop distributed system
            static void stop();

            static bool active() { return theDistributor && theDistributor->m_state >= DIST_INITING; }
            static bool initing() { return active() && theDistributor->m_state == DIST_INITING; }

            /// \return true if at lesat one message is pending (waiting for being sent)
            static bool has_pending_messages() { return theDistributor && theDistributor->m_communicator->has_pending_messages(); }

            /// \brief flush all communication channels
            ///
            /// returns number of messages received since last flush or reset
            /// will not return until all channels have been flushed
            static int flush();

            /// reset received message count
            static int reset_recvd_msg_count() { return active() && theDistributor->m_communicator ? theDistributor->m_nMsgsRecvd.fetch_and_store( 0 ) : 0; }

            /// Use this with care. Only useful for distributed and envs when you know what you're doing.
            static void unsafe_barrier() { if( active() && theDistributor->m_communicator ) theDistributor->m_communicator->unsafe_barrier(); }

        private:
            typedef enum {
                DIST_OFF,
                DIST_SUSPENDED,
                DIST_INITING,
                DIST_ON
            } state_type;
            enum { SELF = -1 };
            typedef tbb::concurrent_hash_map< int, distributable_context * > my_map;

            my_map               m_distContexts[2]; // use first entry only, only single-process communicators need the other
            tbb::atomic< int >   m_nextGId;
            state_type           m_state;
            tbb::concurrent_bounded_queue< int > m_sync; // simulates conditional variable, using char corrupts the stack on windows
            int                  m_flushCount;
            tbb::atomic< int >   m_nMsgsRecvd;
            bool                 m_distEnv;
            static distributor  * theDistributor;
            static communicator * m_communicator;

            friend struct dist_init;
        };

    } // namespace Internal
} // namespace CnC

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
    #pragma warning (pop)
#endif // warnings 4251 4275 are back

#endif // _CnC_DISTRIBUTOR_H_
