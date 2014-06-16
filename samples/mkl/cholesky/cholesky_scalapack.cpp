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
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <mkl_scalapack.h>
#include <mkl_blacs.h>
#include <mkl_pblas.h>

#define MAX(a,b) ((a)<(b)?(b):(a))

typedef MKL_INT DESC[9];

extern "C" int numroc_( int *, int *, int *, int *, int *);
extern "C" int descinit_( int *, int *, int *, int *, int *, 
                          int *, int *, int *, int *, int * );

extern void matrix_init( double *A, int n, const char *fname );

int main(int argc, char* argv[]) {
    if( argc != 4 && argc != 6 ) {
        std::cout << "Usage: " << argv[0] << " N BS np [-i <file>]" << std::endl;
        return 0;
    }
    MKL_INT matrix_size = static_cast<MKL_INT>(atoi(argv[1]));
    MKL_INT block_size = static_cast<MKL_INT>(atoi(argv[2]));
    MKL_INT nprow = static_cast<MKL_INT>(atoi(argv[3]));
    MKL_INT npcol = 1;
    while( nprow > npcol && nprow % 2 == 0 ) {
        nprow /= 2;
        npcol *= 2;
    }
    const char * fname = NULL;
    int argi = 4;
    while( argi < argc ) {
        if( ! strcmp( argv[argi], "-i" ) ) fname = argv[++argi];
        ++argi;
    }
    
    // Init BLACS
    MKL_INT i_one = 1, i_negone = -1, i_zero = 0;
    double zero = 0.0, one = 1.0;
    MKL_INT ictxt;

    MKL_INT myrow, mycol;
    MKL_INT mp, nq; // num rows, num cols local
    
    DESC descA, descA_distr;
    

    // get ctxt
    blacs_get_( &i_negone, &i_zero, &ictxt );
    // set up 2D nprow x npcol grid, row major 
    blacs_gridinit_( &ictxt, "R", &nprow, &npcol );
    // get my grid row and grid col
    blacs_gridinfo_( &ictxt, &nprow, &npcol, &myrow, &mycol );
    
    double * data = NULL;

    // init matrix on proc (0,0)
    if (myrow == 0 && mycol == 0) {
        std::cout << "Matrix Size: " << matrix_size << " x " << matrix_size << std::endl;
        data = new double[matrix_size*matrix_size];
        matrix_init( data, matrix_size, fname );
    }
        
    tbb::tick_count t0 = tbb::tick_count::now();

    // locr
    mp = numroc_( &matrix_size, &block_size, &myrow, &i_zero, &nprow );
    // locc
    nq = numroc_( &matrix_size, &block_size, &mycol, &i_zero, &npcol );

    //    std::cout << "mp = " << mp << std::endl;
    //    std::cout << "nq = " << nq << std::endl;
    
    double * A_distr = new double[mp*nq];
    MKL_INT lld = MAX( numroc_( &matrix_size, &matrix_size, &myrow, &i_zero, &nprow ), 1 );
    //    std::cout << "lld = " << lld << std::endl;

    MKL_INT info;
    descinit_( descA, &matrix_size, &matrix_size, &matrix_size, &matrix_size, 
               &i_zero, &i_zero, &ictxt, &lld, &info );

    MKL_INT lld_distr = MAX( mp, 1 );
    //    std::cout << "lld_distr = " << lld_distr << std::endl;

    descinit_( descA_distr, &matrix_size, &matrix_size, &block_size, &block_size,
               &i_zero, &i_zero, &ictxt, &lld_distr, &info );


    // distribute A to nodes
    pdgeadd_( "N", &matrix_size, &matrix_size, &one, data, &i_one, &i_one,
              descA, &zero, A_distr, &i_one, &i_one, descA_distr );

    pdpotrf( "L", &matrix_size, A_distr, &i_one, &i_one, descA_distr, &info );
    //    call pdpotrf(uplo, n, a, ia, ja, desca, info)
    
    // do something

    pdgeadd_( "N", &matrix_size, &matrix_size, &one, A_distr, &i_one, &i_one,
              descA_distr, &zero, data, &i_one, &i_one, descA );

    tbb::tick_count t1 = tbb::tick_count::now();
    if (myrow == 0 && mycol == 0) {
        std::cout << "Time for cholesky_scalapack( " << matrix_size << " ): " << (t1-t0).seconds() << std::endl;
    }

    delete [] A_distr;
    delete [] data;

    blacs_gridexit_( &ictxt );
    blacs_exit_( &i_zero );

    return 0;
}
