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
  see GenericCommunicator.h
*/

#include "ChannelInterface.h"
#include <../socket_comm/pal_socket.h>
#include "GenericCommunicator.h"
#include "ThreadExecuter.h"
#include "itac_internal.h"

#include <cnc/serializer.h>
#include <cnc/internal/dist/msg_callback.h>

#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_queue.h>

#include <cstring> // memcmp

#include <iostream>

/**
 * YIELD:
 */
#ifdef _WIN32
# include <windows.h>
static inline void YIELD() { Sleep( 0 ); }
#elif __APPLE__
#include <sched.h>
static inline void YIELD() { sched_yield(); }
#else
# include <pthread.h>
static inline void YIELD() { pthread_yield(); }
#endif

/**
 * Different compilation modes:
 */
//#define WITHOUT_SENDER_THREAD
//#define WITHOUT_SENDER_THREAD_TRACING

// Correction:
#ifdef WITHOUT_SENDER_THREAD
# define WITHOUT_SENDER_THREAD_TRACING
#endif

namespace CnC
{
    namespace Internal
    {
        // Tags for ITAC tracing:
        enum ITC_Tags{
            ITC_TAG_EXTERNAL,
            ITC_TAG_INTERNAL
        };

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        //%%%%%%%%%%%%%%%
        //              %
        //  RecvThread  %
        //              %
        //%%%%%%%%%%%%%%%

        class GenericCommunicator::RecvThread : public ThreadExecuter
        {
        public:
            RecvThread( GenericCommunicator & instance )
                : m_instance( instance ), m_channel( *instance.m_channel ) {}

            /// ThreadExecuter's event loop:
            virtual void runEventLoop();

        private:
            /// Event loop for the host:
            void runRecvEventLoopHost();

            /// Event loop for the clients:
            void runRecvEventLoopClients();

            /// Receive the next message from any remote process.
            bool recv_msg( int & sender );

