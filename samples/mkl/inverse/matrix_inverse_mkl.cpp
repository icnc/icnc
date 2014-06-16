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
#include <cstdlib>
#include <iostream>
#include <mkl_lapack.h>
#include "tbb/tick_count.h"


int main ( int argc, char * argv[] )
{
    if (argc == 2 && 0 != atoi(argv[1])) {
        std::cout << "Generating matrix of size " << argv[1] << std::endl;
    } else {
        std::cout << "Usage: matrix_inverse dim" << std::endl;
		return -1;
	}

    int n = atoi( argv[1] );
    double * A = new double[n*n];

    for( int i = 0; i < n; ++i ) {
        for( int j = 0; j < n; ++j ) {
            A[i*n+j] = double(rand())/RAND_MAX;
        }
    }

    tbb::tick_count t0 = tbb::tick_count::now();
    
    int * ipiv = new int[n];
    int info;
    dgetrf_( &n, &n, A, &n, ipiv, &info );
    // std::cout << "Matrix A after sgetrf\n";
    // for( int i=0; i < n*n; ++i ) {
    //     std::cout << A[i] << " ";
    // }
    // std::cout << "\ninfo for dgetrf_: " << info << std::endl;

    double _tmp; int _lwork = -1;
    dgetri_(&n, A, &n, ipiv, &_tmp, &_lwork, &info); // get optimal lwork
    _lwork = (int)_tmp;
    double * work = new double[_lwork];
    dgetri_(&n, A, &n, ipiv, work, &_lwork, &info);
    // std::cout << "Matrix A after sgetri\n";
    // for( int i=0; i < n*n; ++i ) {
    //     std::cout << A[i] << " ";
    // }
    // std::cout << "\ninfo for dgetri_: " << info << std::endl;

    delete work;
    delete ipiv;

    tbb::tick_count t1 = tbb::tick_count::now();
    std::cout <<  " Total Time: " << (t1-t0).seconds() << " sec" << std::endl;

    delete [] A;

    return(0);
}
