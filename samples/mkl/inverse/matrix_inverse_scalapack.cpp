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
#include <cstdlib>
#include <iostream>
#include <mkl_scalapack.h>
#include <mkl_blacs.h>
#include <mkl_pblas.h>

#define MAX(a,b) ((a)<(b)?(b):(a))

typedef MKL_INT DESC[9];

MKL_INT matrix_size;
MKL_INT block_size;
MKL_INT nprow, npcol; // num process rows/cols
MKL_INT lld, lld_distr;
MKL_INT info;

extern "C" int numroc_( int *, int *, int *, int *, int *);
extern "C" int descinit_( int *, int *, int *, int *, int *, 
                          int *, int *, int *, int *, int * );

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cout << "Usage: ./mklinverse Size BlockSize np " << std::endl;
        return 0;
    }

    matrix_size = static_cast<MKL_INT>(atoi(argv[1]));
    block_size = static_cast<MKL_INT>(atoi(argv[2]));
    MKL_INT nprow = static_cast<MKL_INT>(atoi(argv[3]));
    MKL_INT npcol = 1;
    while( nprow > npcol && nprow % 2 == 0 ) {
        nprow /= 2;
        npcol *= 2;
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
    
    double * data;

    // init matrix on proc (0,0)
    if (myrow == 0 && mycol == 0) {
        std::cout << "Matrix Size: " << matrix_size << " x " << matrix_size << std::endl;
        data = new double[matrix_size*matrix_size];
        
        double (*matrix)[matrix_size] = (double (*)[matrix_size]) data;
        
        for (int i = 0; i < matrix_size; i++) {
            for (int j = 0; j < matrix_size; j++) {
                double e = 0.0;
                if ((i < matrix_size) & (j < matrix_size)) {
                    e = static_cast<double>(rand())/RAND_MAX;
                }
                else if (i == j) {
                    std::cout << "NEVER GONNA HAPPEN" << std::endl;
                    e = 1.0;
                }
                else {
                    e = 0.0;
                }
                matrix[j][i] = e;
            }
        }
    } else {
        data = 0;
    }
    
    // locr
    mp = numroc_( &matrix_size, &block_size, &myrow, &i_zero, &nprow );
    // locc
    nq = numroc_( &matrix_size, &block_size, &mycol, &i_zero, &npcol );

    std::cout << "mp = " << mp << std::endl;
    std::cout << "nq = " << nq << std::endl;
    
    tbb::tick_count t0 = tbb::tick_count::now();

    double * A_distr = new double[mp*nq];
    lld = MAX( numroc_( &matrix_size, &matrix_size, &myrow, &i_zero, &nprow ), 1 );
    std::cout << "lld = " << lld << std::endl;

    descinit_( descA, &matrix_size, &matrix_size, &matrix_size, &matrix_size, 
               &i_zero, &i_zero, &ictxt, &lld, &info );

    lld_distr = MAX( mp, 1 );
    std::cout << "lld_distr = " << lld_distr << std::endl;

    descinit_( descA_distr, &matrix_size, &matrix_size, &block_size, &block_size,
               &i_zero, &i_zero, &ictxt, &lld_distr, &info );


    // distribute A to nodes
    pdgeadd_( "N", &matrix_size, &matrix_size, &one, data, &i_one, &i_one,
              descA, &zero, A_distr, &i_one, &i_one, descA_distr );

    MKL_INT * ipiv = new MKL_INT[mp+block_size];
    pdgetrf_( &matrix_size, &matrix_size, A_distr, &i_one, &i_one, 
              descA_distr, ipiv, &info );
 
    
    MKL_INT lwork = mp * block_size; 
    MKL_INT liwork = nq + block_size;
    double * work = new double[lwork];
    MKL_INT * iwork = new MKL_INT[liwork];
    
    pdgetri_( &matrix_size, A_distr, &i_one, &i_one, descA_distr, ipiv,
              work, &lwork, iwork, &liwork, &info );
    //              a, ia, ja, desca, ipiv, work, lwork, iwork, liwork, info)

    // do something

    pdgeadd_( "N", &matrix_size, &matrix_size, &one, A_distr, &i_one, &i_one,
              descA_distr, &zero, data, &i_one, &i_one, descA );

    /*
    tbb::tick_count tstart, tend;

    tstart = tbb::tick_count::now();

    LAPACKE_dgetrf( LAPACK_ROW_MAJOR, matrix_size, matrix_size, data, matrix_size, ipiv );
    LAPACKE_dgetri( LAPACK_ROW_MAJOR, matrix_size, data, matrix_size, ipiv );

    tend = tbb::tick_count::now();

    
    double time = (tend-tstart).seconds();

    std::cout << "Time: " << time << std::endl;

    delete data;
*/
    delete work;
    delete iwork;
    delete ipiv;

    tbb::tick_count t1 = tbb::tick_count::now();
    if (myrow == 0 && mycol == 0) {
        std::cout <<  " Total Time: " << (t1-t0).seconds() << " sec" << std::endl;
    }

    delete A_distr;
    
    blacs_gridexit_( &ictxt );
    blacs_exit_( &i_zero );

    // {
    //     double (*matrix)[matrix_size] = (double (*)[matrix_size]) data;
    //     for (int i = 0; i < matrix_size; i++) {
    //         for (int j = 0; j < matrix_size; j++) {
    //             std::cout << matrix[j][i] << " ";
    //         }
    //         std::cout << std::endl;
    //     }
    // }
    return 0;
}
