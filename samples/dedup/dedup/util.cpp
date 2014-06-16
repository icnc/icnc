#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "config.h"
#include "queue.h"
#include "dedupdef.h"
#include "util.h"

int compress_way;
struct hashtable * cache;

void Calc_SHA1Sig(const byte* buf, ulong num_bytes, u_char * digest)
{
    SHA_CTX sha;
 
    SHA1_Init(&sha);
    SHA1_Update(&sha, buf, (unsigned long)num_bytes);
    SHA1_Final(digest, &sha);
#if 0
    char b[4096];
    sprintf(b, "buf(%x) num(%d) d(%d,%d,%d,...)\n", 
	    buf, num_bytes,buf[0],buf[1],buf[2]);
    printf("%s",b);
    fflush(stdout);

    int *p = (int *)digest;
    sprintf(b, "digest(%d,%d,%d,%d,%d)\n", 
	    p[0],p[1],p[2],p[3],p[4]);
    printf("%s",b);
    fflush(stdout);
#endif
}

int
xread(int sd, void *buf, ulong len)
{
  char *p = (char *)buf;
  ulong nrecv = 0;
  size_t rv;
  
  while (nrecv < len) {
    rv = READ(sd, p,(unsigned int)(len - nrecv));
    //printf("  xread request(%d) read(%d)\n", (len-nrecv),rv);
    //write(stdout, p, len - nrecv);
    if (0 > rv && errno == EINTR)
      continue;
    if (0 > rv)
      return -1;
    if (0 == rv)
      return 0;
    nrecv += rv;
    p += rv;
  }
  return (int)nrecv;
}

int
xwrite(int sd, const void *buf, ulong len)
{
  char *p = (char *)buf;
  ulong nsent = 0;
  //ssize_t rv;
  ulong rv = 0;
  
  while (nsent < len) {
    rv = WRITE(sd, p, (unsigned int)(len - nsent)); 

#if 0
    long t = _tell(sd);
    printf("  xwrite request(%d) written(%d) tell(%d)\n", (unsigned int)(len-nsent),(unsigned int)rv,(unsigned int)t);
    if (t < 500) {
      for (int i = 0; i < (len-nsent); i++) {
	printf("%2.2x ", (unsigned)p[i]);
        if (i > 0 && i % 32 == 0) printf("\n");
      }
      printf("\n");
    }

    //if (t > 1187000) {
      //}
#endif
    if (0 > rv && (errno == EINTR || errno == EAGAIN))
      continue;
    if (0 > rv)
      return -1;
    nsent += rv;
    p += rv;
  }
  return (int)nsent;
}

unsigned int
hash_from_key_fn( void *k ){
  int i = 0;
  int hash = 0;
  unsigned char* sha1name = ((CacheKey*)k)->sha1name;

  for (i = 0; i < SHA1_LEN; i++) {
    hash += *(((unsigned char*)sha1name) + i);
  }
  return hash;
}

void print_anchor(data_chunk * anchor) {
  printf("len %ld, anchorid %d\n", anchor->len, anchor->anchorid);
}

void print_chunk(data_chunk * chunk) {
  printf("len %ld, cid %d\n", chunk->len, chunk->cid); 
}

//for 64-bit data types, remove if instrumentation no longer needed
#ifndef _WIN32
#include "stdint.h"
#endif
//#include <inttypes.h>
static uint64_t l1_num = 0;
static uint64_t l1_scans = 0;
static uint64_t l2_num = 0;
static uint64_t l2_scans = 0;

void dump_scannums() {
  /*printf("Number L1 scans: %"PRIu64"\n", l1_num);
  printf("Number L1 elements touched on average: %"PRIu64"\n", l1_scans/l1_num);
  printf("Number L2 scans: %"PRIu64"\n", l2_num);
  printf("Number L2 elements touched on average: %"PRIu64"\n", l2_scans/l2_num);
  */
}

struct write_list * writelist_insert(struct write_list * list_head, u_int32 aid, u_int32 cid) {
  struct write_list * p, * pnext = NULL, *panchornext = NULL, *prenew, *tmp;
  if (list_head == NULL) {
    list_head = (struct write_list *) malloc(sizeof(struct write_list));
    pnext = NULL;
    p = list_head;
    panchornext = p;
  } else {
    if (aid < list_head->anchorid || (aid == list_head->anchorid && cid < list_head->cid)) {
      p = (struct write_list *) malloc(sizeof(struct write_list));
      p->next = list_head;
      if (aid < list_head->anchorid) {
        p->anchornext = p;
      } else {
        p->anchornext = list_head->anchornext;
      }
      return p;
    } else {
      p = list_head;

      l1_num++;
      while (p->anchornext->next != NULL && aid > p->anchornext->next->anchorid) {
        p = p->anchornext->next;
        l1_scans++;
      }
      panchornext = p->anchornext;
      l2_num++;
      while (p->next != NULL && (aid > p->next->anchorid || (aid == p->next->anchorid && cid > p->next->cid))) {
        p = p->next;
        l2_scans++;
      }
      tmp = p;
      pnext = p->next;   
      p->next = (struct write_list *) malloc(sizeof(struct write_list));
      p = p->next;
      if (aid == tmp->anchorid && cid > tmp->cid && (pnext == NULL || pnext->anchorid >aid)) {
        prenew = list_head;
        while (aid > 0 && prenew->anchorid < aid-1) {
          prenew= prenew->anchornext->next;
        }
        while (prenew != NULL && prenew != p && prenew->anchorid == aid) {
          prenew->anchornext = p;
          prenew = prenew->next;
        } 
        panchornext = p;
      }
      if (pnext != NULL && pnext->anchorid == aid) {
        panchornext = pnext->anchornext;
      }
      if (aid > tmp->anchorid && (pnext == NULL || pnext->anchorid > aid)) {
        panchornext = p;
      }
    }
  }

  p->next = pnext;
  p->anchornext = panchornext;

  return p;
} 
