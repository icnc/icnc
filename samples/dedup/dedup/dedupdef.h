#ifndef _DEDUPDEF_H_
#define _DEDUPDEF_H_

#include <sys/types.h>
#ifndef _WIN32
#include "stdint.h"
#endif
#include "config.h"

#ifdef PARALLEL
#include <pthread.h>
#endif //PARALLEL

#define CHECKBIT 123456

#define MAX_THREADS 100
#define GAP 100000
/* Define USED macro */
//#define USED(x) { ulong y __attribute__ ((unused)); y = (ulong)x; }

/*-----------------------------------------------------------------------*/
/* type definition */
/*-----------------------------------------------------------------------*/

typedef uint8_t u_char;
//typedef uint64_t u_long;
typedef uint64_t ulong;
typedef uint32_t u_int;

typedef uint8_t      byte;
typedef byte               u_int8;
typedef uint16_t     u_int16;
typedef uint32_t       u_int32;
typedef uint64_t u_int64;

typedef uint64_t u64int;
typedef uint32_t      u32int;
typedef uint8_t      uchar;
typedef uint16_t     u16int;

#if !defined( WIN32 ) && !defined( _WIN32 )
typedef int8_t        int8;
#endif
typedef int16_t       int16;
typedef int32_t        int32;
typedef int64_t   int64;

typedef u_int32            u_int32_t;

/*-----------------------------------------------------------------------*/
/* random signature denoting DEDUP formats */
/*-----------------------------------------------------------------------*/
#define DEDUP_SIGNATURE (0xfedd)

/*-----------------------------------------------------------------------*/
/* definition of current version */
/*-----------------------------------------------------------------------*/
#define CUR_VERSION  (0x0001)

/*-----------------------------------------------------------------------*/
/* hashing methods */
/*-----------------------------------------------------------------------*/

#define MT_HASH_SHA1 (0x0000)  /* SHA-1 signature */

/* SHA1 length is 20 bytes(160 bites) */
#define SHA1_LEN  20

/*-----------------------------------------------------------------------*/
/* compression methods */
/*-----------------------------------------------------------------------*/

#define MT_COMP_PLAIN (0x0000) /* no compression */
#define MT_COMP_GZIP  (0x0001) /* GZIP */

/* below not implemented */
#define MT_COMP_BZIP  (0x0002)
#define MT_COMP_ZIP   (0x0003)

/*-----------------------------------------------------------------------*/
/* encryption methods */
/*-----------------------------------------------------------------------*/
#define MT_ENCR_PLAIN (0x0000) /* no encryption */


/*-----------------------------------------------------------------------*/
/* useful macros */
/*-----------------------------------------------------------------------*/
#ifndef NELEM
#define NELEM(x) (sizeof(x)/sizeof(x[0]))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef TRUE
#define TRUE  1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef MEMORY_FREE
#define MEMORY_FREE(x) if(x) { free(x); (x) = NULL;}
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE  0100000
#endif

#define EXT       ".ddp"           /* extension */ 
#define EXT_LEN   (sizeof(EXT)-1)  /* extention length */
#define MAX_BSIZE (64*1024)        /* maximum block size */


#define COPY_SHA1(dest, src)   memcpy(dest, src, SHA1_LEN)
#define COMP_SHA1(dest, src)   memcmp(dest, src, SHA1_LEN)

/*----------------------------------------------------------------------*/

// the structure for a data chunk
typedef struct {
  u_char * start; //anchor string pointer
  ulong len; //anchor length
  u_int32 anchorid; //first level id
  u_int32 cid;    //second level id
  u_char sha1[SHA1_LEN];
} data_chunk ;

#define LEN_FILENAME 100

//the input queue for SendBlock or Compress
typedef struct {
  u_char type; //whether this chunk is a fingerprint or compressed data
  u_char * str;
  u_char * content; //data
  u_char * sha1; //SHA-1
} send_buf_item;

//file header
typedef struct {
  char filename[LEN_FILENAME]; //filename
  u_int32 fid;  //file id (reserved variable, not used now)
} send_head;

typedef struct {
  u_int32 fid;  //file id
  u_int32 anchorid; //first level id
  u_int32 cid; //second level id
  ulong len;
}send_body;

struct chunk_list {
  struct chunk_list * next;
  ulong len;
  u_int32 cid;
  u_char * content;
};

//for reordering
struct write_list {
  struct write_list * next; //next item in the list
  struct write_list * anchornext; //next item in terms of first level id
  u_int32 cid; //second level id
  u_int32 anchorid; //first level id
  ulong len;
  u_char * content;
  u_char type;
};

#define TYPE_FINGERPRINT 0
#define TYPE_COMPRESS 1
#define TYPE_ORIGINAL 2
#define TYPE_HEAD 3
#define TYPE_FINISH 4

#define QUEUE_SIZE 1024UL*1024

#define NUM_BUF_CHECKCACHE 1024UL*1024
#define NUM_BUF_DECOMPRESS 1024UL*1024
#define NUM_BUF_REASSEMBLE 1024UL*1024


#define PORT 12340


#define MAXBUF (600*1024*1024)     /* 128 MB for buffers */
#define ANCHOR_JUMP (2*1024*1024) //best for all 2*1024*1024

#define MAX_PER_FETCH 10000

#define ITEM_PER_FETCH 20
#define ITEM_PER_INSERT 20

#define CHUNK_ANCHOR_PER_FETCH 20
#define CHUNK_ANCHOR_PER_INSERT 20

#define ANCHOR_DATA_PER_FETCH 1
#define ANCHOR_DATA_PER_INSERT 1

//for cache
typedef struct{
  u_char sha1name[SHA1_LEN];
}CacheKey;


typedef struct {
  char infile[LEN_FILENAME];
  char outfile[LEN_FILENAME];
  int method;
  int preloading;
  int nthreads;
  u_int32 b_size;
  int compress_way;  // for Nikolay
  int input_fd;
  int output_fd;
} config;
typedef config * CONFIG;

#define TAG_OCCUPY 0
#define TAG_DATAREADY 1
#define TAG_WRITTEN 2 

struct pContent{
  ulong len;
  u_char * content;
  u_int count;
  u_char tag;
#ifdef PARALLEL
  pthread_cond_t empty;
#endif //PARALLEL
};

#define COMPRESS_GZIP 0
#define COMPRESS_BZIP2 1
#define COMPRESS_NONE 2

#define UNCOMPRESS_BOUND 10000000

#endif
