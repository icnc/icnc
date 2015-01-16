/************************************************************************
* posumm_cannon/kernel.cpp: This file is part of the POSUMM project.
*
* POSUMM: Portable OSU Matrix-Multiply, CnC implementations.
*
* Copyright (C) 2014,2015 Ohio State University, Columbus, Ohio
*
* This program can be redistributed and/or modified under the terms
* of the license specified in the LICENSE.txt file at the root of the
* project.
*
* Contact: Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>
*
*
* @file: posumm_cannon/kernel.cpp
* @author: Martin Kong <kongm@cse.ohio-state.edu>
************************************************************************/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ceild
#define ceild(x,y) (((x) > 0)? (1 + ((x) - 1)/(y)): ((x) / (y)))
#endif

#ifndef floord
#define floord(x,y) (((x) > 0)? ((x)/(y)): 1 + (((x) -1)/ (y)))
#endif

#ifndef max
#define max(x,y)    ((x) > (y)? (x) : (y))
#endif

#ifndef min
#define min(x,y)    ((x) < (y)? (x) : (y))
#endif

// K-dimension of L1 tiles are unrolled by 4. Acceptable values
// for this implementation are 1-4.
#define UF 4


/*********************************************************************
 *
 * Block-multiply kernel: used by the block-multiply step.
 * Tile for L3 and L1. Si, Sj and Sk are the L3 tile sizes, 
 * whereas Ti, Tj and Tk are L1 tile sizes
 *
 *********************************************************************/
void mm_kernel_l3_tiled 
(int n, float * A, float * B, float * C, int Si, int Sj, int Sk, int Ti, int Tj, int Tk)
{
  int ns = floord((n + -1), Si*Ti);
  for (int c1 = 0; c1 <= ns; c1++) 
    for (int c2 = 0; c2 <= floord((n + -1), Sj*Tj); c2++) 
      for (int c3 = 0; c3 <= floord((n + -1), Sk*Tk); c3++) 

        for (int c4 = Ti*c1; c4 <= min(floord(n-1,Si),Ti*c1 + Ti-1); c4++)
          for (int c5 = Tj*c2; c5 <= min(floord(n-1,Sj),Tj*c2 + Tj-1); c5++)
            for (int c6 = Tk*c3; c6 <= min(floord(n-1,Sk),Tk*c3 + Tk-1); c6++)
  {
        for (int c7 = Si*c4; c7 <= min(n-1,Si*c4+Si-1); c7++)
        #if (UF>1)
          for (int c9 = Sk*c6; c9 + (UF-1) < min(n,Sk*c6+Sk); c9+=UF)
        #else
          for (int c9 = Sk*c6; c9 <= min(n-1,Sk*c6+Sk-1); c9++)
        #endif
            #pragma simd 
            #pragma vector
            #pragma ivdep
            for (int c8 = Sj*c5; c8 <= min(n-1,Sj*c5+Sj-1); c8++)
            {
              C[c7 * n + c8] += A[c7 * n + c9+0] * B[(c9+0) * n + c8];
              #if (UF>=2)
              C[c7 * n + c8] += A[c7 * n + c9+1] * B[(c9+1) * n + c8];
              #endif
              #if (UF>=3)
              C[c7 * n + c8] += A[c7 * n + c9+2] * B[(c9+2) * n + c8];
              #endif
              #if (UF>=4)
              C[c7 * n + c8] += A[c7 * n + c9+3] * B[(c9+3) * n + c8];
              #endif
            }
  }
}
