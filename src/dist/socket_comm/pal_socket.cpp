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
  see pal_socket.h
*/

#include "pal_config.h"
#include "pal_socket.h"

#include <cstring> // strerror
#include <cstdlib> // atoi  
#include <cstdio>
#include <cnc/internal/cnc_stddef.h>
#include <sstream>  // ostringstream

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

/**
 * Interface to Pallas utilities:
 */
#include "pal_util.h" // PAL_TimeOfDayClock, PAL_random, PAL_error/warning
#define VT_UINT16_MAX  ((UINT32)0xFFFF)
#define PAL_ENSURE_VALID( _buffer, _size )
#define STRNCPY( target, src ) do { strncpy(target, src, sizeof(target) - 1); (target)[sizeof(target) - 1] = 0; } while (0)
#define HERE __FILE__, __LINE__

/**
 * Windows:
 */
#ifdef HAVE_WINSOCK_H
# undef __GOT_SECURE_LIB__
        /* avoid redefining ( strcpy, strncpy, sprintf, strcat ) to
         * ( _strcpy_s, _strncpy_s, _sprintf_s, _strcat_s )
         * in <wspiapi.h> (included from <ws2tcpip.h>), because these functions
         * are not available in older compilers (MSVC .NET 2003). */
# include <ws2tcpip.h>  /* getaddrinfo */
# undef  errno
# define errno     WSAGetLastError()
# define h_errno   WSAGetLastError()
# ifndef ETIMEDOUT
#  define ETIMEDOUT WSAETIMEDOUT
# endif
# define PAL_SOCK_EAGAIN WSAEWOULDBLOCK
# define PAL_SOCK_EINTR  WSAEWOULDBLOCK
 // Config stuff:
# undef HAVE_INET_ATON
# undef HAVE_HSTRERROR
 // Linker stuff:
# pragma message( "--> automatic linking with ws2_32.lib" )
# pragma comment( lib, "ws2_32.lib" )
#endif

/**
 * Linux:
 */
#ifndef HAVE_WINSOCK_H
# include <unistd.h> // close
# include <sys/socket.h>
# include <sys/time.h>
# include <netinet/tcp.h>
 // might also be EWOULDBLOCK, but these usually have the same value anyway
# define PAL_SOCK_EAGAIN EAGAIN
# define PAL_SOCK_EINTR  EINTR
 // Config stuff:
# define HAVE_INET_ATON
# define HAVE_HSTRERROR
#endif

#define LOG_DEBUG(x) // x

