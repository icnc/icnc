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

#ifndef _CNC_COMM_MPI_COMMUNICATOR_H_
#define _CNC_COMM_MPI_COMMUNICATOR_H_

#include "../generic_comm/GenericCommunicator.h"

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

namespace CnC
{
    class serializer;

    namespace Internal
    {
        /// Overwrites init() and fini() in order to do proper initialization
        /// of mpi environment. Especially, it allocates the m_channel
        /// attribute in the base class GenericCommunicator accordingly.
        /// Evaluates CNC_MPI_HOST and sets it value as MPI_Info("host").
        /// Evaluates CNC_MPI_SPAWN. If set to a number, spawns that many MPI processes
        ///  using the value of CNC_MPI_EXECUTABLE as the program to run.
        /// If CNC_MPI_SPAWN is not set, expects MPI launcher like mpirun/mpiexec.
        ///
        /// The MPI-Communicator supports distributed environments (\see CnC::dist_cnc_init)
        ///
        /// An optional argument provided to init is interpreted as the comunicator to be used.
        /// If none is provided, it uses MPI_COMM_WORLD.
        ///
        /// \see CnC::Internal::GenericCommunicator
        class MpiCommunicator : public GenericCommunicator
        {
        public:
            MpiCommunicator( msg_callback & cb );
            ~MpiCommunicator();
            /// if thecomm != 0 it is interpreted as the MPI_Comm to use
            virtual int init( int minId, long thecomm = 0 );
            virtual void fini();
        private:
            bool m_customComm;
        };
    }
}

#endif // _CNC_COMM_MPI_COMMUNICATOR_H_
