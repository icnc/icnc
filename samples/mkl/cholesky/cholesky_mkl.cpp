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
#include <tbb/tick_count.h>
#include <mkl_lapacke.h>
#include <mkl.h>
#include <omp.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <mkl_lapack.h>
#include <cstdio>
#include <cassert>
#include <mkl_cblas.h>

// Read the matrix from the input file
extern void matrix_init( double *A, int n, const char *fname );

#define MAX(a,b) ((a)<(b)?(b):(a))

typedef MKL_INT DESC[9];

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 4) {
        std::cout << "Usage: " << argv[0] << " N [-i <file>]" << std::endl;
        return 0;
    }

    mkl_set_num_threads( 1 );
    omp_set_num_threads( 1 );
    
    MKL_INT matrix_size= static_cast<MKL_INT>(atoi(argv[1]));    
    const char * fname = NULL;
    int argi = 2;
    while( argi < argc ) {
        if( ! strcmp( argv[argi], "-i" ) ) fname = argv[++argi];
        ++argi;
    }

    double * data = new double[matrix_size*matrix_size];
    matrix_init( data, matrix_size, fname );
    
    tbb::tick_count t0 = tbb::tick_count::now();

    MKL_INT info;
    dpotrf_( "L", &matrix_size, data, &matrix_size, &info );
    
    tbb::tick_count t1 = tbb::tick_count::now();
#if 0
    for( int i = 0; i < matrix_size; ++i ) {
        for( int j = 0; j < matrix_size; ++ j ) {
                fprintf(stdout,"%lf ", data[j*matrix_size+i] );
        }
        fprintf(stdout,"\n");
    }
#endif
    std::cout << "Time for cholesky_mkl( " << matrix_size << " ): " << (t1-t0).seconds() << std::endl;

    delete [] data;

    return 0;
}
