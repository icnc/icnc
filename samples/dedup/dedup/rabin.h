#ifndef _RABIN_H
#define _RABIN_H

//#include <endian.h>
#include "dedupdef.h"

typedef struct R128 R128;
typedef struct R256 R256;

struct R128 {
	u64int	hash[2];
};

struct R256 {
	u64int	hash[4];
};

enum {
	NWINDOW		= 32,
	MinSegment	= 1024,
	RabinMask	= 0xfff	/* must be less than <= 0x7fff */
};

void		rabininit(int);
u32int		rabinhash32(uchar*, int);
u32int		rabinhash32u2(uchar*, int);
u32int		rabinhash32u4(uchar*, int);

int		rabinseg(uchar*, int, int);
int		rabinseg2(uchar*, int, int);
int		rabinseg4(uchar*, int, int);

#endif //_RABIN_H
