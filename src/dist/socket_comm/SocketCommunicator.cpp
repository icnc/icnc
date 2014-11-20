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
  see SocketCommunicator.h
*/

#include "SocketCommunicator.h"
#include "SocketChannelInterface.h"
#include "SocketHostInitializer.h"   // does the init work (for the host)
#include "SocketClientInitializer.h" // does the init work (for the clients) 
#include "Settings.h"
#include "itac_internal.h"

#include <cnc/internal/dist/msg_callback.h>

#include <cnc/internal/cnc_stddef.h>

namespace CnC
{
    namespace Internal
    {
        SocketCommunicator::SocketCommunicator( msg_callback & cb )
          : GenericCommunicator( cb )
        {}

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        SocketCommunicator::~SocketCommunicator()
        {
            CNC_ASSERT( m_channel == 0 ); // was deleted in fini()
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int SocketCommunicator::init( int minId, long flag )
        {
            VT_FUNC_I( "Dist::Socket::init" );

             // Already running?
            if ( m_hasBeenInitialized ) {
                return 0;
            }

             // Initialize socket channel object:
            double timeout = Internal::Settings::get_double( "CNC_SOCKET_TIMEOUT", -1.0 );
            CNC_ASSERT( m_channel == 0 );
            SocketChannelInterface* myChannel = new SocketChannelInterface( use_crc(), timeout );
            m_channel = myChannel;

             // Are we on the host or on the remote side?
            const char* CnCSocketClient = getenv( "CNC_SOCKET_CLIENT" ); // don't make it a config-option/Setting
            if ( ! CnCSocketClient )
            {
                // ==> HOST startup: 
                // This initializes the PAL_socket attributes in myChannel.
                SocketHostInitializer hostInitializer( *myChannel );
                hostInitializer.init_socket_comm();
            } else {
                // ==> CLIENT startup:
                // "CnCSocketClient" is the host's contact string.
                // This initializes the PAL_socket attributes in myChannel.
                SocketClientInitializer clientInitializer( *myChannel, CnCSocketClient );
                clientInitializer.init_socket_comm();
            }

             // Now the socket specific setup is finished.
             // Do the generic initialization stuff.
            GenericCommunicator::init( minId, flag );

            return 0;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void SocketCommunicator::fini()
        {
             // Already running?
            if ( ! m_hasBeenInitialized ) {
                return;
            }

             // First the generic cleanup:
            GenericCommunicator::fini();

             // Cleanup of socket specific stuff:
            delete m_channel;
            m_channel = 0;
        }

        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /**
         * Initializing function
         */
        extern "C" void load_communicator_( msg_callback & cb, bool )
        {
            // init communicator, there can only be one
            static SocketCommunicator _sock_c( cb );
        }

    } // namespace Internal
} // namespace CnC
