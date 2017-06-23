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
  see pal_config.h
*/

#include "pal_config.h" // HAVE_SYS_TIME_H
#include "pal_util.h"

#include <cstdlib> // rand, srand
#include <cstdarg>
#include <cstdio>
#include <cctype>  // isspace
#include <cstring> // strlen
#include <cnc/internal/cnc_stddef.h>

#ifdef _WIN32
# include <windows.h> // _spawnvp, GetModuleFileName
# include <process.h> // _P_NOWAIT for _spawnvp
# include <direct.h>  // _getcwd
# define getcwd _getcwd
#else
# include <unistd.h>
#endif

namespace CnC {
namespace Internal {

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef HAVE_SYS_TIME_H

#include <sys/time.h>
FLOAT64 PAL_TimeOfDayClock()
{
    struct timeval cur_time;
    FLOAT64 time;
    gettimeofday( &cur_time, NULL );
    /* don't remove the (long) - it is required on Solaris */
    time = (cur_time.tv_sec) + (FLOAT64)(long)(cur_time.tv_usec) /  (FLOAT64)1000000;
    return time;
}

#else

#include <sys/timeb.h>
FLOAT64 PAL_TimeOfDayClock()
{
    struct _timeb tv;
    _ftime( &tv );
    return tv.time + (FLOAT64)(long)(tv.millitm) / (FLOAT64)1000.0;
}

#endif

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

UINT32 PAL_random()
{
    static bool initialized = false;
    if ( ! initialized ) {
        srand( (UINT32)PAL_TimeOfDayClock() );
    }
    return (UINT32)rand();
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_Error( const char *format, ... )
{
    fprintf( stderr, "ERROR: " );
    va_list ap;
    va_start( ap, format );
    vfprintf( stderr, format, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
    fflush( stderr );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_Warning( const char *format, ... )
{
    fprintf( stderr, "WARNING: " );
    va_list ap;
    va_start( ap, format );
    vfprintf( stderr, format, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
    fflush( stderr );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_DebugPrintf( const char *filename, int line, const char *format, ... )
{
    fprintf( stderr, "[%s:%d] ", filename, line );
    va_list ap;
    va_start( ap, format );
    vfprintf( stderr, format, ap );
    va_end( ap );
    fprintf( stderr, "\n" );
    fflush( stderr );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

std::string PAL_MakeQuotedPath( const char exeName[] )
{
     // Check whether the string is already quoted:
    int endPos = (int)strlen( exeName ) - 1;
    if ( endPos > 0 && exeName[endPos] == '"' && exeName[0] == '"' ) {
        return exeName; // no need to quote
    }
    
     // Check whether the string contains a blank:
    const char* pc = exeName;
    while ( *pc ) {
        if ( isspace( *pc ) ) {
            return std::string( "\"" ) + exeName + "\"";
        }
        ++pc;
    }
    return exeName;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

int PAL_StartProcessInBackground( const char exeName[], const char* argv[] )
{
    int ret;
    std::string exeNameQuoted = PAL_MakeQuotedPath( exeName );
#ifdef _WIN32
    // ==> Windows
    int counter = 0;
    while ( argv[counter] != 0 ) ++counter;
    int nAddArgs = 3;
    const char** myArgv = new const char*[nAddArgs + counter + 1];
    myArgv[0] = "cmd"; // ignored
    myArgv[1] = "/C";
    myArgv[2] = exeNameQuoted.c_str();
    for ( int j = 0; j <= counter; ++j ) {
        myArgv[nAddArgs + j] = argv[j];
    } // including trailing NULL
    ret = (int)_spawnvp( _P_NOWAIT, "cmd", myArgv );
    delete [] myArgv;
#else
    // ==> Linux
    std::string cmd = exeNameQuoted;
    const char** pArgv = argv;
    while ( *pArgv != NULL ) {
        cmd += " ";
        cmd += *pArgv;
        ++pArgv;
    }
    cmd += " &"; // start in the background
    ret = system( cmd.c_str() );
#endif
    return ret;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

std::string PAL_GetProgname( std::string * args )
{
    const int bufLen = 2048;
    char buf[bufLen]; // file name of target file must fit in here

#ifdef _WIN32

    // ==> Windows:
    CNC_ASSERT( args == NULL || "getting program arguments not implemented for Windows" == NULL );

    int len = GetModuleFileNameA( 0, buf, bufLen ); // FIXME: Unicode?
    if ( len > 0 && len < bufLen ) {
        buf[len] = '\0';
        return std::string( buf );
    }
    return std::string();

#else

    // ==> Linux:
    pid_t myPID = getpid();

     // determine exe name from "/proc/<PID>/cmdline".
    char path[30];  // long enough for the string "/proc/<PID>/exe"
    sprintf( path, "/proc/%d/cmdline", myPID );
    FILE *cmdlineFile = fopen( path, "r" );
    std::string exeName;
    if ( cmdlineFile ) {
        int nChars = fread( buf, 1, sizeof(buf), cmdlineFile );
        if ( nChars > 0 ) {
            if ( static_cast< unsigned int >( nChars ) > sizeof(buf) - 1 ) {
                nChars = sizeof(buf) - 1;
            }
            buf[nChars] = '\0';
            exeName = buf;
            if( args ) {
                int s = -1;
                while( buf[++s] );
                ++s;
                for( int i = s; i < nChars; ++i ) if( buf[i] == 0 ) buf[i] = ' ';
                *args = &buf[s];
            }
        }
        fclose( cmdlineFile );
    }
    return exeName;

#endif
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

std::string PAL_getCWD()
{
    const int bufLen = 2048;
    char buf[bufLen]; // file name of target file must fit in here
    std::string result;
    char* cwd = getcwd( buf, bufLen - 1 );
    if ( cwd ) {
        result = cwd;
    }
    return result;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_SetEnvVar( const char* name, const char* value, int overwrite )
{
#ifdef _WIN32
    std::string envSpec( name );
    envSpec += "=";
    envSpec += value;
    _putenv( const_cast< char* >( envSpec.c_str() ) );
#else
    int ret = setenv( name, value, overwrite );
    CNC_ASSERT( ret == 0 );
#endif
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

} // namespace Internal
} // namespace CnC
