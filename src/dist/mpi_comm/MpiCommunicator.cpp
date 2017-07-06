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
  see MpiCommunicator.h
*/

#include <mpi.h>
#include <cassert>

#include "MpiCommunicator.h"
#include "MpiHostInitializer.h"   // does the init work (for the host)
#include "MpiClientInitializer.h" // does the init work (for the clients) 
#include "MpiChannelInterface.h"
#include "Settings.h"
#include "itac_internal.h"
#include "../socket_comm/pal_util.h"
#include <cnc/internal/dist/msg_callback.h>
#include <iostream>
#include <cnc/internal/cnc_stddef.h>

namespace CnC
{
    namespace Internal
    {
        MpiCommunicator::MpiCommunicator( msg_callback & cb, bool dist_env )
            : GenericCommunicator( cb, dist_env ),
              m_comm( MPI_COMM_NULL ),
              m_customComm( false )
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        MpiCommunicator::~MpiCommunicator()
        {
            VT_FUNC_I( "MpiCommunicator::~MpiCommunicator" );
            CNC_ASSERT( m_channel == 0 ); // was deleted in fini()
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int MpiCommunicator::init( int minId, long thecomm_ )
        {
            VT_FUNC_I( "MpiCommunicator::init" );

            assert( sizeof(thecomm_) >= sizeof(MPI_Comm) );
            MPI_Comm thecomm = (MPI_Comm)thecomm_;

            // turn wait mode on for intel mpi if possible
            // this should greatly improve performance for intel mpi
            PAL_SetEnvVar( "I_MPI_WAIT_MODE", "enable", 0);

            int flag;
            MPI_Initialized( &flag );
            if ( ! flag ) {
                int p;
                //!! FIXME passing NULL ptr breaks mvapich1 mpi implementation
                MPI_Init_thread( 0, NULL, MPI_THREAD_MULTIPLE, &p );
                if( p != MPI_THREAD_MULTIPLE ) {
                    // can't use Speaker yet, need Channels to be inited
                    std::cerr << "[CnC] Warning: not MPI_THREAD_MULTIPLE (" << MPI_THREAD_MULTIPLE << "), but " << p << std::endl;
                }
            } else if( thecomm == 0 ) {
                CNC_ABORT( "Process has already been initialized" );
            }


            m_comm = MPI_COMM_WORLD;
            int rank;
            MPI_Comm parentComm;
            if( thecomm == 0 ) {
                MPI_Comm_get_parent( &parentComm );
            } else {
                m_customComm = true;
                m_exit0CallOk = false;
                m_comm = thecomm;
            }
            MPI_Comm_rank( m_comm, &rank );
            
            // father of all checks if he's requested to spawn processes:
            if ( rank == 0 && parentComm == MPI_COMM_NULL ) {
                // Ok, let's spawn the clients.
                // I need some information for the startup.
                // 1. Name of the executable (default is the current exe)
                const char * _tmp = getenv( "CNC_MPI_SPAWN" );
                if ( _tmp ) {
                    int nClientsToSpawn = atol( _tmp );
                    _tmp = getenv( "CNC_MPI_EXECUTABLE" );
                    std::string clientExe( _tmp ? _tmp : "" );
                    if( clientExe.empty() ) clientExe = PAL_GetProgname();
                    CNC_ASSERT( ! clientExe.empty() );
                    // 3. Special setting for MPI_Info: hosts
                    const char * clientHost = getenv( "CNC_MPI_HOSTS" );
                    
                    // Prepare MPI_Info object:
                    MPI_Info clientInfo = MPI_INFO_NULL;
                    if ( clientHost ) {
                        MPI_Info_create( &clientInfo );
                        if ( clientHost ) {
                            MPI_Info_set( clientInfo, const_cast< char * >( "host" ), const_cast< char * >( clientHost ) );
                            // can't use Speaker yet, need Channels to be inited
                            std::cerr << "[CnC " << rank << "] Set MPI_Info_set( \"host\", \"" << clientHost << "\" )\n";
                        }
                    }
                    // Now spawn the client processes:
                    // can't use Speaker yet, need Channels to be inited
                    std::cerr << "[CnC " << rank << "] Spawning " << nClientsToSpawn << " MPI processes" << std::endl;
                    int* errCodes = new int[nClientsToSpawn];
                    MPI_Comm interComm;
                    int err = MPI_Comm_spawn( const_cast< char * >( clientExe.c_str() ),
                                              MPI_ARGV_NULL, nClientsToSpawn,
                                              clientInfo, 0, MPI_COMM_WORLD,
                                              &interComm, errCodes );
                    delete [] errCodes;
                    if ( err ) {
                        // can't use Speaker yet, need Channels to be inited
                        std::cerr << "[CnC " << rank << "] Error in MPI_Comm_spawn. Skipping process spawning";
                    } else {
                        MPI_Intercomm_merge( interComm, 0, &m_comm );
                    }
                } // else {
                // No process spawning
                // MPI-1 situation: all clients to be started by mpiexec
                //                    m_comm = MPI_COMM_WORLD;
                //}
            }
            if ( thecomm == 0 && parentComm != MPI_COMM_NULL ) {
                // I am a child. Build intra-comm to the parent.
                MPI_Intercomm_merge( parentComm, 1, &m_comm );
            }
            MPI_Comm_rank( m_comm, &rank );

            CNC_ASSERT( m_channel == NULL );
            MpiChannelInterface* myChannel = new MpiChannelInterface( use_crc(), m_comm );
            m_channel = myChannel;

            int size;
            MPI_Comm_size( m_comm, &size );
            // Are we on the host or on the remote side?
            if ( rank == 0 ) {
                if( size <= 1 ) {
                    Speaker oss( std::cerr ); oss << "Warning: no clients avabilable. Forgot to set CNC_MPI_SPAWN?";
                }
                // ==> HOST startup: 
                // This initializes the mpi environment in myChannel.
                MpiHostInitializer hostInitializer( *myChannel );
                hostInitializer.init_mpi_comm( m_comm );
            } else {
                // ==> CLIENT startup:
                // This initializes the mpi environment in myChannel.
                MpiClientInitializer clientInitializer( *myChannel );
                clientInitializer.init_mpi_comm( m_comm );
            }

            { Speaker oss( std::cerr ); oss << "MPI initialization complete (rank " << rank << ")."; }

            //            MPI_Barrier( m_comm );

            // Now the mpi specific setup is finished.
            // Do the generic initialization stuff.
            GenericCommunicator::init( minId );

            return 0;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiCommunicator::fini()
        {
            VT_FUNC_I( "MpiCommunicator::fini" );
            // Already running?
            if ( ! m_hasBeenInitialized ) {
                return;
            }

            // First the generic cleanup:
            GenericCommunicator::fini();

            // Cleanup of mpi specific stuff:
            delete m_channel;
            m_channel = NULL;
            if( ! ( isDistributed() || m_customComm ) ) {
                MPI_Finalize();
            }
        }

        /// Use this with care. Only useful for distributed and envs when you know what you're doing.
        void MpiCommunicator::unsafe_barrier()
        {
            MPI_Barrier(m_comm);
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /**
         * Initializing function
         */
        extern "C" void load_communicator_( msg_callback & cb, bool dist_env )
        {
            // init communicator, there can only be one
            static MpiCommunicator _mpi_c( cb, dist_env );
        }

    } // namespace Internal
} // namespace CnC
