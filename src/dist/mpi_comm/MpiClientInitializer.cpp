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
  see MpiClientInitializer.h
*/

#include <mpi.h>

#include "MpiClientInitializer.h"
#include "MpiChannelInterface.h"
#include "Settings.h"
#include "itac_internal.h"
#include <cstring>
#include <cstdlib>
#include <string>
#include <iostream>
#include <cnc/internal/cnc_stddef.h>

#define HERE __FILE__, __LINE__

namespace CnC
{
    namespace Internal
    {
        MpiClientInitializer::MpiClientInitializer( MpiChannelInterface & obj)
        :
            m_channel( obj )
        {
            VT_FUNC_I( "MpiClientInitializer::MpiClientInitializer" );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        MpiClientInitializer::~MpiClientInitializer()
        {
            VT_FUNC_I( "~MpiClientInitializer::MpiClientInitializer" );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::init_mpi_comm( const MPI_Comm & comm )
        {
            VT_FUNC_I( "Mpi::init_mpi_comm" );

             // Predefined client id?
            int myClientId = 0;
            int numProcsTotal;
            MPI_Comm_rank( comm, &myClientId );
            MPI_Comm_size( comm, &numProcsTotal );
            CNC_ASSERT( myClientId >= 0 );

             // ==> Now the number of relevant processes is known.
             //     Prepare the data structures accordingly !!!
            m_channel.setLocalId( myClientId );
            m_channel.setNumProcs( numProcsTotal );

             // Agree with the host on setup data like the number of worker threads.
            exchange_setup_info();

             // Setup connections between clients. Host will be coordinating this.
            build_client_connections();

             // Final step of initialization: setup of ITAC connections (VTcs):
            init_itac_comm();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::exchange_setup_info()
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::build_client_connections()
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::accept_client_connections()
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::connect_to_other_client()
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiClientInitializer::init_itac_comm()
        {
        }

    } // namespace Internal
} // namespace CnC
