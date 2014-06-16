#ifndef CONFIG_H
#define CONFIG_H

//Define to 1 to enable parallelization with pthreads
//#define PARALLEL 1

#define CNC 1

//Define to desired number of threads per queues
//The total number of queues between two pipeline stages will be
//greater or equal to #threads/MAX_THREADS_PER_QUEUE
#define MAX_THREADS_PER_QUEUE 4

#ifdef _WIN32

#define _CRT_SECURE_NO_WARNINGS 1
#include <io.h>

typedef unsigned __int8 uint8_t;
#if !defined( WIN32 ) && !defined( _WIN32 )
typedef __int8 int8_t;
#endif
typedef unsigned __int16 uint16_t;
typedef __int16 int16_t;
typedef unsigned __int32 uint32_t;
typedef __int32 int32_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

#define READ( _f, _b, _s ) _read( _f, _b, _s )
#define WRITE( _f, _b, _s ) _write( _f, _b, _s )
#define OPEN( _f, _l, _p ) _open( _f, _l, _p )
#define CLOSE( _f ) _close( _f )


#else

#include <unistd.h>

#define READ( _f, _b, _s ) read( _f, _b, _s )
#define WRITE( _f, _b, _s ) write( _f, _b, _s )
#define OPEN( _f, _l, _p ) open( _f, _l, _p )
#define CLOSE( _f ) close( _f )

#endif

#endif//CONFIG_H