        private:
            /// A RecvThread belongs to a specific GenericCommunicator
            /// instance which it knows
            GenericCommunicator & m_instance;
            ChannelInterface & m_channel; // the channel of m_instance
            serializer m_ser1, m_ser2;
        };

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::RecvThread::runEventLoop()
        {
            if ( m_instance.remote() ) {
                runRecvEventLoopClients();
            } else {
                runRecvEventLoopHost();
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::RecvThread::runRecvEventLoopHost()
        {
            // Host expects one termination request from each client.
            // Clients expect two termination requests from the host.
            int nTerminationRequestsExpected = m_channel.numProcs() - 1;
            int nTerminationRequests = 0;
            bool done = false;
            do {
                int client;
                if(  ! recv_msg( client )  ) {
                    // normal message
                } else {
                    // wasTerminationRequest
                    ++nTerminationRequests;
                    if ( nTerminationRequests == nTerminationRequestsExpected ) {
                        done = true;
                    }
                }
            } while ( ! done );

            // All clients are shutting down. Send final termination requests:
            for ( int iClient = 1; iClient < m_channel.numProcs(); ++iClient ) {
                m_instance.send_termination_request( iClient, true ); // blocking send !!!
                // The communication with this client is over. Disable the channel:
                m_channel.deactivate( iClient );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::RecvThread::runRecvEventLoopClients()
        {
            // Host expects one termination request from each client.
            // Clients expect two termination requests from the host.
            int nTerminationRequestsExpected = 2;
            int nTerminationRequests = 0;
            bool done = false;
            do {
                int sender;
                if( ! recv_msg( sender ) ) {
                    // normal message
                } else {
                    // wasTerminationRequest
                    CNC_ASSERT( sender == 0 ); // termination request from host
                    ++nTerminationRequests;
                    if ( nTerminationRequests == 1 ) {
                        m_instance.send_termination_request( 0 ); // send back to the host
                    }
                    if ( nTerminationRequests == nTerminationRequestsExpected ) {
                        done = true;
                    }
                    // For the rest of this run, the clients can only communicate
                    // with the host. Disable all other client connections !!!
                    m_channel.invalidate_client_communication_channels();
                }
            } while ( ! done );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        //        const int term_bits[4] = { 0, 0, 0, 0 };

        bool GenericCommunicator::RecvThread::recv_msg( int & sender )
        {
            VT_FUNC( "Dist::GenComm::recv_msg" );
            // ==> Wait for a client to send a message:

            // ==> Receive the message from a client:
            try {
                serializer * ser = m_channel.waitForAnyClient( sender );
                
                // Check:
                CNC_ASSERT( 0 <= sender && sender < m_channel.numProcs() );
                CNC_ASSERT( sender != m_channel.localId() );
                
                if( ser ) {
                    // Ok, there is body to it, that's why we got the serializer
                    // Call the recv callback of the communicator instance
                    m_instance.recv_msg_callback( *ser, sender );
                    return false;
                } else {
                    // ==> termination requested !!!
                    return true;
                }
            }
            catch ( ConnectionError ) {
                CNC_ABORT( "Connection Error" );
            }
            catch ( ... ) {
                //throw ClientId( sender );
            }
            return false;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        //%%%%%%%%%%%%%%%
        //              %
        //  SendThread  %
        //              %
        //%%%%%%%%%%%%%%%

        class GenericCommunicator::SendThread : public ThreadExecuter
        {
        public:
            typedef scalable_vector< request_type > RcverArray;
            SendThread( ChannelInterface & channel ) : m_channel( channel ) {}

            /// ThreadExecuter interface:
            virtual void runEventLoop();
            virtual void stop();

            /// The following "push" methods are called from an
            /// "application thread" (not from the sender thread):
            void pushForSend( serializer * ser, int rcverLocalId );
            void pushForBcast( serializer * ser );
            bool pushForBcast( serializer * ser, const int * rcverArr, int n, int globalIdShift );
            void pushTerminationRequest( int rcverLocalId, volatile bool * indicator );
            void pushStopRequest();
            bool has_pending_messages();

        private:
            /// The following methods are called on the sender thread only:
            request_type send( serializer * ser, int rcverLocalId );
            void bcast( serializer * ser );
#ifndef PRE_SEND_MSGS
            void bcast( serializer * ser, const RcverArray & rcverArr );
#endif
            void sendTerminationRequest( int rcverLocalId, volatile bool * indicator );
            struct SendItem;
            void cleanupSerializer( serializer *& ser ) { delete ser; ser = 0; }
            void cleanupItemData( SendItem & item );

            /// A SendThread belongs to a specific GenericCommunicator.
            /// It uses the ChannelInterface of this instance.
            ChannelInterface & m_channel;

            /// Queue where send requests are put into.
            /// A separate sender thread pops the items from there
            /// and performs the actual send operation.
            struct SendItem {
                serializer * m_ser;
                int m_rcverLocalId;
#ifdef PRE_SEND_MSGS
                request_type m_req;
#endif
                volatile bool * m_indicator; // if nonzero, this will be set to "true" if the
                                             // communication has been finished successfully
                RcverArray * m_rcverArr; // for bcast
                
                SendItem( serializer * ser = 0, int rcverLocalId = 0,
                          volatile bool * indicator = 0, RcverArray * rcverArr = 0
#ifdef PRE_SEND_MSGS
                          , request_type req = MPI_REQUEST_NULL
#endif
                          )
                    : m_ser( ser ), m_rcverLocalId( rcverLocalId ),
                      m_indicator( indicator ), m_rcverArr( rcverArr )
#ifdef PRE_SEND_MSGS
                    , m_req( req )
#endif
                {}
            };
            tbb::concurrent_bounded_queue< SendItem > m_sendQueue;
        };

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool GenericCommunicator::SendThread::has_pending_messages()
        {
            return m_sendQueue.empty() == false;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::runEventLoop()
        {
            SendItem item;
            bool done = false;
            do
                {
                    // Wait for next item in the queue:
                    m_sendQueue.pop( item );
                    /*
                     *  Distinguish four cases:
                     *
                     *  ser != 0, rcver >= 0  ==> Send
                     *  ser != 0, rcver == -1 ==> Bcast
                     *  ser == 0, rcver >= 0  ==> SendTerminationRequest
                     *  ser == 0, rcver == -1 ==> local finish request
                     */
                    
                    // Send/bcast this item:
                    if ( item.m_ser && item.m_rcverLocalId != -1 ) {
                        CNC_ASSERT( item.m_rcverArr == 0 );
#ifdef PRE_SEND_MSGS
                        m_channel.wait( &item.m_req, 1 );
#else
                        send( item.m_ser, item.m_rcverLocalId );
#endif
                    } else if ( item.m_ser != 0 ) {
                        CNC_ASSERT( item.m_rcverLocalId == -1 );
                        if ( item.m_rcverArr == 0 ) {
                            bcast( item.m_ser );
                        } else {
#ifdef PRE_SEND_MSGS
                            m_channel.wait( &item.m_rcverArr->front(), item.m_rcverArr->size() );
#else
                            bcast( item.m_ser, *item.m_rcverArr );
#endif
                        }
                    }
                    else if ( item.m_rcverLocalId >= 0 ) {
                        CNC_ASSERT( item.m_rcverArr == 0 );
                        sendTerminationRequest( item.m_rcverLocalId, item.m_indicator );
                        // if nonzero, item.m_indicator will be set to true
                        // if the message has been sent successfully.
                    }
                    else {
                        CNC_ASSERT( item.m_rcverArr == 0 );
                        CNC_ASSERT( item.m_rcverLocalId == -1 );
                        done = true;
                    }
                    
                    // Delete corresponding serializer:
                    cleanupItemData( item );
                } while ( ! done );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::cleanupItemData( SendItem & item )
        {
            cleanupSerializer( item.m_ser );
            delete item.m_rcverArr;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::pushForSend( serializer * ser, int rcverLocalId )
        {
            CNC_ASSERT( ser && rcverLocalId >= 0 );
#ifdef WITHOUT_SENDER_THREAD
            send( ser, rcverLocalId );
            cleanupSerializer( ser );
#else
# ifdef PRE_SEND_MSGS
            request_type req = send( ser, rcverLocalId );
            m_sendQueue.push( SendItem( ser, rcverLocalId, NULL, NULL, req ) );
#else
            m_sendQueue.push( SendItem( ser, rcverLocalId ) );
# endif
            // nonzero ser and rcver id >= 0 indicate Send
#endif
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::pushForBcast( serializer * ser )
        {
# ifdef PRE_SEND_MSGS
            pushForBcast( ser, NULL, m_channel.numProcs(), 0 ); // FIXME m_globalIdShift );
#else
            CNC_ASSERT( ser != 0 );

# ifdef CNC_WITH_ITAC
            // ITAC logging of bcast messages:
            for ( int rcver = 0; rcver < m_channel.numProcs(); ++rcver ) {
                // skip inactive clients (especailly myself):
                if ( ! m_channel.isActive( rcver ) ) continue;
#   ifdef WITHOUT_SENDER_THREAD_TRACING
                VT_SEND( rcver, (int)ser->get_total_size(), ITC_TAG_EXTERNAL );
#   else
                VT_SEND( m_channel.localId(), (int)ser->get_total_size(), ITC_TAG_INTERNAL );
#   endif
            }
# endif // CNC_WITH_ITAC

# ifdef WITHOUT_SENDER_THREAD
            bcast( ser );
            cleanupSerializer( ser );
# else
            m_sendQueue.push( SendItem( ser, -1 ) );
            // nonzero ser and rcver id == -1 indicate Bcast
# endif
#endif // PRE_SEND_MSGS
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool GenericCommunicator::SendThread::pushForBcast( serializer * ser, const int * rcverArr, int n, int globalIdShift )
        {
            CNC_ASSERT( ser != 0 );
            
            // Allocate copy of the given rcverArr. Note that rcverArr 
            // still contains global ids. They must be transformed into local ones
            // (using globalIdShift).
            RcverArray* myRcverArr = new RcverArray( n );
            RcverArray::iterator it = myRcverArr->begin();
            int _new_n = n;
            bool _self = false;
            for( int i = 0; i < n; ++ i ) {
                // Transform global id to local id:
                int localRecvId = rcverArr != NULL ? rcverArr[i] - globalIdShift : i;
                CNC_ASSERT( 0 <= localRecvId && localRecvId < m_channel.numProcs() );  
                // Skip inactive clients (especailly myself):
                if ( ! m_channel.isActive( localRecvId ) ) {
                    if( localRecvId == m_channel.localId() && rcverArr != NULL ) _self = true;
                    --_new_n;
                    continue;
                }
                // ITAC tracing:
#ifdef WITHOUT_SENDER_THREAD_TRACING
                VT_SEND( localRecvId, (int)ser->get_total_size(), ITC_TAG_EXTERNAL );
#else
                VT_SEND( m_channel.localId(), (int)ser->get_total_size(), ITC_TAG_INTERNAL );
#endif
#ifdef PRE_SEND_MSGS
                *it = send( ser, localRecvId );
#else
                *it = localRecvId;
#endif
                ++it;
            }
            
            myRcverArr->resize( _new_n );

#ifdef WITHOUT_SENDER_THREAD
            bcast( ser, *myRcverArr );
            cleanupSerializer( ser );
            delete myRcverArr;
#else
            m_sendQueue.push( SendItem( ser, -1, 0, myRcverArr ) );
            // nonzero ser and rcver id == -1 indicate Bcast
            // myRcverArr will be deleted after bcast on the sender thread
#endif

            return _self;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::pushTerminationRequest( int rcverLocalId, volatile bool * indicator )
        {
            CNC_ASSERT( rcverLocalId >= 0 );
#ifdef WITHOUT_SENDER_THREAD
            sendTerminationRequest( rcverLocalId, indicator );
#else
            m_sendQueue.push( SendItem( 0, rcverLocalId, indicator ) );
            // zero ser and rcver id >= 0 indicate "send termination request"
#endif
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::pushStopRequest()
        {
            m_sendQueue.push( SendItem( 0, -1 ) );
            // zero ser and rcver id == -1 indicates local finish request
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::stop()
        {
            // This overwrites (extends) ThreadExecuter's virtual stop()
            // method. Before joining with sender thread (which is done by
            // the generic ThreadExecuter::stop()) we notify the sender
            // thread that it should finish.

            // Indicate to the sender thread that it should stop executing.
            // This is done by pushing a special item on the send queue.
            pushStopRequest();

            // Now join with the sender thread:
            ThreadExecuter::stop();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        request_type GenericCommunicator::SendThread::send( serializer * ser, int rcverLocalId )
        {
            CNC_ASSERT( 0 <= rcverLocalId && rcverLocalId < m_channel.numProcs() );
            CNC_ASSERT( ser && m_channel.isActive( rcverLocalId ) );

#ifndef WITHOUT_SENDER_THREAD_TRACING
            // ITAC logging:
            VT_RECV( m_channel.localId(), (int)ser->get_total_size(), ITC_TAG_INTERNAL );
            VT_SEND( rcverLocalId, (int)ser->get_total_size(), ITC_TAG_EXTERNAL );
#endif
            
            size_type _bsz = ser->get_body_size();
            CNC_ASSERT( _bsz > 0 );
            // we send header and body at once:
            request_type ret = m_channel.sendBytes( ser->get_header(), ser->get_header_size(), _bsz, rcverLocalId );
            CNC_ASSERT( _bsz == ser->get_body_size() );
#ifndef PRE_SEND_MSGS
            CNC_ASSERT( ret != -1 );
#endif
            return ret;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::bcast( serializer * ser )
        {
            CNC_ASSERT( ser );

            // Send data. Protect access to sendSocket by mutex.
            int maxNumRecv = m_channel.numProcs();
            size_type _bsz = ser->get_body_size();
            for ( int rcver = 0; rcver < maxNumRecv; ++rcver ) {
                // skip inactive clients (especailly myself):
                if ( ! m_channel.isActive( rcver ) ) continue;
                // Send message to this receiver:
                send( ser, rcver );
                CNC_ASSERT( _bsz == ser->get_body_size() );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef PRE_SEND_MSGS
        void GenericCommunicator::SendThread::bcast( serializer * ser, const RcverArray & rcverArr )
        {
            CNC_ASSERT( ser );

            size_type _bsz = ser->get_body_size();
            // Send data. Protect access to sendSocket by mutex.
            for ( RcverArray::const_iterator it = rcverArr.begin(); it != rcverArr.end(); ++it ) {
                CNC_ASSERT( 0 <= *it && *it < m_channel.numProcs() );
                // must have elided inactive clients (especailly myself):
                CNC_ASSERT( m_channel.isActive( *it ) );
                
                // Send message to this receiver:
                send( ser, *it );
                CNC_ASSERT( _bsz == ser->get_body_size() );
            }
        }
#endif

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::SendThread::sendTerminationRequest( int rcverLocalId, volatile bool * indicator )
        {
            CNC_ASSERT( 0 <= rcverLocalId && rcverLocalId < m_channel.numProcs() );
            CNC_ASSERT( m_channel.isActive( rcverLocalId ) );

            // Dummy serializer:
            serializer ser( m_useCRC, true );
            ser.set_mode_pack();

            // Prepare it for sending: set its header to NULL
            // which indicates the termination request.
            BufferAccess::finalizePack( ser );
            CNC_ASSERT( ser.get_body_size() == 0 );

            // Send only the zero'd header:
            m_channel.sendBytes( ser.get_header(), ser.get_header_size(), 0 /* body size */ , rcverLocalId );

            // Indicate that the message has been sent successfully:
            if ( indicator ) {
                *indicator = true;
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        //%%%%%%%%%%%%%%%%%%%%%%%%
        //                       %
        //  GenericCommunicator  %
        //                       %
        //%%%%%%%%%%%%%%%%%%%%%%%%

        GenericCommunicator::GenericCommunicator( msg_callback & cb, bool de )
            : m_channel( 0 ),
              m_callback( cb ),
              m_sendThread( 0 ),
              m_recvThread( 0 ),
              m_globalIdShift( 0 ),
              m_hasBeenInitialized( false ),
              m_exit0CallOk( true ),
              m_distEnv( de )
        {
            m_callback.set_communicator( this );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        GenericCommunicator::~GenericCommunicator()
        {
            // Cleanup ITAC stuff:
            VT_FINALIZE();
            m_callback.set_communicator(NULL);
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int GenericCommunicator::myPid()
        {
            return m_channel->localId() + m_globalIdShift;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int GenericCommunicator::numProcs()
        {
            return m_channel->numProcs();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool GenericCommunicator::remote()
        {
            return ( m_channel->localId() != 0 ) ? true : false;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int GenericCommunicator::init( int minId, long /*flag*/ )
        {
            VT_FUNC_I( "Dist::GenComm::init" );

            // Already running?
            if ( m_hasBeenInitialized ) {
                return 0;
            } else {
                m_hasBeenInitialized = true;
            }

            // Initialize ids:
            m_globalIdShift = minId;

            /*
             * NOTE: This method will be called from init() of a derived class.
             *       The derived class must have instantiated m_channel
             *       accordingly. Therefore, we can use m_channel here.
             */

            // A process cannot send a message to itself.
            // Therefore, disable this channel:
            CNC_ASSERT( m_channel );
            m_channel->deactivate( m_channel->localId() );

            // Start sender loop (same on host and on client):
            CNC_ASSERT( m_sendThread == 0 );
            m_sendThread = new SendThread( *m_channel );
#ifndef WITHOUT_SENDER_THREAD
# ifndef WITHOUT_SENDER_THREAD_TRACING
            m_sendThread->defineThreadName( "SEND", m_channel->localId() );
# endif
            m_sendThread->start();
#endif

            // Start receiver loop:
            CNC_ASSERT( m_recvThread == 0 );
            m_recvThread = new RecvThread( *this ); // needs whole instance, not only the channel
            if ( ! remote() || isDistributed() ) {
                // Host(s) runs the receiver loop in a separate thread.
                // Prepare receiver thread:
                m_recvThread->defineThreadName( "RECV" );
                m_recvThread->start();
            } else {
                // clients run it in the main thread
                m_recvThread->runEventLoop();
                fini();

                // On certain OSes (such as MIC), a call to "exit" is 
                // considered an abnormal termination, regarless of 
                // the argument. 
                if (m_exit0CallOk) exit( 0 ); 
            }

            return 0;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::fini()
        {
            VT_FUNC_I( "Dist::GenComm::fini" );
            // Something to do at all?
            if ( ! m_hasBeenInitialized ) {
                return;
            } else {
                m_hasBeenInitialized = false;
            }

            // Host sends termination requests to the remote clients.
            // Each client will send a response.
            if( m_channel->localId() == 0 ) {
                for ( int client = 1; client < numProcs(); ++client ) {
                    send_termination_request( client );
                }
            }

            // Stop sender and receiver threads:
            if ( m_recvThread ) {
                m_recvThread->stop();
            }
            if ( m_sendThread ) {
                m_sendThread->stop();
            }

            // Cleanup:
            delete m_recvThread;
            m_recvThread = NULL;
            delete m_sendThread;
            m_sendThread = NULL;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::send_msg( serializer * ser, int rcver )
        {
            VT_FUNC_I( "Dist::GenComm::send_msg" );

            // Transform id from global to local:
            rcver -= m_globalIdShift;

            // Check:
            CNC_ASSERT( 0 <= rcver && rcver < m_channel->numProcs() );
            CNC_ASSERT( rcver != m_channel->localId() ); // no message to self allowed
            CNC_ASSERT( m_channel->isActive( rcver ) );

            // Prepare the serializer for sending (add Size, CRC etc.)
            CNC_ASSERT( ser && ser->is_packing() );
            //BufferAccess::finalizePack( *ser );

            // ITAC logging:
#ifdef WITHOUT_SENDER_THREAD_TRACING
            VT_SEND( rcver, (int)ser->get_total_size(), ITC_TAG_EXTERNAL );
#else
            VT_SEND( m_channel->localId(), (int)ser->get_total_size(), ITC_TAG_INTERNAL );
#endif
            // Send data:
            m_sendThread->pushForSend( ser, rcver );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::bcast_msg( serializer * ser )
        {
            VT_FUNC_I( "Dist::GenComm::bcast_msg" );

            // Prepare the serializer for sending (add Size, CRC etc.)
            CNC_ASSERT( ser && ser->is_packing() );
            //BufferAccess::finalizePack( *ser );

            // Bcast data:
            m_sendThread->pushForBcast( ser );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool GenericCommunicator::bcast_msg( serializer * ser, const int * rcvers, int nrecvrs )
        {
            VT_FUNC_I( "Dist::GenComm::bcast_msg" );

            // Prepare the serializer for sending (add Size, CRC etc.)
            CNC_ASSERT( ser && ser->is_packing() );
            //BufferAccess::finalizePack( *ser );

            // Bcast data:
            return m_sendThread->pushForBcast( ser, rcvers, nrecvrs, m_globalIdShift );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::send_termination_request( int rcverLocalId, bool isBlocking )
        {
            VT_FUNC_I( "Dist::GenComm::send_termination_request" );

            // Send data. No ITAC logging of termination message.
            if ( ! isBlocking ) {
                m_sendThread->pushTerminationRequest( rcverLocalId, 0 );
            }
            else {
                volatile bool myIndicator = false;
                m_sendThread->pushTerminationRequest( rcverLocalId, &myIndicator );
                // Blocking send: wait until the message has been sent successfully:
                while ( ! myIndicator ) {
                    YIELD();
                }
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::recv_msg_callback( serializer & ser, int sender )
        {
            // ITAC logging:
            VT_RECV( sender, (int)ser.get_total_size(), ITC_TAG_EXTERNAL );

            // Prepare the serializer for unpacking:
            //BufferAccess::initUnpack( ser );

            // Delegate the message to the msg_callback:
            /*            int globalSenderId = sender + m_globalIdShift; uncomment this for the local tests */
            {
                VT_FUNC( "Dist::msg_callback::recv_msg" );
                m_callback.recv_msg( &ser/*, globalSenderId uncomment this for the local tests */ );
            }
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool GenericCommunicator::has_pending_messages()
        {
            return m_sendThread->has_pending_messages();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void GenericCommunicator::unsafe_barrier()
        {
            CNC_ABORT("unsafe_barrier not implemented for this communicator");
        }

    } // namespace Internal
} // namespace CnC
