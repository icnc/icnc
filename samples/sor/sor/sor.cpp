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
#include <cstdlib>
#include "tbb/tick_count.h"
#include "cnc/cnc.h"

typedef size_t size_tPair;
#include "sor.h"

size_tPair makePair( size_t i, size_t j ) {return (i << 16) + j; }
size_t getFirst( const size_tPair & givenPair ) { return givenPair >> 16; }
size_t getSecond( const size_tPair & givenPair ) { return givenPair & 0xffff; }

int iterationStep::execute( const size_t & iterationNumber, sor_context& graph) const
{
    size_t nRows;
    graph.M.get(0, nRows);
    size_t nColumns;
    graph.N.get(0, nColumns);

    // prescribe the first and the last column to be copied
    for(size_t i = 0; i < nRows; ++i){
        graph.copySpace.put(makePair(i,0));
        graph.copySpace.put(makePair(i,nColumns-1));
    }

    // prescribe the first and the last row to be copied
    for(size_t j = 0; j < nColumns; ++j){
        graph.copySpace.put(makePair(0,j));
        graph.copySpace.put(makePair(nRows-1,j));
    }

    // prescribe all the inner indices to be relaxed
    for(size_t i = 1; i < nRows-1; ++i)
        for(int j = 1; j < nColumns-1; ++j)
            graph.matrixSpace.put(makePair(i,j));

    return CnC::CNC_Success;
}

// get the value from the previous iteration for the given iteration space
// and put it into the current iteration space matrix with the same value
int copier::execute( const size_tPair & index2D, sor_context& graph) const
{
    double _tmp;
    graph.prevIterMatrix.get(index2D, _tmp );
    graph.currIterMatrix.put(index2D, _tmp ); //M[i,j]
    return CnC::CNC_Success;
}

// for the given index get the weight, compute a weighted average of the value on
// the index and its neighbors, put that value on the current iteration matrix entry
// with the same index
// 
int relaxation::execute( const size_tPair & index2D, sor_context& graph) const
{
    size_t rowIndex = getFirst(index2D);
    size_t columnIndex = getSecond(index2D);

    double omega;
    graph.omega.get(0, omega );
    double omegaOver4 = omega * 0.25;
    double oneMinusOmega = 1-omega;

    double ij;
    graph.prevIterMatrix.get(makePair(rowIndex, columnIndex), ij); //M[i,j]
    double iPlus1j;
    graph.prevIterMatrix.get(makePair(rowIndex+1, columnIndex), iPlus1j); //M[i+1,j]
    double ijPlus1;
    graph.prevIterMatrix.get(makePair(rowIndex, columnIndex+1), ijPlus1); //M[i,j+1]
    // Attention! These are from output since you are walking over i,j tuples incrementally 
    double iMinus1j;
    graph.currIterMatrix.get(makePair(rowIndex-1, columnIndex), iMinus1j); //M[i-1,j]
    double ijMinus1;
    graph.currIterMatrix.get(makePair(rowIndex, columnIndex-1), ijMinus1); //M[i,j-1]

    graph.currIterMatrix.put(index2D, omegaOver4*(iPlus1j+ijPlus1+iMinus1j+ijMinus1) + oneMinusOmega*ij);

    return CnC::CNC_Success;
}

namespace {
    void fillMatrixWithRand ( double ** inputArray , int nRows, int nColumns )
    {
        for(int i = 0; i < nRows; ++i){
            double* currRow = inputArray[i] = new double[nColumns];
            for(int j = 0; j < nColumns; ++j)
                //                currRow[j] = (rand() % 100) *0.01;
				currRow[j] = ((j*11) % 100) *0.01;
        }
    }
    void printMatrix ( double ** inputArray , int nRows, int nColumns )
    {
        for(int i = 0; i < nRows; ++i){
            for(size_t j = 0; j < nColumns-1; ++j) 
                printf("%lf, ",inputArray[i][j]);
            printf("%lf\n",inputArray[i][nColumns-1]);
        }
    }
    void deallocateMatrix ( double ** inputArray , int nRows )
    {
        for(size_t i = 0; i < nRows; ++i)
            delete [] inputArray[i];
        delete [] inputArray;
    }
}
int main(int argc, char* argv[])
{
    int toBeReturned = 0;
    if(argc < 5) {
        std::cerr<<"usage: sor omega nRows nColumns nIterations [-v]\n";
        toBeReturned = 1;
    } else {
        //        srand(0xdeadbeef);
        double omega = atof(argv[1]);
        size_t nRows = (size_t)atoi(argv[2]);
        size_t nColumns = (size_t)atoi(argv[3]);
        int nIterations = (size_t)atoi(argv[4]);
        bool verbose = argc==6 && !strcmp("-v", argv[5]);

        double** inputArray = new double*[nRows];
        // not the best rand but this is a test program anyway
        fillMatrixWithRand( inputArray , nRows, nColumns );

        if(verbose){
            printf("omega=%lf, M=%u, N=%u, and nIterations=%u\nbefore\n",omega,nRows,nColumns,nIterations);
            printMatrix( inputArray , nRows, nColumns );
        }

        tbb::tick_count t0 = tbb::tick_count::now();
        for(int iteration = 0; iteration < nIterations; ++iteration){
            sor_context graph;
            graph.omega.put(0, omega);
            graph.M.put(0, nRows);
            graph.N.put(0, nColumns);
            graph.iterationSpace.put(iteration);
            for(size_t i = 0; i < nRows; ++i)
                for(size_t j = 0; j < nColumns; ++j)
                    graph.prevIterMatrix.put(makePair(i,j), inputArray[i][j]);

            graph.wait();
            // since execution finished I can now write over **inputArray

            for(size_t i = 0; i < nRows; ++i)
                for(size_t j = 0; j < nColumns; ++j)
                    graph.currIterMatrix.get(makePair(i,j), inputArray[i][j]); 
        }

        tbb::tick_count t1 = tbb::tick_count::now();
        printf("Computed in %g seconds\n", (t1-t0).seconds());

        if(verbose){
            printf("after\n");
            printMatrix( inputArray , nRows, nColumns );
        }
        deallocateMatrix(inputArray, nRows);
    }
    return toBeReturned ;
}
