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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include "tbb/tick_count.h"
#include "tbb/parallel_for.h"
#include "cholesky_types.h"
#include "cholesky.h"
#include <cassert>
#ifdef USE_MKL
#include <omp.h>
#include <mkl.h>
#endif

extern void cholesky (double *A, const int n, const int b, const char * oname, dist_type dt);
extern void posdef_gen( double * A, int n );
// Read the matrix from the input file
extern void matrix_init (double *A, int n, const char *fname);
// write matrix to file
extern void matrix_write ( double *A, int n, const char *fname );

int main (int argc, char *argv[])
{
#ifdef _DIST_
    CnC::dist_cnc_init< cholesky_context > dc_init;
#endif
    int n;
    int b;
    dist_type dt = BLOCKED_ROWS;
    const char *fname = NULL;
    const char *oname = NULL;
    const char *mname = NULL;
    int argi;

    // Command line: cholesky n b filename [out-file]
    if (argc < 3 || argc > 7) {
        fprintf(stderr, "Incorrect number of arguments, epxected N BS [-i infile] [-o outfile] [-w mfile] [-dt disttype]\n");
        return -1;
    }
    argi = 1;
    n = atol(argv[argi++]);
    b = atol(argv[argi++]);
    while( argi < argc ) {
        if( ! strcmp( argv[argi], "-o" ) ) oname = argv[++argi];
        else if( ! strcmp( argv[argi], "-i" ) ) fname = argv[++argi];
        else if( ! strcmp( argv[argi], "-w" ) ) mname = argv[++argi];
        else if( ! strcmp( argv[argi], "-dt" ) ) dt = static_cast< dist_type >( atoi( argv[++argi] ) );
        ++argi;
    }

#ifdef USE_MKL
    if( mname == NULL ) {
        mkl_set_num_threads( 1 );
        omp_set_num_threads( 1 );
    }
#endif

    if(n % b != 0) {
        fprintf(stderr, "The tile size is not compatible with the given matrix\n");
        exit(0);
    }

    double * A = new double[n*n];

    matrix_init( A, n, fname );
    if( mname ) matrix_write( A, n, mname );
    else cholesky(A, n, b, oname, dt);     

    delete [] A;

    return 0;
}
