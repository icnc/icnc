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
#include <cassert>
#include <cstdio>
#include <cmath>
#include <tbb/tick_count.h>

#include "cholesky_types.h"
#include "cholesky.h"

#ifdef USE_MKL
#include <mkl_cblas.h>
#include "mkl_lapack.h"
#include "mkl.h"
#endif

// Perform unblocked Cholesky factorization on the input block (item indexed by tag values).
// Output is a lower triangular matrix.
int S1_compute::execute(const int & t, cholesky_context & c ) const
{
    tile_const_ptr_type A_block;

    int b = c.b;
    const int k = t;

    c.Lkji.get(triple(k,k,k), A_block); // Get the input tile.


#ifdef USE_MKL
    char uplo = 'l';
    int info;
	dpotf2(&uplo, &b, const_cast< double * >( A_block->get_array() ), &b, &info);
    tile_const_ptr_type L_block = A_block;
#else
    // Allocate memory for the output tile.
    tile_ptr_type L_block = std::make_shared< tile_type >( b );
    // FIXME this need to be a triangular tile only
    // for(int i = 0; i < b; i++) {
    //     L_block[i] = (double *) malloc((i+1) * sizeof(double));
    //     if(L_block[i] == NULL) {
    //         fprintf(stderr, "Malloc failed -> %d\n", i);
    //         exit(0);
    //     }
    // }
    for(int k_b = 0; k_b < b; k_b++) {
        if((*A_block)( k_b, k_b ) <= 0) {
            fprintf(stderr, "Not a symmetric positive definite (SPD) matrix\n");
            exit(0);
        } 
        (*L_block)( k_b, k_b ) = sqrt((*A_block)( k_b, k_b ));
        for(int j_b = k_b+1; j_b < b; j_b++) {
            (*L_block)( j_b, k_b ) = (*A_block)( j_b, k_b )/(*L_block)( k_b, k_b );
        }
        for(int j_bb = k_b+1; j_bb < b; j_bb++) {
            for(int i_b = j_bb; i_b < b; i_b++) {
                const_cast< tile_type & >(*A_block)( i_b, j_bb ) = (*A_block)( i_b, j_bb ) - ((*L_block)( i_b, k_b ) * (*L_block)( j_bb, k_b ));
            }
        }
    }
#endif

    c.Lkji.put(triple(k+1, k, k), L_block);   // Write the output tile at the next time step.
    
    return CnC::CNC_Success;
}

// Perform triangular system solve on the input tile. 
// Input to this step are the input tile and the output tile of the previous step.
int S2_compute::execute(const pair & t, cholesky_context & c ) const
{
    tile_const_ptr_type A_block;
    tile_const_ptr_type Li_block;
    
    int b = c.b;
    const int k = t.first;
    const int j = t.second;

    assert( j != k );
    c.Lkji.get(triple(k,j,k), A_block); // Get the input tile.
    c.Lkji.get(triple(k+1, k, k), Li_block);    // Get the 2nd input tile (Output of previous step).

#ifdef USE_MKL
    char uplo='l', side='r', transa='t', diag='n';
	double alpha = 1;
	dtrsm (&side,&uplo,&transa,&diag,&b,&b,&alpha,Li_block->get_array(),&b,const_cast< double * >( A_block->get_array() ),&b);
    tile_const_ptr_type Lo_block = A_block;
#else    
    // Allocate memory for the output tile.
    tile_ptr_type Lo_block = std::make_shared< tile_type >( b );
    for(int k_b = 0; k_b < b; k_b++) {
        for(int i_b = 0; i_b < b; i_b++) {
            (*Lo_block)( i_b, k_b ) = (*A_block)( i_b, k_b )/(*Li_block)( k_b, k_b );
        }
        for( int j_b = k_b+1; j_b < b; j_b++) {
            for( int i_b = 0; i_b < b; i_b++) {
                const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) - ((*Li_block)( j_b, k_b ) * (*Lo_block)( i_b, k_b ));
            }
        }   
    }
#endif
    c.Lkji.put(triple(k+1, j, k),Lo_block); // Write the output tile at the next time step.
    
    return CnC::CNC_Success;
}

