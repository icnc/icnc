/*Copyright (c) 2013-, Ravi Teja Mullapudi, CSA Indian Institue of Science
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of the Indian Institute of Science nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "floyd_base.h"
void init_square_matrix(double* matrix, int matrix_size)
{
    int i, j;
    srand(123);
    for(i= 0; i < matrix_size; i++) {
        for(j= 0; j < matrix_size; j++) {
            double ra = 1.0 + ((double)matrix_size * rand() / (RAND_MAX + 1.0));
            matrix[i*matrix_size + j] = ra;
        }
    }
    for(i= 0; i < matrix_size; i++) {
        matrix[i*matrix_size + i] = 0;
    }
}

void print_square_matrix(double *matrix, int matrix_size)
{
    int i, j;
    for(i= 0; i < matrix_size; i++) {
        for(j= 0; j < matrix_size; j++) {
            printf("%lf ", matrix[i* matrix_size + j]); 
        }
        printf("\n");
    }
}

int check_square_matrices(double * A, double * B, int n)
{
    int i, j;
    int errors = 0;
    int max_errors = 10;
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            double a = A[i*n+j];
            double b = B[i*n+j];
            if (!fequal(a, b)) {
                if (++errors >= max_errors) goto EXIT;
                printf("(A[%d][%d]=%lg) != (B[%d][%d]=%lg) (diff=%lg)\n", i, j, a,
                        i, j, b, 
                        a-b);
            }
        }
    }
EXIT:
    if (errors > 0)
    {
        printf("Number of errors=%d (max reported errors %d)\n", errors, max_errors);
    }
    return errors > 0;
}

void floyd_seq(double* path_distance_matrix, int size)
{
   int k, y, x;
   for(k=0; k < size; k++) {
        for(y=0; y < size; y++) {
            for(x=0; x < size; x++) {
                path_distance_matrix[y*size + x] = min(path_distance_matrix[y*size + k] + path_distance_matrix[k* size + x], path_distance_matrix[y*size + x]);
            }
        }
    }
}

void floyd_tiled_omp(double* path_distance_matrix, int matrix_size, int block_size)
{
    int num_blocks = matrix_size/block_size;

    for(int k = 0; k < matrix_size; k++) {
        #pragma omp parallel for firstprivate(num_blocks, block_size, k)
        for(int i = 0; i < num_blocks ; i++) { 
            for(int j = 0; j < num_blocks; j++) {
                for(int p = block_size * i; p < block_size * (i+1); p++) {
                    double dist_pk = path_distance_matrix[p*matrix_size + k];
                    for(int q = block_size * j; q < block_size * (j+1); q++) {    
                        path_distance_matrix[p*matrix_size + q] = min(dist_pk + path_distance_matrix[k*matrix_size + q], path_distance_matrix[p*matrix_size + q]);
                    }
                }
            }
        }
    }
}
