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
  see dist_init.h
*/

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/dist_init.h>

#ifndef _WIN32
# include <dlfcn.h>
#else
#include <windows.h>
#endif
#include <cstring>
#include <string>
#include <iostream>

namespace CnC {
    namespace Internal {

        /// open the given dynamic/shared library
        static communicator_loader_type cnc_load_lib( const char * clientDllName )
        {
            std::cerr << "Loading " << clientDllName << "..." << std::flush;
#ifdef _WIN32
            std::string lib( clientDllName );
            lib.append( ".dll" );
            HMODULE clientDllHandle = LoadLibraryA( lib.c_str() );
#else
            std::string lib( clientDllName );
            lib.insert( 0, "lib" );
            lib.append( ".so" );

            // TODO grrr OpenMPI requires RTLD_GLOBAL...
            void * clientDllHandle = dlopen( lib.c_str(), RTLD_LAZY|RTLD_GLOBAL );
#endif
            if ( ! clientDllHandle ) {
                std::cerr << "\nCould not open client library \'" << clientDllName << "\'\n";
#ifdef _WIN32
                std::cerr << "Error code: " << GetLastError() << std::endl;
                // FIXME: get Windows error string
#else
                std::cerr << dlerror() << std::endl;
#endif
                return NULL;
            }
            std::cerr << "...and communicator...";
            communicator_loader_type loader;
#ifdef _WIN32
            loader = (communicator_loader_type) GetProcAddress( clientDllHandle, "load_communicator_" );
#else              
            loader = (communicator_loader_type) dlsym( clientDllHandle, "load_communicator_" );
#endif
            if ( ! clientDllHandle ) {
                std::cerr << "\nCould not open client library \'" << clientDllName << "\'\n";
#ifdef _WIN32
                std::cerr << "Error code: " << GetLastError() << std::endl;
                // FIXME: get Windows error string
#else
                std::cerr << dlerror() << std::endl;
#endif
                return NULL;
            }
            std::cerr << "done." << std::endl;

            return loader;
        }

        /// generate name of library corresponding to given argument and compile mode
        /// we add _itac to comm if use_itac
        /// we add _debug if debug mode
        const std::string comm_lib_name( const char * comm, bool use_itac )
        {
            std::string lib_name( "cnc_" );
            if ( comm != NULL ) {
                if ( strcmp( comm, "SOCKETS" ) == 0 ) {
                    lib_name.append( "socket" );
                } else if ( strcmp( comm, "MPI" ) == 0 ) {
                    lib_name.append( "mpi" );
                } else if ( strcmp( comm, "SHMEM" ) == 0 ) {
                    return std::string();
                } else if ( strcmp( comm, "XN" ) == 0 ) { 
                    lib_name.append( "xn_" );
#if defined(LRB)
                    lib_name.append("client");
#else
                    lib_name.append("host");
#endif  
                } else {
                    std::cerr << "Warning: DIST_CNC = " << comm 
                              << " is not supported (currently supported  options are SHMEM, SOCKETS, MPI, or XN);"
                              << "proceeding in SOCKETS mode." << std::endl;
                    lib_name.append("socket");
                }
            } else {
                std::cout << "Warning: DIST_CNC environment variable not specified; proceeding in shared-memory mode." 
                          << std::endl;
                return std::string();
            }

            if( use_itac ) lib_name.append( "_itac" );
#ifdef _DEBUG
            lib_name.append( "_debug" );
#endif
#ifdef CNC_DLL_SUFX
#define __STRFY( _x ) #_x
#define _STRFY( _x ) __STRFY( _x )
            lib_name.append( _STRFY( CNC_DLL_SUFX ) );
#endif
            return lib_name;
        }

        communicator_loader_type CNC_API dist_cnc_load_comm( const char * comm, bool use_itac )
        {
            const std::string clientDllName = comm_lib_name( comm, use_itac );
            if ( clientDllName.empty() ) return NULL;
            return cnc_load_lib( clientDllName.c_str() );
        }

    } // namespace Internal
} // end namespace CnC