// Performs symmetric rank-k update of the submatrix.
// Input to this step is the given submatrix and the output of the previous step.
int S3_compute::execute(const triple & t, cholesky_context & c ) const
{
    tile_const_ptr_type A_block;
    tile_const_ptr_type L1_block;
    tile_const_ptr_type L2_block;
    double temp;
    
    int b = c.b;
    const int k = t[0];
    const int j = t[1];
    const int i = t[2];

    assert( j != k && i != k );
    c.Lkji.get(triple(k, j, i), A_block); // Get the input tile.
    
    if(i==j){   // Diagonal tile.
        c.Lkji.get(triple(k+1,j,k), L2_block); // In case of a diagonal tile, i=j, hence both the tiles are the same.
    }
    else{   // Non-diagonal tile.
        c.Lkji.get(triple(k+1,i,k), L2_block); // Get the first tile.
        c.Lkji.get(triple(k+1,j,k), L1_block); // Get the second tile.
    }
    
#ifdef USE_MKL
    const double alpha = -1;
	const double beta = 1;
    if(i==j){   // Diagonal tile.
		cblas_dsyrk( CblasColMajor, CblasLower, CblasNoTrans, b, b, alpha, L2_block->get_array(), b, beta, const_cast< double* >( A_block->get_array() ), b );
    } else {
		cblas_dgemm( CblasColMajor, CblasNoTrans, CblasTrans, b, b, b, alpha, L1_block->get_array(), b, L2_block->get_array(), b, beta, const_cast< double* >( A_block->get_array() ), b );
    }
#else
    for(int j_b = 0; j_b < b; j_b++) {
        for(int k_b = 0; k_b < b; k_b++) {
            temp = -1 * (*L2_block)( j_b, k_b );
            if(i!=j){
                for(int i_b = 0; i_b < b; i_b++) {
                    const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L1_block)( i_b, k_b ));
                }
            }
            else {
                for(int i_b = j_b; i_b < b; i_b++) {
                    const_cast< tile_type & >(*A_block)( i_b, j_b ) = (*A_block)( i_b, j_b ) + (temp * (*L2_block)( i_b, k_b ));
                }
            }
        }
    }
#endif

    c.Lkji.put(triple(k+1,j,i),A_block);  // Write the output at the next time step.
    
    return CnC::CNC_Success;
}

void cholesky( double * A, const int n, const int b, const char * oname, dist_type dt )
{
    tile_ptr_type mat_out;
    int p;
    int k;
    FILE *fout;

    p = n/b;
    tbb::tick_count t2 = tbb::tick_count::now();
    // Create an instance of the context class which defines the graph
    cholesky_context c( b, p, n, dt );
    
    //    for(int i = 0; i < p; i++) {
    CnC::parallel_for( 0, p, 1, [A,&c,b,p,n]( int i ) {
            for( int j = 0; j <= i; j++ ) {
                // Allocate memory for the tiles.
                tile_ptr_type temp = std::make_shared< tile_type >( b );
                // Split the matrix into tiles and write it into the item space at time 0.
                // The tiles are indexed by tile indices (which are tag values).
                for(int A_i = i*b,T_i = 0; T_i < b; A_i++,T_i++) {
                    for(int A_j = j*b,T_j = 0; T_j < b; A_j++,T_j++) {
                        (*temp)( T_i, T_j ) = A[A_i*n+A_j];
                    }
                }
                c.Lkji.put(triple(0,i,j),temp);
            }
            // control tags
            const int k = i;
            c.control_S1.put( k );
            for(int j = k+1; j < p; j++) {  
                c.control_S2.put(pair(k,j));
                for(int i = k+1; i <= j; i++) {
                    c.control_S3.put(triple(k,j,i));
                }
            }
        } );
    
    // Wait for all steps to finish
    c.wait();
    tbb::tick_count t3 = tbb::tick_count::now();

    printf("The time taken for parallel execution a matrix of size %d x %d : %g sec\n", n, n, (t3-t2).seconds());
    
    if(oname) {
        fout = fopen(oname, "w");
        for (int i = 0; i < p; i++) {
            for(int i_b = 0; i_b < b; i_b++) {
                k = 1;
                for (int j = 0; j <= i; j++) {
                    tile_const_ptr_type _tmp;
                    c.Lkji.get(triple(k, i, j), _tmp);
                    if(i != j) {
                        for(int j_b = 0; j_b < b; j_b++) {
                            fprintf(fout, "%lf ", (*_tmp)( i_b, j_b )); 
                        }
                    }
                    else {
                        for(int j_b = 0; j_b <= i_b; j_b++) {
                            fprintf(fout, "%lf ", (*_tmp)( i_b, j_b )); 
                        }
                    }
                    k++;
                }
                fprintf(fout, "\n" );
            }
        }
        fclose(fout);
    } else {
        // make sure the result has been computed
        c.Lkji.size();
        for (int i = 0; i < p; i++) {
            for (int j = 0; j <= i; j++) {
                tile_const_ptr_type _tmp;
                c.Lkji.get(triple(j+1, i, j), _tmp);
            }
        }
    }
}
