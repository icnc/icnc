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
#include "cnc/cnc.h"
#include "cnc/debug.h"

#include "Tile.h"
#include "TiledInputArray.h"

//there was a hash issue as far as I recall
//typedef std::pair<int,int> intPair;
//
typedef int intPair;
intPair makePair(int i, int j) {return (i << 16) + j; }
int getFirst (intPair givenPair) { return givenPair >> 16;}
int getSecond (intPair givenPair) { return givenPair & 0xffff;}

int ceiling(int dividend, int divisor)
{
	int tmp = dividend / divisor;
	if(tmp*divisor != dividend) ++tmp;
	return tmp;
}

int nRowsOfTiles;
int nColumnsOfTiles;

struct sorContext;

struct myTuner : public CnC::step_tuner<>
{
	int priority(const intPair& givenTag,sorContext& graph) const
	{
			static int diagonalWidth = (nRowsOfTiles+nColumnsOfTiles-2)/20+1;
			int row = getFirst(givenTag);
			int column = getSecond(givenTag);
			return ((row+column)/diagonalWidth)%20;
	}

	template <typename T>
        void depends( const intPair& tag, sorContext& graph, T & dC ) const
        {
        }

        /// \brief returns whether the step should be pre-scheduled 
        ///
        /// only useful if the XGet mechanism is used
        bool preschedule() const
        {
            return false;
        }
};// tunerInstance;

struct relaxation{
	int execute(intPair tileIndex2D, sorContext& graph) const;
};

struct sorContext : public CnC::context<sorContext>
{
	CnC::step_collection< relaxation, myTuner > m_steps;
	CnC::tag_collection<intPair> tileSpace;

	CnC::item_collection<intPair, Tile> prevTileMatrix, currTileMatrix;
	CnC::item_collection<int, int> nRowsInTile, nColumnsInTile;
	CnC::item_collection<int, double> omega;

	sorContext()
        : CnC::context< sorContext >(),
          m_steps( *this ),
          tileSpace(*this),
          prevTileMatrix(*this),
          currTileMatrix(*this), 
          nRowsInTile(*this),
          nColumnsInTile(*this),
          omega(*this)
	{
		tileSpace.prescribes( m_steps, *this );
	}
};

int relaxation::execute(intPair tileIndex2D, sorContext& graph) const
{
	int rowIndexOfTile = getFirst(tileIndex2D);
	int columnIndexOfTile = getSecond(tileIndex2D);

	// these are from curr, already computed matrix
	bool hasAbove = rowIndexOfTile != 0;
	bool hasLeft = columnIndexOfTile != 0;

	// these are from prev, input matrix
	// also these are the corner cases
	bool hasBelow = rowIndexOfTile != nRowsOfTiles-1;
	bool hasRight= columnIndexOfTile != nColumnsOfTiles-1;

	int nRowsInTile;
    graph.nRowsInTile.get(222, nRowsInTile); 
	int nColumnsInTile;
    graph.nColumnsInTile.get(333, nColumnsInTile); 

	Tile above, left, below, right;
	double *aboveArray = NULL, *leftArray = NULL, *belowArray = NULL, *rightArray = NULL;
	
	if(hasAbove)
	{
		graph.currTileMatrix.get(makePair(rowIndexOfTile-1, columnIndexOfTile), above);
		aboveArray = above.getArray();
	}

	if(hasLeft)
	{
		graph.currTileMatrix.get(makePair(rowIndexOfTile, columnIndexOfTile-1), left);
		leftArray = left.getArray();
	}

	if(hasBelow)
	{
		graph.prevTileMatrix.get(makePair(rowIndexOfTile+1, columnIndexOfTile), below);
		belowArray = below.getArray();
	}

	if(hasRight)
	{
		graph.prevTileMatrix.get(makePair(rowIndexOfTile, columnIndexOfTile+1), right);
		rightArray = right.getArray();
	}


	Tile tile;
    graph.prevTileMatrix.get(tileIndex2D, tile);
	double* tileArray = tile.getArray();
    
    double omega;
    graph.omega.get(111, omega);
	double omegaOver4 = omega * 0.25;
	double oneMinusOmega = 1-omega;

	int currRowDist = 0;
	int indexLastRow = (nRowsInTile-1)*nColumnsInTile;
	for(int i = 0; i < nRowsInTile; ++i)
	{
		double *currRow = tileArray + currRowDist, *aboveRow = NULL, *belowRow = NULL;

		if(i) aboveRow = currRow-nColumnsInTile; //non zero row
		else if(hasAbove) aboveRow = aboveArray + indexLastRow; //zero row

		if(i != nRowsInTile-1) belowRow = currRow + nColumnsInTile; //not last row
		else if(hasBelow) belowRow = belowArray; //zero row

		if(aboveRow && belowRow)
		{
			if(hasLeft && (nColumnsInTile > 1 || hasRight))
			{
				double valueRight;

				if(nColumnsInTile > 1) valueRight = currRow[1];
				else valueRight = rightArray[currRowDist];

				currRow[0] = omegaOver4*(belowRow[0]+valueRight+aboveRow[0]+leftArray[currRowDist+nColumnsInTile-1]) + oneMinusOmega*currRow[0];
			}

			for(int j = 1; j < nColumnsInTile-1; ++j)
			{
				currRow[j] = omegaOver4*(belowRow[j]+currRow[j+1]+aboveRow[j]+currRow[j-1])+oneMinusOmega*currRow[j];
			}

			if(hasRight && nColumnsInTile > 1)
			{
				int j = nColumnsInTile-1;
				currRow[j] = omegaOver4*(belowRow[j]+rightArray[currRowDist]+aboveRow[j]+currRow[j-1]) + oneMinusOmega*currRow[j];
			}
		}
		currRowDist += nColumnsInTile;
	}

	// if(hasAbove) delete above;
	// if(hasLeft) delete left;
	// if(hasBelow) delete below;
	// if(hasRight) delete right;

	graph.currTileMatrix.put(tileIndex2D, tile);

    //	delete tile;
	return CnC::CNC_Success;
}

