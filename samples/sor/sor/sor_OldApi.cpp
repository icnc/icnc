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

#include <stdio.h>
#include <stdlib.h>
#include "tbb/tick_count.h"
#include "sor.h"

using namespace CnC;
using namespace std;

typedef unsigned int uint_t;

StepReturnValue_t iterationStep::operator()(sor_graph_t& graph, const index_t tag)
{
	uint_t nRows = graph.M.Get(index_t(0));
	uint_t nColumns = graph.N.Get(index_t(0));

	for(uint_t i = 0; i < nRows; ++i)
	{
		graph.copySpace.Put(index_t(i,0));
		graph.copySpace.Put(index_t(i,nColumns-1));
	}

	for(uint_t j = 0; j < nColumns; ++j)
	{
		graph.copySpace.Put(index_t(0,j));
		graph.copySpace.Put(index_t(nRows-1,j));
	}

	for(int i = 1; i < nRows-1; ++i)
		for(int j = 1; j < nColumns-1; ++j)
			graph.matrixSpace.Put(index_t(i,j));

	return CNC_Success;
}

StepReturnValue_t copier::operator()(sor_graph_t& graph, const index_t tag)
{
	graph.currIterMatrix.Put(tag, graph.prevIterMatrix.Get(tag)); //M[i,j]
	return CNC_Success;
}

StepReturnValue_t relaxation::operator()(sor_graph_t& graph, const index_t tag)
{
	int rowIndex = tag[0];
	int columnIndex = tag[1];

	double omega = graph.omega.Get(index_t(0));
	double omegaOver4 = omega * 0.25;
	double oneMinusOmega = 1-omega;

	double ij = graph.prevIterMatrix.Get(index_t(rowIndex, columnIndex)); //M[i,j]
	double iPlus1j = graph.prevIterMatrix.Get(index_t(rowIndex+1, columnIndex)); //M[i+1,j]
	double ijPlus1 = graph.prevIterMatrix.Get(index_t(rowIndex, columnIndex+1)); //M[i,j+1]
	// Attention! These are from output since you are walking over i,j tuples incrementally 
	double iMinus1j = graph.currIterMatrix.Get(index_t(rowIndex-1, columnIndex)); //M[i-1,j]
	double ijMinus1 = graph.currIterMatrix.Get(index_t(rowIndex, columnIndex-1)); //M[i,j-1]

	graph.currIterMatrix.Put(tag, omegaOver4*(iPlus1j+ijPlus1+iMinus1j+ijMinus1) + oneMinusOmega*ij);

	return CNC_Success;
}

int main(int argc, char* argv[])
{
	int toBeReturned = 0;
	if(argc < 5)
	{
		std::cerr<<"usage: sor omega nRows nColumns nIterations [-v]\n";
		toBeReturned = 1;
	}
	else{
		srand(0xdeadbeef);
		double omega = atof(argv[1]);
		uint_t nRows = (uint_t)atoi(argv[2]);
		uint_t nColumns = (uint_t)atoi(argv[3]);
		uint_t nIterations = (uint_t)atoi(argv[4]);
		bool verbose = argc==6 && !strcmp("-v", argv[5]);

		double** inputArray = new double*[nRows];

		// not the best rand but this is a test program anyway
		for(uint_t i = 0; i < nRows; ++i)
		{
			double* currRow = inputArray[i] = new double[nColumns];
			for(uint_t j = 0; j < nColumns; ++j)
				currRow[j] = (rand() % 100) *0.01;
		}

		if(verbose)
		{
			printf("omega=%lf, M=%u, N=%u, and nIterations=%u\n",omega,nRows,nColumns,nIterations);
			printf("before\n");
			for(uint_t i = 0; i < nRows; ++i)
			{
				for(uint_t j = 0; j < nColumns-1; ++j)
					printf("%lf, ",inputArray[i][j]);
				printf("%lf\n",inputArray[i][nColumns-1]);
			}
		}
		tbb::tick_count t0 = tbb::tick_count::now();

		for(uint_t iteration = 0; iteration < nIterations; ++iteration)
		{
			sor_graph_t graph;
			graph.omega.Put(index_t(0), omega);
			graph.M.Put(index_t(0), nRows);
			graph.N.Put(index_t(0), nColumns);
			graph.iterationSpace.Put(index_t(iteration));
			for(uint_t i = 0; i < nRows; ++i)
				for(uint_t j = 0; j < nColumns; ++j)
					graph.prevIterMatrix.Put(index_t(i,j), inputArray[i][j]);

			graph.run();
			// since execution finished I can now write over **inputArray

			for(uint_t i = 0; i < nRows; ++i)
				for(uint_t j = 0; j < nColumns; ++j)
					inputArray[i][j] = graph.currIterMatrix.Get(index_t(i,j)); 
		}

		tbb::tick_count t1 = tbb::tick_count::now();
		printf("Computed in %g seconds\n", (t1-t0).seconds());
		if(verbose)
		{
			printf("after\n");
			for(uint_t i = 0; i < nRows; ++i)
			{
				for(uint_t j = 0; j < nColumns-1; ++j)
					printf("%lf, ",inputArray[i][j]);
				printf("%lf\n",inputArray[i][nColumns-1]);
			}
		}

		for(uint_t i = 0; i < nRows; ++i)
			delete [] inputArray[i];
		delete [] inputArray;
	}
	return toBeReturned;
}
