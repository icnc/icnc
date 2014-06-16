#include <assert.h>
#include <stdlib.h>
#ifndef _WIN32
#include "stdint.h"
#endif

#include "dedupdef.h"
#include "rabin.h"

#undef PRINT

/* Functions to compute rabin fingerprints */

u32int *rabintab = 0;
u32int *rabintab1 = 0;
u32int *rabintab2 = 0;
u32int *rabintab3 = 0;
u32int *rabinwintab = 0;
//static u32int irrpoly = 0x759ddb9f;
static u32int irrpoly = 0x45c2b6a1;

u_int32_t
bswap32(u_int32_t x)
{
        return  ((x << 24) & 0xff000000 ) |
                 ((x <<  8) & 0x00ff0000 ) |
                 ((x >>  8) & 0x0000ff00 ) |
                 ((x >> 24) & 0x000000ff );
}

static u32int
fpreduce(u32int x, u32int irr)
{
    int i;

    for(i=32; i!=0; i--){
        if(x >> 31){
            x <<= 1;
            x ^= irr;
        }else
            x <<= 1;
    }
    return x;
}

static void
fpmkredtab(u32int irr, int s, u32int *tab)
{
    u32int i;

    for(i=0; i<256; i++)
        tab[i] = fpreduce(i<<s, irr);
    return;
}

static u32int
fpwinreduce(u32int irr, int winlen, u32int x)
{
    int i;
    u32int winval;

    winval = 0;
    winval = ((winval<<8)|x) ^ rabintab[winval>>24];
    for(i=1; i<winlen; i++)
        winval = (winval<<8) ^ rabintab[winval>>24];
    return winval;
}

static void
fpmkwinredtab(u32int irr, int winlen, u32int *tab)
{
    u32int i;

    for(i=0; i<256; i++)
        tab[i] = fpwinreduce(irr, winlen, i);
    return;
}

void
rabininit(int winlen)
{
    rabintab = (u32int*)malloc(256*sizeof rabintab[0]);
    rabintab1 = (u32int*)malloc(256*sizeof rabintab[0]);
    rabintab2 = (u32int*)malloc(256*sizeof rabintab[0]);
    rabintab3 = (u32int*)malloc(256*sizeof rabintab[0]);
    rabinwintab = (u32int*)malloc(256*sizeof rabintab[0]);
    fpmkredtab(irrpoly, 0, rabintab);
    fpmkredtab(irrpoly, 8, rabintab1);
    fpmkredtab(irrpoly, 16, rabintab2);
    fpmkredtab(irrpoly, 24, rabintab3);
    fpmkwinredtab(irrpoly, winlen, rabinwintab);
    return;
}

u32int
rabinhash32(uchar *p, int n)
{
    uchar *e;
    u32int h, x;

    h = 1;
    e = p+n;
    while(p<e){
        x = h >> 24;
        h <<= 8;
        h |= *p++;
        h ^= rabintab[x];
    }
    return h;
}

u32int
rabinhash32u2(uchar *p, int n)
{
    u32int h, t;
    u32int x0, x1;
    u16int *d, *e;

    assert((n&1) == 0);

    h = 1;
    d = (u16int*)p;
    e = (u16int*)(p+n);
    while(d<e){
        t = *d++;
        x1 = h>>24;
        x0 = (h>>16) & 0xff;
        h <<= 16;
        h |= (t>>8)|((t&0xff)<<8);
        h ^= rabintab[x0];
        h ^= rabintab1[x1];
    }
    return h;
}

u32int
rabinhash32u4(uchar *p, int n)
{
    u32int h;
    u32int h0, t;
    u32int *d, *e;

#ifndef __i386__
    u32int y0, y1;
    u32int y2, y3;
#endif

    assert((n&2) == 0);

    h = 1;
    d = (u32int*)p;
    e = (u32int*)(p+n);
    while(d<e){
#ifndef __i386__
        t = *d++;
        y0 = t >> 24;
        y1 = (t >> 16) & 0xff;
        y2 = (t >> 8) & 0xff;
        y3 = t & 0xff;
#else
        //t = __bswap32(*d);
        t = bswap32(*d);
        d++;
        //t = *d++;     /* can swap tables instead */
#endif
        h0 = h;
#ifndef __i386__
        h = y0|(y1<<8)|(y2<<16)|(y3<<24);
#else
        h = t;
#endif
        h ^= rabintab[h0 & 0xff];
        h ^= rabintab1[(h0>>8) & 0xff];
        h ^= rabintab2[(h0>>16) & 0xff];
        h ^= rabintab3[h0 >> 24];
    }
    return h;
}

