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
  see MpiChannelInterface.h
*/

#include <mpi.h>
#include "MpiChannelInterface.h"
#include "itac_internal.h"
#include <iostream>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/cnc_stddef.h>
#include <climits>
#include <iostream>

namespace CnC
{
    namespace Internal
    {
        MpiChannelInterface::MpiChannelInterface( bool useCRC, MPI_Comm cm )
            : ChannelInterface(),
              m_ser1( new serializer ),
              m_ser2( new serializer ),
              m_communicator( cm ),
              m_request( MPI_REQUEST_NULL )
        {
            // Prepare serializers:
            BufferAccess::ensure_capacity( *m_ser1, ChannelInterface::BUFF_SIZE );
            BufferAccess::ensure_capacity( *m_ser2, ChannelInterface::BUFF_SIZE );
            m_ser1->set_mode_unpack( useCRC, true );
            m_ser2->set_mode_unpack( useCRC, true );
            MPI_Irecv( m_ser1->get_header(), BufferAccess::capacity( *m_ser1 ), MPI_CHAR, MPI_ANY_SOURCE, FIRST_MSG, m_communicator, &m_request );
        }

        MpiChannelInterface::~MpiChannelInterface()
        {
            MPI_Cancel( &m_request );
            delete m_ser1;
            delete m_ser2;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int MpiChannelInterface::sendBytes( void * data, size_type headerSize, size_type bodySize, int rcverLocalId )
        {
            VT_FUNC_I( "MPI::sendBytes" );
            CNC_ASSERT( 0 <= rcverLocalId && rcverLocalId < numProcs() );
            if( bodySize + headerSize > INT_MAX ) {
                std::cerr << "MPI_Get_count doesn't allow a count > " << INT_MAX << ". No workaround implemented yet." << std::endl;
                MPI_Abort( m_communicator, 1234 );
            }
            char* header_data = static_cast<char*>( data );
            MPI_Request request = 0;
            if( headerSize+bodySize < BUFF_SIZE ) {
                MPI_Isend( header_data, headerSize+bodySize, MPI_CHAR, rcverLocalId, FIRST_MSG, m_communicator, &request );
            } else {
                CNC_ASSERT( bodySize > 0 );
                // header Tag should not be equal to localId()
                MPI_Send( header_data, headerSize, MPI_CHAR, rcverLocalId, FIRST_MSG, m_communicator );
                char * body_data = header_data+headerSize;
                MPI_Isend( body_data, bodySize, MPI_CHAR, rcverLocalId, SECOND_MSG, m_communicator, &request );
            }
            //            { Speaker oss; oss << "sendBytes " << headerSize << " " << bodySize; }
#ifdef PRE_SEND_MSGS
            return request;
#else
            MPI_Wait( &request, MPI_STATUS_IGNORE );
            return 0;
#endif
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiChannelInterface::wait( int * requests, int cnt )
        {
            VT_FUNC_I( "MPI::wait" );
            MPI_Waitall( cnt, requests, MPI_STATUSES_IGNORE );
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        serializer * MpiChannelInterface::waitForAnyClient( int & senderLocalId )
        {
            VT_FUNC_I( "MPI::waitForAnyClient" );

            MPI_Status status;
            MPI_Wait( &m_request, &status );
            senderLocalId = status.MPI_SOURCE;
            CNC_ASSERT( 0 <= senderLocalId && senderLocalId < numProcs() );
            int _cnt;
            MPI_Get_count( &status, MPI_CHAR, &_cnt );
            
            size_type _bodySize = m_ser1->unpack_header(); // throws an exception in case of error
            CNC_ASSERT( _bodySize + m_ser1->get_header_size() == _cnt || m_ser1->get_header_size() == _cnt );

            // if we did not receive the body yet, we need to do so now
            if( _bodySize != 0 ) {
                CNC_ASSERT( _bodySize != Buffer::invalid_size );
                BufferAccess::acquire( *m_ser1, _bodySize ); // this is needed even if all is received: sets current pointer in buffer
                if( _cnt == m_ser1->get_header_size() ) {
                    // Enlarge the buffer if needed
                    MPI_Recv( m_ser1->get_body(), _bodySize, MPI_CHAR, senderLocalId, SECOND_MSG, m_communicator, MPI_STATUS_IGNORE );
                }
            }

            std::swap( m_ser1, m_ser2 ); // double buffer exchange
            m_ser1->set_mode_unpack();
            MPI_Irecv( m_ser1->get_header(), BufferAccess::capacity( *m_ser1 ), MPI_CHAR, MPI_ANY_SOURCE, FIRST_MSG, m_communicator, &m_request );
            
            //            { Speaker oss; oss << "recvBytes " << _bodySize; }

            return _bodySize != 0 ? m_ser2 : NULL;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void MpiChannelInterface::recvBodyBytes( void * body, size_type bodySize, int senderLocalId )
        {
            VT_FUNC_I( "MPI::recvBodyBytes" );
            CNC_ASSERT( 0 <= senderLocalId && senderLocalId < numProcs() );
            MPI_Recv( body, bodySize, MPI_CHAR, senderLocalId, SECOND_MSG, m_communicator, MPI_STATUS_IGNORE );
        }

    } // namespace Internal
} // namespace CnC
