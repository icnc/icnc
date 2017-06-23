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

/**
 * @name Sockets
 *
 * This is a thin layer around the standard BSD socket calls.
 * It uses its own type for a socket handle, which is
 * mapped to a int (= file descriptor) on Unix and SOCKET
 * on Windows.
 *
 * The current source code location is passed to all functions
 * for debugging purposes. Errors are printed via PAL_Error/Warning() and
 * the return code indicates success, permanent failure
 * and timeouts.
 */

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


#ifndef _CNC_COMM_PAL_SOCKET_H_
#define _CNC_COMM_PAL_SOCKET_H_

#include "../generic_comm/pal_config.h"
// Windows:

#ifdef HAVE_WINSOCK_H
# include <winsock2.h>
#else
// Linux:
# include <netinet/in.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netdb.h>
#endif

#include <exception> // for PalSocketException
#include <utility>   // std::pair (in exception class ExceptionalFds)
#include <vector>
#include <string>
// in exception class ExceptionalFds


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

namespace CnC {
namespace Internal {

#ifdef HAVE_WINSOCK_H

/** make native socket type available as PAL_Socket_t */
typedef SOCKET PAL_NativeSocket_t;

/** an invalid value for PAL_Socket_t */
# define PAL_INVALID_NATIVE_SOCKET   INVALID_SOCKET

/** the value returned by e.g. bind() in case of an error */
# define PAL_SOCKET_FAILURE   SOCKET_ERROR

#else /* HAVE_WINSOCK_H */

typedef int PAL_NativeSocket_t;
#define PAL_INVALID_NATIVE_SOCKET -1
#define PAL_SOCKET_FAILURE   -1

#ifndef h_errno
extern int h_errno;
#endif

#endif /* HAVE_WINSOCK_H */

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/** wrapper struct for native socket */
typedef struct _PAL_Socket {
    PAL_NativeSocket_t socket;     /** native socket itself */
    UINT32 secret;                 /**< secret number that must be sent by the process connecting to a listen socket, see PAL_SOCKET_MAGIC */
} PAL_Socket;

typedef PAL_Socket* PAL_Socket_t;

#define PAL_INVALID_SOCKET NULL

/** the result of a socket function */
typedef enum _PAL_SockRes {
    PAL_SOCK_OK,      /**< no error */
    PAL_SOCK_ERR,     /**< error occured, was printed */
    PAL_SOCK_CLOSED,  /**< socket was closed by peer (this error is only returned if explicitly asked for) */
    PAL_SOCK_TIMEOUT  /**< a timeout occured, try again if you want to */
} PAL_SockRes_t;

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//%%%%%%%%%%%%%%%
//              %
//  Exceptions  %
//              %
//%%%%%%%%%%%%%%%

struct PalSocketException : public std::exception
{
    virtual const char * what () const throw() { return "PalSocketException"; }
};

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/// General connection error: something went wrong when using 
/// a communication connection (such as a socket)
struct ConnectionError : public PalSocketException
{
    virtual const char * what () const throw() { return "ConnectionError"; }
    ~ConnectionError() throw() {}
};

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/// socket operation timed out
struct Timeout : public PalSocketException
{
    virtual const char * what () const throw() { return "Timeout"; }
    ~Timeout() throw() {}
};

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Exception that may be thrown by PAL_Select:
 * it indicates which file descriptors are in an exceptional state.
 */
struct ErroneousFds : public PalSocketException
{
    /// Vector of erroneous socket during a PAL_Select
    /// Per item the socket identifier (of the read or write array) 
    /// is given, and its position in the read resp. write array
    /// of the PAL_Select call. 
    std::vector< std::pair< PAL_Socket_t, int /* position */ > > items;
    virtual const char * what () const throw() { return "ErroneousFds"; }
    ~ErroneousFds() throw() {}
};

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//                           %
//  PALSocket API functions  %
//                           %
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Init socket interface, unless done already.
 *
 * @return PAL_SOCK_OK or PAL_SOCK_ERR
 */
PAL_SockRes_t PAL_SockInit( const char *file, int line );

/**
 * Aborts any communication from now on till the next time PAL_SockInit()
 * is called. As the other functions block, PAL_SockAbort() can only be
 * called from another thread.
 *
 * This function does not wait for a successful abortion, instead it
 * returns immediately.
 */
void PAL_SockAbort( void );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Create a socket that listens for incoming connections
 * from any other machine.
 *
 * @param   procid    value will be included in contact string as prefix ("%d:...")
 * @param   backlog   minimum number of connections which will be accepted automatically
 *                    without having to call PAL_Accept()
 * @param   defhostname  a hostname which is to be used instead of gethostname(), if it resolves; can be NULL
 * @param   defport   0 or a port number which is to be used instead of searching for ports > 1024;
 * @retval  contact   a C string which tells a remote or local process to connect to
 *                    the new socket; does not contain spaces and is null terminated.
 * @retval  res       the socket for PAL_Accept()
 * @return  OK or ERR
 */
PAL_SockRes_t PAL_Listen( const char *file, int line, int procid,
                          int backlog, const char *defhostname, const UINT32 defport,
                          std::string & contact, PAL_Socket_t * res );

/**
 * Accept a connection on listen socket. Blocks until
 * the connection has been established, connection failed or timeout.
 *
 * @param   socket   as created by PAL_Listen()
 * @param   timeout  amount of seconds to wait, -1 means unlimited, 0 no wait at all
 * @retval  res      set to the new socket in case of OK
 * @return  OK, ERR, TIMEOUT
 */
PAL_SockRes_t PAL_Accept( const char *file, int line, PAL_Socket_t socket, FLOAT64 timeout, PAL_Socket_t *res );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Connect to a contact. Blocks until the other process performs
 * a PAL_Accept() or till the timeout
 *
 * @param   contact  the contact infos generated by the other process
 * @param   timeout  amount of seconds to wait, -1 means unlimited, 0 no wait at all
 * @retval  res      set to the new socket in case of OK
 * @return  OK, ERR, TIMEOUT
 */
PAL_SockRes_t PAL_Connect( const char *file, int line, const char *contact, FLOAT64 timeout, PAL_Socket_t *res );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Send data via a socket. Blocks until all data is on the way, or till
 * timeout.
 *
 * @param   socket   the socket as returned by PAL_Connect() or PAL_Accept()
 * @param   data     the data itself
 * @param   len      the number of bytes to send
 * @retval  sent     number of bytes sent (may be > 0 even in case of failure)
 * @param   timeout  amount of seconds to wait, -1 means unlimited, 0 no wait at all
 * @return  OK, ERR, TIMEOUT
 */
PAL_SockRes_t PAL_Send( const char *file, int line, PAL_Socket_t socket, const void *data, UINT32 len,
                        UINT32 *sent, FLOAT64 timeout );

/**
 * Receive data from socket. Blocks until specified amount has been
 * received or no more data is available.
 *
 * @param   socket    the socket as returned by PAL_Connect() or PAL_Accept()
 * @param   data      buffer for data
 * @param   len       expected amount of data
 * @retval  recvd     amount of received bytes (may be > 0 even in case of failure)
 * @param   eof_ok    if TRUE, then return PAL_SOCK_CLOSED if peer closes, else treat this as error
 * @return OK, ERR, TIMEOUT,
 */
PAL_SockRes_t PAL_Recv( const char *file, int line, PAL_Socket_t socket, void *data, UINT32 len,
                        UINT32 *recvd, FLOAT64 timeout, BOOL eof_ok );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Blocks until at least one of the listed sockets is ready for writing or reading.
 * When it returns successfully, then those array entries that are ready will have
 * been set to PAL_INVALID_SOCKET. It is okay to pass PAL_INVALID_SOCKET into
 * the function, so the same arrays can be used repeatedly.
 *
 * A socket created with PAL_Listen() is ready for reading when a connection request
 * is currently pending.
 *
 * @param   read       pointer to sockets that shall be ready for reading, may be NULL
 * @param   readnum    number of sockets in read array
 * @param   write      pointer to sockets that shall be ready for writing, may be NULL
 * @param   writenum   number of sockets in write array
 * @param   timeout    timout time, may be < 0 meaning "no timeout"
 * @return OK, ERR, or TIMEOUT
 */
PAL_SockRes_t PAL_Select( const char *file, int line,
                          PAL_Socket_t *read, UINT32 readnum,
                          PAL_Socket_t *write, UINT32 writenum,
                          FLOAT64 timeout );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/**
 * Close a socket. Never fails...
 *
 * @param   socket    the socket as returned by PAL_Connect() or PAL_Accept();
 *                    PAL_INVALID_SOCKET is okay
 */
void PAL_Close( const char *file, int line, PAL_Socket_t socket );

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

} // namespace Internal
} // namespace CnC

#endif // _CNC_COMM_PAL_SOCKET_H_

/*@} */
