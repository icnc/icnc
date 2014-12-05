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
  A few utilities for platform independent interaction with the system
*/

#ifndef _CNC_COMM_PAL_UTIL_H_
#define _CNC_COMM_PAL_UTIL_H_

#include "pal_config.h"
#include <string>

namespace CnC {
namespace Internal 
{
    /**
     * return system time in seconds since some unspecified point in the past
     */
    FLOAT64 PAL_TimeOfDayClock();

    /**
     * Returns a good random 32 bit number without affecting the application.
     * Implemented on Linux with /dev/random and rand() as fallback.
     */
    UINT32 PAL_random();

    /**
     * Routines for printing output
     */
    void PAL_Error( const char *format, ... );
    void PAL_Warning( const char *format, ... );
    void PAL_DebugPrintf( const char *filename, int line, const char *format, ... );

    /**
     * Returns a string containing the given path name, and double-quoted
     * if necessary (i.e. if the path name contains blanks).
     */
    std::string PAL_MakeQuotedPath( const char pathName[] );

    /**
     * Starts the given executable as a new process in the background.
     * @param exeName : name of the executable. May contain blanks, need
     *        not be double-quoted in this case.
     * @param argv : command line args. Must be NULL terminated. 
     *        0th entry is first command line arg (not name of executable).
     *        The entries of argv must not contain double quotes (due to
     *        problems on Windows). If an entry of argv contains a blank,
     *        it'll be split up into two different arguments, so avaoid
     *        blanks in arguments.
     * @return -1 on error
     */
    int PAL_StartProcessInBackground( const char exeName[], const char* argv[] );

    /**
     * Determines the name of the currently running executable.
     * optionally stores command-line arguments in (optional) string parameter
     */
    std::string PAL_GetProgname( std::string * = NULL );

    /**
     * Determines the name of the current working directory.
     */
    std::string PAL_getCWD();

    /**
     * Sets environment variable <name> to given value:
     */
    void PAL_SetEnvVar( const char* name, const char* value, int overwrite = 1 );

} // namespace Internal
} // namespace CnC

#endif // _CNC_COMM_PAL_UTIL_H_

/*@} */
