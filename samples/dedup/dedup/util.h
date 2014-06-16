#ifndef X_UTIL_H
#define X_UTIL_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <zlib.h>
#include <openssl/sha.h>
//#include <netinet/in.h>

//#include <arpa/inet.h>
#include <sys/stat.h>
#include <math.h>

#include "queue.h"
#include "dedupdef.h"
#include "dedup_debug.h"
#include "hashtable.h"
#include "config.h"
#include "hashtable_private.h"

void * FindAllAnchors(void * args);
void Calc_SHA1Sig(const byte * buf, ulong num_bytes, u_char * digest);

int xread(int sd, void *buf, ulong len);
int xwrite(int sd, const void *buf, ulong len);
void print_body(send_body * body);
void print_head(send_head * head);


unsigned int hash_from_key_fn( void *k );

extern struct hashtable * cache;

#define METHOD_SOCKET 0
#define METHOD_FILE 1

extern int compress_way;

//extern struct queue *chunk_que, *anchor_que, *send_que, *compress_que;

/* for pointer <-> integer manipulation in tsearch() */
#define PTR_SIZE      (sizeof(void *))
#define PTR_BITLEN    (PTR_SIZE/2)
#define PTR_BITFLAG   (~(PTR_SIZE-1))

/* align a random integer to a ptr address boundary */
#define INT_TO_PTR(x) (((x) << PTR_BITLEN) & PTR_BITFLAG) 
#define PTR_TO_INT(x) (((x) >> PTR_BITLEN))

/* chunk size function */
typedef int32 (*CHUNKSIZE_FUNC)(u_char*, int32, int32);

struct DIGEST
{
  u_char digest[SHA1_LEN];
};
#define BI_INC_UNIT 1024

void print_anchor(data_chunk * anchor);
void print_chunk(data_chunk * chunk);

struct write_list * writelist_insert(struct write_list * list_head, u_int32 aid, u_int32 cid);


#endif //X_UTIL_H
