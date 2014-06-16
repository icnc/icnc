//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#ifdef USE_MKL
#include <mkl_cblas.h>
#else
#include "tbb/parallel_for.h"
#endif

void posdef_gen( double * A, int n )
{
	/* Allocate memory for the matrix and its transpose */
	double *L;
	double *LT;
    double two = 2.0;
	double one = 1.0;
    //	srand( 1 );

	L = (double *) calloc(sizeof(double), n*n);
	assert (L);

	LT = (double *) calloc(sizeof(double), n*n);
	assert (LT);

    memset( A, 0, sizeof( double ) * n * n );
	
	/* Generate a conditioned matrix and fill it with random numbers */
    for(int j = 0; j < n; j++) {
        for(int k = 0; k <= j; k++) {
			if(k<j) {
				// The initial value has to be between [0,1].
				L[k*n+j] = ( ( (j*k) / ((double)(j+1)) / ((double)(k+2)) * two) - one ) / ((double)n);
			} else if (k == j) {
				L[k*n+j] = 1;
            }
		}
	}

	/* Compute transpose of the matrix */
	for(int i = 0; i < n; i++) {
		for(int j = 0; j < n; j++) {
			LT[j*n+i] = L[i*n+j];
		}
	}

#ifdef USE_MKL
    cblas_dgemm( CblasColMajor, CblasNoTrans, CblasNoTrans, n, n, n, 1, L, n, LT, n, 0, A, n );
#else
    tbb::parallel_for( 0, n, 1, [&]( int i ) {
            //for (int i = 0; i < n; i++) {
            double * _a = &A[i*n];
            double * _l = &L[i*n];
            for (int k = 0; k < n; k++) {
                double * _lt = &LT[k*n];
                for (int j = 0; j < n; j++) {
                    _a[j] += _l[k] * _lt[j];
                }
            }
        } );
#endif
    
    free (L);
    free (LT);
}

// Read the matrix from the input file
void matrix_init( double *A, int n, const char *fname )
{
    if( fname ) {
        int i;
        int j;
        FILE *fp;
        
        fp = fopen(fname, "r");
        if(fp == NULL) {
            fprintf(stderr, "\nFile does not exist\n");
            exit(0);
        }
        for (i = 0; i < n; i++) {
            for (j = 0; j <= i; j++) {
                if( fscanf(fp, "%lf ", &A[i*n+j]) <= 0) {   
                    fprintf(stderr,"\nMatrix size incorrect %i %i\n", i, j);
                    exit(0);
                }
                if( i != j ) {
                    A[j*n+i] = A[i*n+j];
                }
            }
        }
        fclose(fp);
    } else {
        posdef_gen( A, n );
    }
}

// write matrix to file
void matrix_write ( double *A, int n, const char *fname )
{
    if( fname ) {
        int i;
        int j;
        FILE *fp;
        
        fp = fopen(fname, "w");
        if(fp == NULL) {
            fprintf(stderr, "\nCould not open file %s for writing.\n", fname );
            exit(0);
        }
        for (i = 0; i < n; i++) {
            for (j = 0; j <= i; j++) {
                fprintf( fp, "%lf ", A[i*n+j] );
            }
            fprintf( fp, "\n" );
        }
        fclose(fp);
    }
}
