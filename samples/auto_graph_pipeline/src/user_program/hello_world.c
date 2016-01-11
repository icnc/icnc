// This user program contains two calls to matrix multiply (cblas_dgemm). A
// compiler is supposed to automatically compose their CnC graphs (their wrappers)
// into a single graph, and then replace the two calls with a call to the
// composed graph. So far, the compiler's work is done manually. We highlight 
// it with "COMPILER WORK" in the annotations to clearly indicate what the
// compiler should do (in future).

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#ifdef USE_MKL
    #include "mkl.h"
#else
    #include "cblas.h"
#endif

#ifdef USE_COMPOSED
    // COMPILER WORK: This part should be auto generated and inserted by compiler.
    #include "interface.h"
#endif

int main()
{
#ifdef USE_COMPOSED
    // COMPILER WORK: This part should be auto generated and inserted by compiler.
    // Initialize CnC. This should be done once and only once in an application 
    initialize_CnC();
#endif
    
    unsigned long long num_elements = (unsigned long long)SZ * (unsigned long long)SZ;
    double *A = (double *)malloc(num_elements * sizeof(double));
    double *B = (double *)malloc(num_elements * sizeof(double));
    double *C = (double *)malloc(num_elements * sizeof(double));
    double *D = (double *)malloc(num_elements * sizeof(double));
    double *E = (double *)malloc(num_elements * sizeof(double));

#ifdef VALIDATE    
    for (unsigned long long i =0; i < num_elements; i++) { 
        A[i] = B[i] = D[i] = 1.0; 
    }
#endif

    double t1 = omp_get_wtime();
        
#ifdef USE_COMPOSED
    // COMPILER WORK: This part should be auto generated and inserted by compiler.
    // Call the composed graph.
    dgemm_dgemm(SZ, SZ, SZ, A, B, C, D, E);
#else
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                SZ, SZ, SZ, 1.0, A, SZ, B, SZ, 0.0, C, SZ);
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                SZ, SZ, SZ, 1.0, C, SZ, D, SZ, 0.0, E, SZ);
#endif

    double t2 = omp_get_wtime();
    printf("Time used: %f sec.", t2 - t1);

#ifdef VALIDATE    
    double sum = 0.0;
    for (unsigned long long i =0; i < num_elements; i++) { 
        sum += E[i];
    }
    printf("sum = %f\n", sum);
#endif
    
#ifdef USE_COMPOSED
    // COMPILER WORK: This part should be auto generated and inserted by compiler.
    // Finalize CnC. This should be done once and only once in an application 
    finalize_CnC();
#endif
    return 0;
}