int main(int argc, char* argv[])
{
	int toBeReturned = 0;
	setbuf(stdout,NULL);
	if(argc < 5)
	{
		std::cerr<<"usage: sor omega nRowsInMatrix nColumnsInMatrix nIterations nRowsInTile nColumnsInTile [-v]\n";
		std::cerr<<"nRowsInTile should be a factor of nRowsInMatrix \n";
		std::cerr<<"nColumnsInTile should be a factor of nColumnsInMatrix \n";
		toBeReturned = 1;
	}
	else{
        //		srand(0xdeadbeef);
		double omega = atof(argv[1]);
		int nRowsInMatrix = (int)atoi(argv[2]);
		int nColumnsInMatrix = (int)atoi(argv[3]);
		int nIterations = (int)atoi(argv[4]);

		int nRowsInTile = (int)atoi(argv[5]);
		int nColumnsInTile = (int)atoi(argv[6]);
		int nElementsInTile = nRowsInTile * nColumnsInTile; 

		//int  ceiling(
		nRowsOfTiles = nRowsInMatrix/ nRowsInTile;
		//int  ceiling(
		nColumnsOfTiles = nColumnsInMatrix/ nColumnsInTile;

		bool verbose = argc==8 && !strcmp("-v", argv[7]);

		double** inputArray = new double*[nRowsInMatrix];

		// not the best rand but this is a test program anyway
		for(int i = 0; i < nRowsInMatrix; ++i)
		{
			double* currRow = inputArray[i] = new double[nColumnsInMatrix];
			for(int j = 0; j < nColumnsInMatrix; ++j)
                //				currRow[j] = (rand() % 100) *0.01;
				currRow[j] = ((j*11) % 100) *0.01;
		}
		TiledInputArray tiledInputArray(inputArray, nRowsOfTiles, nColumnsOfTiles, nRowsInTile, nColumnsInTile);

		if(verbose)
		{
			printf("omega=%lf, M=%d, N=%d, nIterations=%d, nRowsInTile=%d and nColumnsInTile=%d\n",
					omega, nRowsInMatrix, nColumnsInMatrix, nIterations, nRowsInTile, nColumnsInTile);
			printf("before\n");
			for(int i = 0; i < nRowsInMatrix; ++i)
			{
				for(int j = 0; j < nColumnsInMatrix-1; ++j)
					printf("%lf, ",inputArray[i][j]);
				printf("%lf\n",inputArray[i][nColumnsInMatrix-1]);
			}
		}

		tbb::tick_count t0 = tbb::tick_count::now();

        Tile _tmp;
		for(int iteration = 0; iteration < nIterations; ++iteration)
		{
			sorContext graph;//	
			//CnC::debug::trace_all(graph, "tiled sor");
			graph.omega.put(111, omega);
			graph.nRowsInTile.put(222, nRowsInTile);
			graph.nColumnsInTile.put(333, nColumnsInTile);

			for(int i = 0; i < nRowsOfTiles; ++i)
				for(int j = 0; j < nColumnsOfTiles; ++j)
				{
					graph.prevTileMatrix.put(makePair(i,j), *tiledInputArray.getTile(i,j));
					graph.tileSpace.put(makePair(i,j));
				}


			graph.wait();
			// since execution finished I can now write over **inputArray

			for(int i = 0; i < nRowsOfTiles; ++i)
				for(int j = 0; j < nColumnsOfTiles; ++j) {
                    graph.currTileMatrix.get(makePair(i,j), _tmp );
					tiledInputArray.replace(i, j, _tmp); 
                }
		}

		tbb::tick_count t1 = tbb::tick_count::now();
		printf("Computed in %g seconds\n", (t1-t0).seconds());
		if(verbose) {
			printf("after\n");
			tiledInputArray.prettyPrint();
		}

		for(int i = 0; i < nRowsInMatrix; ++i)
			delete [] inputArray[i];
		delete [] inputArray;
	}
	return toBeReturned;
}
