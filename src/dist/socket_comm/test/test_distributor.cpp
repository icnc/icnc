#include <cnc/internal/dist/distributor.h>
#include <cnc/serializer.h>
#include "SocketCommunicator.h"

#include <iostream>
#include <stdio.h>

#include "test.h"

namespace CnC
{
    namespace Internal
    {
        distributor::my_map          distributor::m_distContexts[2];
        std::atomic< int >           distributor::m_nextGId;
        distributor::state_type      distributor::m_state;
        tbb::concurrent_queue< int > distributor::m_sync;
        int                          distributor::m_flushCount;
        std::atomic< int >           distributor::m_nMsgsRecvd;
        
        communicator * distributor::m_communicator;
        static SocketCommunicator g_mySocketComm;
        static int g_recv_counter = 0;
        volatile bool g_host_communication_done = false;

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        serializer * distributor::new_serializer( const distributable_context * )
        {
            serializer * ser = new serializer( g_mySocketComm.use_crc(), true ); // CRC, Size
            ser->set_mode_pack();
            return ser;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::send_msg( serializer * ser, int rcver )
        {
            BufferAccess::finalizePack( *ser );
            m_communicator->send_msg( ser, rcver );
            //delete ser;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::bcast_msg( serializer * ser )
        {
             BufferAccess::finalizePack( *ser );
             m_communicator->bcast_msg( ser );
             //delete ser;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::bcast_msg( serializer * ser, const std::vector< int > & rcverArr )
        {
             BufferAccess::finalizePack( *ser );
             m_communicator->bcast_msg( ser, rcverArr );
             //delete ser;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static struct HostData
        {
        public:
            int m_numClients;
            int m_numClientsFinished;
            std::vector< bool > m_clientsFinished;
        public:
            HostData( int numClients )
              : m_numClients( numClients ),
                m_numClientsFinished( 0 ),
                m_clientsFinished( numClients + 1, false ) {}
        } * g_host_data = 0;

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void init_host_data()
        {
             // Let's set our own socket communicator:
            distributor::m_communicator = &g_mySocketComm;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::recv_msg( serializer *ser, int pid )
        {
            const int terminationId = -111111;

             // Host initialization stuff:
            if ( ! remote() && g_host_data == 0 ) {
                CNC_ASSERT( ! g_host_communication_done );
                g_host_data = new HostData( g_mySocketComm.numProcs() - 1 );
            }

             // Unpack message:
            BufferAccess::initUnpack( *ser );
            int dummy;
            (*ser) & dummy;
            printf( "PROC %d: received %d from %d\n", myPid(), dummy, pid );
            fflush( stdout );

             // Host checks for finalization messages:
            if ( ! remote() && dummy == terminationId ) { // indicates that client is finished
                CNC_ASSERT( 0 < pid && pid <= g_host_data->m_numClients );
                CNC_ASSERT( g_host_data->m_clientsFinished[pid] == false );
                g_host_data->m_clientsFinished[pid] = true;
                ++g_host_data->m_numClientsFinished;
                if ( g_host_data->m_numClientsFinished == g_host_data->m_numClients ) {
                    g_host_communication_done = true; 
                    // wakes up host application thread
                }
            }

             // SENDS:
            if ( g_recv_counter == 0 )
            {
                 // Clients send some messages:
                 // [0 --> 1], then
                 // 1 --> 2, 2 --> 3, ..., N-1 --> N, N --> 0 [=host]
                if ( remote() ) {
                    int dummy = ( myPid() + 1 ) * 123;
                    int recver = ( myPid() + 1 ) % numProcs();
                    serializer * ser = new_serializer( 0 );
                    (*ser) & dummy;
                    send_msg( ser, recver );
                }

                 // last client makes a bcast:
                if ( myPid() == numProcs() - 1 ) {
                    int dummy = 5555;
                    serializer * ser = new_serializer( 0 );
                    (*ser) & dummy;
#if 1
                     // bcast to all others:
                    bcast_msg( ser ); 
#else
                     // restricted bcast variant:
                    std::vector< int > rcverArr;
                    rcverArr.push_back( 0 );
                    bcast_msg( ser, rcverArr );
#endif
                }

                 // Finally each client sends a termination message to the host:
                if ( remote() ) {
                    int dummy = terminationId;
                    serializer * ser = new_serializer( 0 );
                    (*ser) & dummy;
                    send_msg( ser, 0 );
                }
            }

            // Adjust recv counter:
            ++g_recv_counter;
        }

    } // namespace Internal
} // namespace Worklets

