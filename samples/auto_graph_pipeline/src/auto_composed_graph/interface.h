#ifndef INTERFACE_H
#define INTERFACE_H

// This is the interface between the user program (written in any host language,
// including C/Julia/Python/Matlab, etc.) and the compiler-auto-composed graphs.

// The functions initialize/finalize CnC with the composed graphs.
extern "C" void initialize_CnC();
extern "C" void finalize_CnC();

// For each sequence of functions that have been composed into a single function,
// declare that single function here.
// All CnC magic is hidden in such auto-generated functions.
extern "C" void dgemm_dgemm( int M, int N, int K, double * A, double * B, 
                             double * C, double * D, double * E );
#endif  