int
rabinseg(uchar *p, int n, int winlen)
{
    int i;
    u32int h;
    u32int x;

    //USED(winlen);
    if(n < NWINDOW)
        return n;

    h = 0;
    for(i=0; i<NWINDOW; i++){
        x = h >> 24;
        h = (h<<8)|p[i];
        h ^= rabintab[x];
    }
    if((h & RabinMask) == 0)
        return i;
    while(i<n){
        x = p[i-NWINDOW];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        if((h & RabinMask) == 0)
            return i;
    }
    return n;
}

int
rabinseg2(uchar *p, int n, int winlen)
{
    int i, j;
    u32int h;
    u32int x;
    u32int t0, t1;

    //USED(winlen);
    if(n < NWINDOW)
        return n;

    h = 0;
    for(i=0; i<NWINDOW; i++){
        x = h >> 24;
        h = (h<<8)|p[i];
        h ^= rabintab[x];
    }
    if((h & RabinMask) == 0)
        return i;
    j = 0;
    while(i<n){
        x = p[j++];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        t0 = h;
        x = p[j++];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        t1 = h;
        if((t0 & RabinMask) == 0)
            return i-1;
        if((t1 & RabinMask) == 0)
            return i;
    }
    return n;
}

int
rabinseg4(uchar *p, int n, int winlen)
{
    int i;
    u32int h, x;
    u32int x0, x1;
    u32int v0, v1;
    u32int v2, v3;
    u32int *d, *e;

    //USED(winlen);
    if(n < NWINDOW)
        return n;

    h = 0;
    for(i=0; i<NWINDOW; i++){
        x = h >> 24;
        h <<= 8;
        h |= p[i];
        h ^= rabintab[x];
    }

    /* get ourselves aligned */
    switch(((uintptr_t)p) & 3){     /* OK since winlen&3 == 0 */
    case 1:                 /* 1 -> 3 to get aligned */
        if((h & RabinMask) == 0)
            return NWINDOW+1;
        x = p[i-NWINDOW];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        /* fall through */

    case 2:
        if((h & RabinMask) == 0)
            return NWINDOW+2;
        x = p[i-NWINDOW];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        /* fall through */

    case 3:
        if((h & RabinMask) == 0)
            return NWINDOW+2;
        x = p[i-NWINDOW];
        h ^= rabinwintab[x];
        x = h >> 24;
        h <<= 8;
        h |= p[i++];
        h ^= rabintab[x];
        /* fall through */

    case 0:;
        /* nada */
    }

    if((h & RabinMask) == 0)
        return i+1;

//  winlen /= 4;
    d = (u32int*)(p+i);
    e = (u32int*)(p+n);
    while(d<e){
        x0 = d[-(NWINDOW/4)];
        x1 = d[0];
        d++;

#undef doit
#define doit(round)             \
        x = x0 & 0xff;          \
        x0 >>= 8;           \
        h ^= rabinwintab[x];        \
        x = h >> 24;            \
        h <<= 8;            \
        h |= x1 & 0xff;         \
        x1 >>= 8;           \
        h ^= rabintab[x];       \
        v ## round = h;

        doit(0);
        doit(1);
        doit(2);
        doit(3);

        if((v0 & RabinMask) == 0 ||
           (v1 & RabinMask) == 0 ||
           (v2 & RabinMask) == 0 ||
           (v3 & RabinMask) == 0)
           goto Done;
    }

    return n;

  Done:
    if((v0 & RabinMask) == 0)
        return int(((uchar*)d)-p+1);
    if((v1 & RabinMask) == 0)
        return int(((uchar*)d)-p+2);
    if((v2 & RabinMask) == 0)
        return int(((uchar*)d)-p+3);
    return int(((uchar*)d)-p+4);
}
