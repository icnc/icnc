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
  see MpiHostInitializer.h
*/

#include <mpi.h>

#include "MpiHostInitializer.h"
#include "MpiChannelInterface.h"
#include "Settings.h"
#include "itac_internal.h"

#include <cstdlib> // atoi
#include <cstdio> // popen
#include <cnc/internal/cnc_stddef.h>
#include <string>
#include <iostream>

#define HERE __FILE__, __LINE__

namespace CnC
{
    namespace Internal
    {
        MpiHostInitializer::MpiHostInitializer( MpiChannelInterface & obj )
        :
            m_channel( obj )
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        MpiHostInitializer::~MpiHostInitializer()
        {
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiHostInitializer::init_mpi_comm( const MPI_Comm & comm )
        {
            VT_FUNC_I( "MpiHostInitializer::init_mpi_comm" );

             // How many clients are to be expected?
             // Default is 1, may be changed via environment variable.
            int myClientId = 2711;
            MPI_Comm_rank( comm, &myClientId );
            CNC_ASSERT( myClientId == 0 );
            int size;            
            MPI_Comm_size( comm, &size );
            int nClientsExpected = size-1;

             // ==> Now the number of relevant processes is known.
             //     Prepare the data structures accordingly !!!
            m_channel.setLocalId( 0 );
            m_channel.setNumProcs( 1 + nClientsExpected );

            //%%%%%%%%%%%%%%%%%%%%%%%%%%

            // if something went wrong (exception) up to here,
            // there is nothing we can do to recover ...

            // Agree with the client on setup info (e.g. the number of workers).
            for ( int iClient = 1; iClient <= nClientsExpected; ++iClient ) {
                exchange_setup_info( iClient );
            }

            // Initalize ITAC:
            init_itac_comm();
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiHostInitializer::exchange_setup_info( int client ) // 1,2,3,...
        {
            // nothing special to do here
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiHostInitializer::init_itac_comm()
        {
        }

    } // namespace Internal
} // namespace CnC