namespace CnC {
namespace Internal {

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef HAVE_HSTRERROR

const char *hstrerror( int err )
{
    switch ( err ) {
        case HOST_NOT_FOUND:
            return "host not found";
            break;
        case TRY_AGAIN:
            return "try again";
            break;
        case NO_RECOVERY:
            return "no recovery";
            break;
        case NO_DATA:
            return "no data";
            break;
        default: {
            static char buffer[20];
            sprintf( buffer, "%d", err );
            return buffer;
            break;
        }
    }
    return "";
}

#endif

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef HAVE_INET_ATON

int inet_aton( const char *str, struct in_addr *addrptr )
{
    addrptr->s_addr = inet_addr( str );
    return 1;
}

#endif

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

static const char *PAL_SockErrStr( void )
{
    int err = errno; /* put into a local variable for debugging, errno may be a function call */

#ifdef HAVE_WINSOCK_H
    /* strerror() on Windows does not know WinSock error codes */
    switch ( err ) {
    case WSAEACCES:
        return "Permission denied.";
        break;
    case WSAEADDRINUSE:
        return "Address already in use.";
        break;
    case WSAEADDRNOTAVAIL:
        return "Cannot assign requested address.";
        break;
    case WSAEAFNOSUPPORT:
        return "Address family not supported by protocol family.";
        break;
    case WSAEALREADY:
        return "Operation already in progress.";
        break;
    case WSAECONNABORTED:
        return "Software caused connection abort.";
        break;
    case WSAECONNREFUSED:
        return "Connection refused.";
        break;
    case WSAECONNRESET:
        return "Connection reset by peer.";
        break;
    case WSAEDESTADDRREQ:
        return "Destination address required.";
        break;
    case WSAEFAULT:
        return "Bad address.";
        break;
    case WSAEHOSTDOWN:
        return "Host is down.";
        break;
    case WSAEHOSTUNREACH:
        return "No route to host.";
        break;
    case WSAEINPROGRESS:
        return "Operation now in progress.";
        break;
    case WSAEINTR:
        return "Interrupted function call.";
        break;
    case WSAEINVAL:
        return "Invalid argument.";
        break;
    case WSAEISCONN:
        return "Socket is already connected.";
        break;
    case WSAEMFILE:
        return "Too many open files.";
        break;
    case WSAEMSGSIZE:
        return "Message too long.";
        break;
    case WSAENETDOWN:
        return "Network is down.";
        break;
    case WSAENETRESET:
        return "Network dropped connection on reset.";
        break;
    case WSAENETUNREACH:
        return "Network is unreachable.";
        break;
    case WSAENOBUFS:
        return "No buffer space available.";
        break;
    case WSAENOPROTOOPT:
        return "Bad protocol option.";
        break;
    case WSAENOTCONN:
        return "Socket is not connected.";
        break;
    case WSAENOTSOCK:
        return "Socket operation on non-socket.";
        break;
    case WSAEOPNOTSUPP:
        return "Operation not supported.";
        break;
    case WSAEPFNOSUPPORT:
        return "Protocol family not supported.";
        break;
    case WSAEPROCLIM:
        return "Too many processes.";
        break;
    case WSAEPROTONOSUPPORT:
        return "Protocol not supported.";
        break;
    case WSAEPROTOTYPE:
        return "Protocol wrong type for socket.";
        break;
    case WSAESHUTDOWN:
        return "Cannot send after socket shutdown.";
        break;
    case WSAESOCKTNOSUPPORT:
        return "Socket type not supported.";
        break;
    case WSAETIMEDOUT:
        return "Connection timed out.";
        break;
#if 0
    case WSATYPE_NOT_FOUND:
        return "Class type not found.";
        break;
#endif
    case WSAEWOULDBLOCK:
        return "Resource temporarily unavailable.";
        break;
    case WSAHOST_NOT_FOUND:
        return "Host not found.";
        break;
#if 0
    case WSA_INVALID_HANDLE:
        return "Specified event object handle is invalid.";
        break;
#endif
#if 0
    case WSA_INVALID_PARAMETER:
        return "One or more parameters are invalid.";
        break;
#endif
#if 0
    case WSAINVALIDPROCTABLE:
        return "Invalid procedure table from service provider.";
        break;
#endif
#if 0
    case WSAINVALIDPROVIDER:
        return "Invalid service provider version number.";
        break;
#endif
#if 0
    case WSA_IO_INCOMPLETE:
        return "Overlapped I/O event object not in signaled state.";
        break;
#endif
#if 0
    case WSA_IO_PENDING:
        return "Overlapped operations will complete later.";
        break;
#endif
#if 0
    case WSA_NOT_ENOUGH_MEMORY:
        return "Insufficient memory available.";
        break;
#endif
    case WSANOTINITIALISED:
        return "Successful WSAStartup not yet performed.";
        break;
    case WSANO_DATA:
        return "Valid name, no data record of requested type.";
        break;
    case WSANO_RECOVERY:
        return "This is a non-recoverable error.";
        break;
#if 0
    case WSAPROVIDERFAILEDINIT:
        return "Unable to initialize a service provider.";
        break;
#endif
#if 0
    case WSASYSCALLFAILURE:
        return "System call failure.";
        break;
#endif
    case WSASYSNOTREADY:
        return "Network subsystem is unavailable.";
        break;
    case WSATRY_AGAIN:
        return "Non-authoritative host not found.";
        break;
    case WSAVERNOTSUPPORTED:
        return "WINSOCK.DLL version out of range.";
        break;
    case WSAEDISCON:
        return "Graceful shutdown in progress.";
        break;
#if 0
    case WSA_OPERATION_ABORTED:
        return "Overlapped operation aborted.";
        break;
#endif
    default:
        return strerror( err );
        break;
    }
#else // HAVE_WINSOCK_H
    return strerror( err );
#endif // HAVE_WINSOCK_H
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Disallow that any function accessing the socket blocks.
 */
static void PAL_SockNonblocking( PAL_Socket_t socket )
{
    if ( socket != PAL_INVALID_SOCKET ) {
#ifdef HAVE_WINSOCK_H
        unsigned long arg = 1;
        if ( ioctlsocket( socket->socket, FIONBIO, &arg ) ) {
            PAL_Warning( "ioctlsocket(FIONBIO): %s", PAL_SockErrStr() );
        }
#else
        if ( fcntl( socket->socket, F_SETFL, O_NONBLOCK ) == -1 ) {
            PAL_Warning( "fcntl(O_NONBLOCK): %s", PAL_SockErrStr() );
        }
#endif
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Send all data immediately (disable Nagle's algorithm)
 */
static void PAL_SockNodelay( PAL_Socket_t socket )
{
#ifndef HAVE_WINSOCK_H
    int nodelay = 1;
#else
    char nodelay = 1;
#endif

    int ret = setsockopt( socket->socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay) );
    CNC_ASSERT( ret == 0 );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Close the given socket
 */
static void PAL_CloseNative( PAL_NativeSocket_t nativesocket )
{
    if ( nativesocket != PAL_INVALID_NATIVE_SOCKET ) {
#ifdef HAVE_WINSOCK_H
        closesocket( nativesocket );
#else
        close( nativesocket );
#endif
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/** this is the global abort flag */
static bool PAL_SockAborted;

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * A magic UINT32 which must be sent from the connecting process to
 * the accepting one followed by the secret UINT32 which was embedded
 * in the contact string of the accepting process. Both must be sent in
 * network byte order.
 *
 * Together with a retry mechanism in PAL_Accept() this ensures that
 * arbitrary connection attempts by outside processes do not disturb
 * the application.
 */
static const UINT32 PAL_SOCKET_MAGIC = (('P' << 24) | ('L' << 16) | ('S' << 8) | ('K'));

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_SockInit( const char *file, int line )
{
#ifdef HAVE_WINSOCK_H
    WSADATA info;
    if ( WSAStartup( MAKEWORD(1,1), &info ) != 0 ) {
        PAL_Error( "Cannot open WinSock" );
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }
#endif

    PAL_SockAborted = FALSE;
    return PAL_SOCK_OK;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_SockAbort( void )
{
    PAL_Error( "aborting all communication..." );
    PAL_SockAborted = TRUE;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Listen( const char *file, int line, int procid,
                          int backlog, const char *defhostname, const UINT32 defport,
                          std::string & contact, PAL_Socket_t *res )
{
    char   myname[256];
    UINT32 portnum = defport ? defport : 1024, secretnum;
    const char *contact_addr;
    PAL_NativeSocket_t nativesocket = PAL_INVALID_NATIVE_SOCKET;
    int error = 1;
    BOOL haveaddr = FALSE;
    struct addrinfo *addr = NULL, *curr;
    PAL_SockRes_t sockres = PAL_SOCK_ERR;
    std::ostringstream myContactStr;

    *res = NULL;

    if ( defhostname ) {
        PAL_ENSURE_VALID( defhostname, strlen(defhostname) + 1 );
        error = getaddrinfo( defhostname, NULL, NULL, &addr );
        if ( !error ) {
            haveaddr = TRUE;
            STRNCPY( myname, defhostname );
        }
    }

    if ( ! haveaddr ) {
        if ( gethostname(myname, sizeof(myname) ) == -1 ) {
            PAL_Error( "cannot determine local host name: gethostname(): %s",
                       PAL_SockErrStr() );
            goto done;
        }
        error = getaddrinfo( myname, NULL, NULL, &addr );
        if ( error ) {
            PAL_Error( "cannot determine local host address: getaddrinfo(%s): %s",
                       myname, gai_strerror( error ) );
            goto done;
        }
    }

    nativesocket = socket(AF_INET, SOCK_STREAM, 0);        /* create the socket */
    if ( nativesocket == PAL_INVALID_NATIVE_SOCKET ) {
        PAL_Error( "cannot create socket: socket(): %s", PAL_SockErrStr() );
        goto done;
    }

    do {
        struct sockaddr_in localaddr;
        if( portnum != defport ) ++portnum;
        memset( &localaddr, 0, sizeof( localaddr ) );
        localaddr.sin_family = AF_INET;
        localaddr.sin_addr.s_addr = INADDR_ANY; /* allow connections from anyone */
        localaddr.sin_port = htons((short)portnum); /* try this as port number */
        if ( bind( nativesocket, (struct sockaddr *)&localaddr, sizeof(localaddr) ) != PAL_SOCKET_FAILURE ) {
            break;
        } else if( defport ) {
            PAL_Error( "cannot bind to port %u, last error was: bind(): %s", defport, PAL_SockErrStr() );
            goto done;
        }
    } while ( portnum < VT_UINT16_MAX && defport == 0 );

    if ( portnum == VT_UINT16_MAX ) {
        PAL_Error( "cannot bind to any port between 1024 and %u, last error was: bind(): %s",
                   VT_UINT16_MAX, PAL_SockErrStr() );
        goto done;
    }

    if ( listen( nativesocket, backlog ) == PAL_SOCKET_FAILURE ) {
        PAL_Error( "cannot listen on port %u: bind(): %s", portnum, PAL_SockErrStr() );
        goto done;
    }

    /*
     * Search for an address which is not "127.0.0.1".
     * If none is found, use the hostname - the other nodes then
     * must be able to resolve it.
     */
    contact_addr = NULL;
    for ( curr = addr; curr; curr = curr->ai_next  ) {
        /* only IP4 supported at the moment - this is an intentional simplification */
        if ( curr->ai_family == AF_INET ) {
            const char *curr_addr = inet_ntoa( ((struct sockaddr_in *)curr->ai_addr)->sin_addr );

            if ( strncmp( curr_addr, "127.0.", 6 ) ) { //, "127.0.0.1" ) && strcmp( curr_addr, "127.0.1.1" ) ) {
                contact_addr = curr_addr;
                break;
            }
        }
    }
    if ( !contact_addr ) {
        contact_addr = myname;
    }
    CNC_ASSERT( ! strchr( contact_addr, ' ' ) );
    secretnum = 111;//PAL_random();
    myContactStr << procid << ":" << portnum << "_" << secretnum << "@" << contact_addr;
    contact = myContactStr.str();

    /* setup wrapper struct */
    *res = new PAL_Socket;
    memset( *res, 0, sizeof( PAL_Socket ) );
    (*res)->socket = nativesocket;
    (*res)->secret = secretnum;
    sockres = PAL_SOCK_OK;

 done:
    if ( addr ) {
        freeaddrinfo( addr );
    }
    if ( sockres != PAL_SOCK_OK ) {
        PAL_CloseNative( nativesocket );
        delete *res;
        contact = std::string();
    }

    return sockres;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Accept( const char *file, int line, PAL_Socket_t socket, FLOAT64 timeout, PAL_Socket_t *res )
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    FLOAT64 endts = PAL_TimeOfDayClock() + timeout;
    PAL_NativeSocket_t nativesocket;

    if ( socket == PAL_INVALID_SOCKET ) {
        PAL_Error( "invalid socket" );
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }

    PAL_SockNonblocking( socket );

    do {
        PAL_Socket_t read;
        FLOAT64 deltats;

        /* try to connect, does not block even in case of timeout */
        nativesocket = accept( socket->socket, (struct sockaddr *)&addr, &addrlen );
        if ( nativesocket == PAL_INVALID_NATIVE_SOCKET ) {
            int err = errno; /* put into a local variable for debugging, errno may be a function call */
            if ( err != PAL_SOCK_EAGAIN && err != PAL_SOCK_EINTR ) {
                PAL_Warning( "could not establish connection, keep trying: accept(): %s",
                             PAL_SockErrStr() );
            }
        } else {
            UINT32 buffer[2];
            UINT32 recvd;
            PAL_SockRes_t recvres;

            /* setup wrapper struct */
            *res = new PAL_Socket;
            memset( *res, 0, sizeof( PAL_Socket ) );
            (*res)->socket = nativesocket;
            PAL_SockNodelay( *res );
            PAL_SockNonblocking( *res );

            /* additional check that an authorized process connected to us */
            recvres = PAL_Recv( HERE, *res, buffer, sizeof(buffer), &recvd,
                                timeout >= 0 ? (endts - PAL_TimeOfDayClock()) : -1,
                                FALSE );
            CNC_ASSERT( recvres != PAL_SOCK_OK || recvd == sizeof(buffer) );
            if ( recvres != PAL_SOCK_OK ||
                ntohl( buffer[0] ) != PAL_SOCKET_MAGIC ||
                ntohl( buffer[1] ) != socket->secret ) {
                PAL_Warning( "did not get authorization message, keep trying" );
                PAL_Close( HERE, *res );
                *res = NULL;
            } else {
                return PAL_SOCK_OK;
            }
        }

        if ( timeout == -1 ) {
            deltats = -1;
        } else {
            deltats = endts - PAL_TimeOfDayClock();
            if ( deltats <= 0 ) {
                throw( Timeout() );
                return PAL_SOCK_TIMEOUT;
            }
        }

        /* wait for socket connection request */
        read = socket;
        if ( PAL_Select( HERE, &read, 1, NULL, 0, deltats ) == PAL_SOCK_ERR ) {
            throw ConnectionError();
            return PAL_SOCK_ERR;
        }
    } while ( 1 );
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Connect( const char *file, int line, const char *contact, FLOAT64 timeout, PAL_Socket_t *res )
{
    struct addrinfo *addr = NULL;
    int error;
    UINT32 portnum, secretnum, buffer[2], sent;
    const char *port_addr;
    const char *contact_addr;
    const char *secret;
    PAL_NativeSocket_t nativesocket = PAL_INVALID_NATIVE_SOCKET;
    FLOAT64 ts = PAL_TimeOfDayClock();
    PAL_SockRes_t sockres = PAL_SOCK_ERR;

    *res = NULL;

    port_addr = strchr( contact, ':' );
    contact_addr = strchr( contact, '@' );
    secret = strchr( contact, '_' );
    if ( !port_addr || !contact_addr || !secret ) {
        PAL_Error( "malformed contact infos: %s", contact );
        goto done;
    }
    port_addr++;
    contact_addr++;
    secret++;
    portnum = atoi( port_addr );
    secretnum = (UINT32)strtoul( secret, NULL, 10 );

    nativesocket = socket(AF_INET, SOCK_STREAM, 0); /* create the socket */
    if ( nativesocket == PAL_INVALID_NATIVE_SOCKET ) {
        PAL_Error( "cannot create socket: socket(): %s",
        PAL_SockErrStr() );
        goto done;
    }

    PAL_ENSURE_VALID( contact_addr, strlen(contact_addr) + 1 );
    error = getaddrinfo( contact_addr, NULL, NULL, &addr );
    if ( error ) {
        PAL_Error( "cannot determine remote host address: getaddrinfo(%s): %s",
                   contact_addr, gai_strerror( error ) );
        goto done;
    }
    if ( !addr || !addr->ai_addr ) {
        PAL_Error( "name lookup of %s yielded no results", contact_addr );
        goto done;
    }

    /* note: a potential timeout cannot be passed to connect(), but at least we can retry if the parent chose no timeout */
    do {
        if ( addr->ai_family == AF_INET ) {
            ((struct sockaddr_in *)addr->ai_addr)->sin_port = htons( portnum );
        } else {
            CNC_ASSERT( addr->ai_family == AF_INET6 );
            ((struct sockaddr_in6 *)addr->ai_addr)->sin6_port = htons( portnum );
        }
        if ( connect( nativesocket, addr->ai_addr, (int)addr->ai_addrlen ) == PAL_SOCKET_FAILURE ) {
            if ( timeout == -1 && errno == ETIMEDOUT ) {
                continue;
            } else {
                PAL_Error( "cannot connect to %s: connect(): %s",
                           contact, PAL_SockErrStr() );
                goto done;
            }
        } else {
            break;
        }
    } while ( timeout <= 0 || ts + timeout > PAL_TimeOfDayClock() );

    /* create wrapper struct, set socket attributes */
    *res = new PAL_Socket;
    memset( *res, 0, sizeof( PAL_Socket ) );
    (*res)->socket = nativesocket;
    PAL_SockNodelay( *res );
    PAL_SockNonblocking( *res );

    /* send magic message */
    buffer[0] = htonl( PAL_SOCKET_MAGIC );
    buffer[1] = htonl( secretnum );
    if ( PAL_Send( HERE, *res, buffer, sizeof(buffer), &sent, -1 ) != PAL_SOCK_OK ) {
        PAL_Close( HERE, *res );
        *res = NULL;
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }
    CNC_ASSERT( sent == sizeof(buffer) );

    sockres = PAL_SOCK_OK;

 done:
    if ( addr ) {
        freeaddrinfo( addr );
    }
    if ( sockres != PAL_SOCK_OK ) {
        PAL_CloseNative( nativesocket );
        delete *res;
    }

    return sockres;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Send( const char *file, int line, PAL_Socket_t socket, const void *data, UINT32 len,
                        UINT32 *sent, FLOAT64 timeout )
{
    if ( socket == PAL_INVALID_SOCKET ) {
        PAL_Error( "invalid socket" );
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }

    if ( len == 0 ) {
        *sent = 0;
        return PAL_SOCK_OK;
    }

    FLOAT64 endts = PAL_TimeOfDayClock() + timeout;

    PAL_ENSURE_VALID( data, len );

    *sent = 0;
    LOG_DEBUG( PAL_DebugPrintf( file, line, "PAL_Send >%d:", socket->socket ) );
    do {
        PAL_Socket_t set;
        FLOAT64 deltats;
        UINT32 left = len - *sent;

        LOG_DEBUG( PAL_DebugPrintf( file, line,
                                    " len %u - sent %u = %u left",
                                    len, *sent, left ) );

        /* if there's no need to do anything, then return immediately */
        if ( left == 0 ) {
            return PAL_SOCK_OK;
        }

        /* try to write either buffer or data */
        int cur = send( socket->socket, ((char *)data) + *sent, left, 0 );
        LOG_DEBUG( PAL_DebugPrintf( file, line,
                                    " sent %u bytes of data, res %d",
                                    left, cur ) );
        switch ( cur ) {
#if 1
            case 0:
                PAL_Error( "connection closed by peer, sending remaining %u of %u bytes failed",
                           len - *sent, len );
                throw ConnectionError();
                return PAL_SOCK_ERR;
                break;
#endif
            case -1:
                if ( errno == PAL_SOCK_EAGAIN || errno == PAL_SOCK_EINTR ) {
                    LOG_DEBUG( PAL_DebugPrintf( file, line, " try again" ) );
                    cur = 0;
                } else {
                    PAL_Error( "sending remaining %u of %u bytes failed: send(): %s",
                               len - *sent, len, PAL_SockErrStr() );
                    throw ConnectionError();
                    return PAL_SOCK_ERR;
                }
                break;
            default:
                CNC_ASSERT( cur >= 0 );
                break;
        }

        *sent += cur;
        if ( *sent == len ) {
            LOG_DEBUG( PAL_DebugPrintf( file, line, " sent all data" ) );
            return PAL_SOCK_OK;
        }

        if ( timeout == -1 ) {
            deltats = -1;
        } else {
            deltats = endts - PAL_TimeOfDayClock();
            if ( deltats <= 0 ) {
                /* timeout */
                break;
            }
        }

        /* wait for ready socket */
        set = socket;
        if ( PAL_Select( HERE, NULL, 0, &set, 1, deltats ) == PAL_SOCK_ERR ) {
            LOG_DEBUG( PAL_DebugPrintf( file, line, " select failed" ) );
            throw ConnectionError();
            return PAL_SOCK_ERR;
        }
    } while ( 1 );

    LOG_DEBUG( PAL_DebugPrintf( file, line, " timeout" ) );

    throw( Timeout() ); 
    return PAL_SOCK_TIMEOUT;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Recv( const char *file, int line, PAL_Socket_t socket, void *data, UINT32 len,
                        UINT32 *recvd, FLOAT64 timeout, BOOL eof_ok )
{
    FLOAT64 endts = PAL_TimeOfDayClock() + timeout;

    if ( socket == PAL_INVALID_SOCKET ) {
        PAL_Error( "invalid socket" );
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }

    /* the accept must not block under any circumstances */
    PAL_SockNonblocking( socket );

    *recvd = 0;
    do {
        PAL_Socket_t set;
        FLOAT64 deltats;
        int cur;

        /* if there's no need to do anything, then return immediately */
        if ( *recvd == len ) {
            return PAL_SOCK_OK;
        }

        /* try to read */
        cur = recv( socket->socket, ((char *)data) + *recvd, len - *recvd, 0 );
        LOG_DEBUG( PAL_DebugPrintf( file, line, " PAL_Recv <%d got %d", socket->socket, cur ) );
        switch ( cur ) {
#if 1
            case 0:
                if ( eof_ok ) {
                    return PAL_SOCK_CLOSED;
                } else {
                    PAL_Error( "connection closed by peer, receiving remaining %u of %u bytes failed",
                               len - *recvd, len );
                    throw ConnectionError();
                    return PAL_SOCK_ERR;
                }
                break;
#endif
            case -1:
                if ( errno == PAL_SOCK_EAGAIN || errno == PAL_SOCK_EINTR ) {
                    LOG_DEBUG( PAL_DebugPrintf( file, line, " try again" ) );
                    cur = 0;
                } else {
                    PAL_Error( "receiving remaining %u of %u bytes failed: recv(): %s",
                               len - *recvd, len, PAL_SockErrStr() );
                    throw ConnectionError();
                    return PAL_SOCK_ERR;
                }
                break;
            default:
                CNC_ASSERT( cur >= 0 );
                break;
        }
        *recvd += cur;

        if ( *recvd == len ) {
            return PAL_SOCK_OK;
        }

        if ( timeout == -1 ) {
            deltats = -1;
        } else {
            deltats = endts - PAL_TimeOfDayClock();
            if ( deltats <= 0 ) {
                /* timeout */
                break;
            }
        }

        /* wait for ready socket */
        set = socket;
        if ( PAL_Select( HERE, &set, 1, NULL, 0, deltats ) == PAL_SOCK_ERR ) {
            throw ConnectionError();
            return PAL_SOCK_ERR;
        }
    } while ( TRUE );

    throw( Timeout() );
    return PAL_SOCK_TIMEOUT;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

PAL_SockRes_t PAL_Select( const char *file, int line,
                          PAL_Socket_t *read, UINT32 readnum,
                          PAL_Socket_t *write, UINT32 writenum,
                          FLOAT64 timeout )
{
    /* using fd sets might break on Windows, sorry... */
    fd_set readset, writeset, errset;
    fd_set *preadset = NULL, *pwriteset = NULL;
    struct timeval tv;
    UINT32 i;
    int maxfd = -1;
    BOOL maxfdset = FALSE;
    BOOL fdsizeexceeded = FALSE;

    LOG_DEBUG( PAL_DebugPrintf( HERE, "%f PAL_Select: %s:%d... ", PAL_TimeOfDayClock(), file, line ) );

restart:
    // ==> First action: check whether we should abort the socket layer ...
    if ( PAL_SockAborted ) {
        LOG_DEBUG( PAL_DebugPrintf( HERE, "aborted\n" ) );
        return PAL_SOCK_ERR;
    }

    // ==> Iniitalize the file descriptors. First the READ array:
    FD_ZERO( &errset );
    if ( readnum > 0 ) {
        FD_ZERO( &readset );
#ifdef HAVE_WINSOCK_H
        /* On Windows, FD_SETSIZE has a different meaning. */
        if ( readnum >= FD_SETSIZE ) {
            readnum = FD_SETSIZE;
            fdsizeexceeded = TRUE;
        }
#endif
        for ( i = 0; i < readnum; i++ ) {
            if ( read[i] != PAL_INVALID_SOCKET ) {
                LOG_DEBUG( PAL_DebugPrintf( HERE, "<%d ", read[i]->socket ) );
#ifndef HAVE_WINSOCK_H
                if ( read[i]->socket >= FD_SETSIZE ) {
                    fdsizeexceeded = TRUE;
                } else
#endif
                FD_SET( read[i]->socket, &readset );
                FD_SET( read[i]->socket, &errset );
                if ( !maxfdset || (int)read[i]->socket > maxfd ) {
                    maxfd = static_cast< int >( read[i]->socket );
                    maxfdset = TRUE;
                }
            }
        }
        preadset = &readset;
    }
    
    // ==> Continue initialization of file descriptors, now WRITE stuff:
    if ( writenum > 0 ) {
        FD_ZERO( &writeset );
#ifdef HAVE_WINSOCK_H
        if ( writenum >= FD_SETSIZE ) {
            writenum = FD_SETSIZE;
            fdsizeexceeded = TRUE;
        }
#endif
        for ( i = 0; i < writenum; i++ ) {
            if ( write[i] != PAL_INVALID_SOCKET ) {
                LOG_DEBUG( PAL_DebugPrintf( HERE, ">%d ", write[i]->socket ) );
#ifndef HAVE_WINSOCK_H
                if ( write[i]->socket >= FD_SETSIZE ) {
                    fdsizeexceeded = TRUE;
                } else
#endif
                FD_SET( write[i]->socket, &writeset );
                FD_SET( write[i]->socket, &errset );
                if ( !maxfdset || (int)write[i]->socket > maxfd ) {
                    maxfd = static_cast< int >( write[i]->socket );
                    maxfdset = TRUE;
                }
            }
        }
        pwriteset = &writeset;
    }

    // ==> Prepare timeout data structure
    if ( timeout > 0 ) {
        tv.tv_sec = static_cast<long>( timeout );
        tv.tv_usec = static_cast<long>( ( timeout - tv.tv_sec ) * 1e6 );
    } else if ( timeout == 0.0 ) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    } else {
        /* even if our caller wants us to wait forever, we still have to wake up
           regularly to check PAL_SockAborted */
        tv.tv_sec = 1;
        tv.tv_usec = 0;
    }

    // ==> Debug Messsage
    LOG_DEBUG( PAL_DebugPrintf( HERE, "maxfdset %s (%d) timeout %f -> %u.%06u -> ",
                                maxfdset ? "yes" : "no", maxfd,
                                timeout,
                                tv.tv_sec,
                                tv.tv_usec ) );

    // ==> Check whether there is at least one valid socket:
    if ( ! maxfdset ) {
        LOG_DEBUG( PAL_DebugPrintf( HERE, "no sockets\n" ) );
        PAL_Error( "PAL_Select called without any valid socket" );
        throw ConnectionError();
        return PAL_SOCK_ERR;
    }

    // ==> !!! NOW THE ACTUAL select() SYSTEM CALL !!!
    maxfd = select( maxfd + 1, preadset, pwriteset, &errset, &tv );
    LOG_DEBUG( PAL_DebugPrintf( HERE, "%f ", PAL_TimeOfDayClock() ) );
    
    // ==> Analyze return value of select call:
    switch ( maxfd ) {
        case 0:
            if ( fdsizeexceeded ) {
                /* we don't really know what the status for the fds is 
                   that couldn't be stored in the fd_set,
                   pretend that we were successful */
                break;
            } else if ( timeout >= 0.0 ) {
                LOG_DEBUG( PAL_DebugPrintf( HERE, "timeout\n" ) );
                throw( Timeout() );
                return PAL_SOCK_TIMEOUT;
            } else {
                /* check PAL_SockAborted, then try again */
                goto restart;
            }
            break;
        case -1:
            switch ( errno ) {
                case EINTR:
                    LOG_DEBUG( PAL_DebugPrintf( HERE, "interrupted call, retry\n" ) );
                    goto restart;
                    break;
                default:
                    LOG_DEBUG( PAL_DebugPrintf( HERE, "%s\n", PAL_SockErrStr() ) );
                    PAL_Error( "cannot wait on sockets: select(): %s",
                               PAL_SockErrStr() );
                     // Collect all erroneous sockets and put them into an exception object:
                    ErroneousFds errSockets;
                    for ( int i = 0; i < (int)readnum; ++i ) {
                        if ( read[i] != PAL_INVALID_SOCKET && FD_ISSET( read[i]->socket, &errset ) ) {
                            errSockets.items.push_back( std::make_pair( read[i], i ) );
                        }
                    }
                    for ( int i = 0; i < (int)writenum; ++i ) {
                        if ( write[i] != PAL_INVALID_SOCKET && FD_ISSET( write[i]->socket, &errset ) ) {
                            errSockets.items.push_back( std::make_pair( write[i], i ) );
                        }
                    }
                    throw errSockets;
                    return PAL_SOCK_ERR;
            }
            break;
        default:
            /* number of ready sockets, ignore it */
            break;
    }

    // Remove those sockets from arrays which are ready,
    // leaving the rest for another PAL_Select() call
    for ( i = 0; i < readnum; i++ ) {
        if ( read[i] != PAL_INVALID_SOCKET && FD_ISSET( read[i]->socket, &readset ) ) {
            read[i] = PAL_INVALID_SOCKET;
        }
    }
    for ( i = 0; i < writenum; i++ ) {
        if ( write[i] != PAL_INVALID_SOCKET && FD_ISSET( write[i]->socket, &writeset ) ) {
            write[i] = PAL_INVALID_SOCKET;
        }
    }

    LOG_DEBUG( PAL_DebugPrintf( HERE, "done\n") );
    return PAL_SOCK_OK;
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

void PAL_Close( const char *file, int line, PAL_Socket_t socket )
{
    if ( socket ) {
        PAL_CloseNative( socket->socket );
        delete socket;
    }
}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

} // namespace Internal
} // namespace CnC

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * A P P E N D I X
 *
 * Just for debugging:
 *   - a debug server (compile with -DDEBUG_SERVER_MAIN)
 *   - a debug client (compile with -DDEBUG_CLIENT_MAIN)
 * When started, the server prints out its contact string.
 * This string must be specified as command line argument to the client.
 */

//%%%%%%%%%%%%%%%%%
//                %
//  Debug server  %
//                %
//%%%%%%%%%%%%%%%%%

#ifdef DEBUG_SERVER_MAIN

#include <iostream>
#include "pal_socket.h"
#include <cstring> // memcmp
using namespace std;
using namespace CnC;
using namespace CnC::Internal;
//#define HERE __FILE__, __LINE__

int main()
{
    PAL_SockInit( HERE );

    std::string contact;
    PAL_Socket_t socket0;

    char* CnCSocketHostname = getenv( "CNC_SOCKET_HOSTNAME" );
    char* CnCSocketPort = getenv( "CNC_SOCKET_PORT" );
    unsigned int defPort = ( CnCSocketPort ) ? atoi( CnCSocketPort ) : 0;
    
    PAL_SockRes_t retListen =
        PAL_Listen( HERE, 0, 10, CnCSocketHostname, defPort, contact, &socket0 );

    //std::cout << "retListen = " << retListen << std::endl;
    std::cout << "contact string: " << contact << std::endl;

    PAL_Socket_t socket1;
    PAL_SockRes_t retAccept = PAL_Accept( HERE, socket0, -1, &socket1 );

    //std::cout << "retAccept = " << retAccept << std::endl;

    PAL_Close( HERE, socket0 );

    // ==> Receive something:
    const int refArr[] = { 11, 22, 33 };
    int arr[3] = { 0, 0, 0 };
    CnC::Internal::UINT32 len = sizeof( arr );
    CnC::Internal::UINT32 nReceived;
    PAL_SockRes_t retRecv = PAL_Recv( HERE, socket1, arr, len, &nReceived, -1, false );
    //std::cout << "retRecv = " << retRecv << std::endl;
    //std::cout << "nReceived = " << nReceived << std::endl;
    bool okay1 = false;
    if (    retRecv == PAL_SOCK_OK
         && nReceived == len
         && memcmp( arr, refArr, sizeof( arr ) ) == 0 ) {
        okay1 = true;
    }

    // ==> Send something back:
    CnC::Internal::UINT32 nSent = 0;
    PAL_SockRes_t retSend = PAL_Send( HERE, socket1, refArr, len, &nSent, -1 );
    bool okay2 = true;
    if ( retSend != PAL_SOCK_OK || nSent != len ) {
        okay2 = false;
    }

    // ==> Print result:
    if ( okay1 && okay2 ) {
        std::cout << "!! Connection set up successfully !!" << std::endl;
    }

    PAL_Close( HERE, socket1 );
}

#endif // DEBUG_SERVER_MAIN

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//%%%%%%%%%%%%%%%%%
//                %
//  Debug client  %
//                %
//%%%%%%%%%%%%%%%%%

#ifdef DEBUG_CLIENT_MAIN

#include <iostream>
#include "pal_socket.h"
#include <cstring> // memcmp
using namespace std;
using namespace CnC;
using namespace CnC::Internal;
//#define HERE __FILE__, __LINE__

int main( int argc, char** argv )
{
    if ( argc < 2 ) {
        std::cerr << "add contact string as command line argument\n";
        return 1;
    }

    PAL_SockInit( HERE );

    std::string contact;
    PAL_Socket_t socket;

    PAL_SockRes_t retConnect = PAL_Connect( HERE, argv[1], -1, &socket );

    //std::cout << "retConnect = " << retConnect << std::endl;

    // ==> Send something:
    int refArr[] = { 11, 22, 33 };
    CnC::Internal::UINT32 len = sizeof( refArr );
    CnC::Internal::UINT32 nSent = 0;
    PAL_SockRes_t retSend = PAL_Send( HERE, socket, refArr, len, &nSent, -1 );
    bool okay1 = true;
    if ( retSend != PAL_SOCK_OK || nSent != len ) {
        okay1 = false;
    }

    // ==> Receive something in response:
    int arr[3] = { 0, 0, 0 };
    CnC::Internal::UINT32 nReceived;
    PAL_SockRes_t retRecv = PAL_Recv( HERE, socket, arr, len, &nReceived, -1, false );
    bool okay2 = false;
    if (    retRecv == PAL_SOCK_OK
         && nReceived == len
         && memcmp( arr, refArr, sizeof( arr ) ) == 0 ) {
        okay2 = true;
    }

    // ==> Print result:
    if ( okay1 && okay2 ) {
        std::cout << "!! Connection set up successfully !!" << std::endl;
    }

    PAL_Close( HERE, socket );

    return 0;
}

#endif // DEBUG_CLIENT_MAIN

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
