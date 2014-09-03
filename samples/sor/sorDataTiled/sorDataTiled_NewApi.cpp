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

//there was a hash issue as far as I recall
//typedef std::pair<int,int> intPair;
//
typedef int intPair;
intPair makePair(int i, int j) {return (i << 16) + j; }
int getFirst(intPair givenPair) { return givenPair >> 16;}
int getSecond(intPair givenPair) { return givenPair & 0xffff;}

int ceiling(int dividend, int divisor)
{
	int tmp = dividend / divisor;
	if(tmp*divisor != dividend) ++tmp;
	return tmp;
}

class Tile {
	public:
		Tile(): size(0), array(NULL) { //std::cout<<"def. const."<<std::endl;
		}
		Tile(int givenSize): size(givenSize), array(NULL) { array = new double[size]; }
		Tile& operator=(const Tile& rhs)
		{
		//	std::cout<<"operator="<<std::endl;
			size = rhs.size;
			array = new double[rhs.size];
			for (int i = 0; i < size; ++i)
				array[i] = rhs.array[i];
			return *this;
		}
		Tile(const Tile& rhs) 
		{
		//	std::cout<<"copy. const."<<std::endl;
			size = rhs.size;
			array = new double[rhs.size];
			for (int i = 0; i < size; ++i)
				array[i] = rhs.array[i];
		}
		~Tile() {if(array) { //std::cout<<"destruct"<<std::endl;
			delete [] array;}}
		double* getArray() { return array;}
	private:
		double* array;
		int size;
};
struct sorContext;

struct iterationStep{
	int execute(int i, sorContext& graph) const;
};

struct relaxation{
	int execute(intPair tileIndex2D, sorContext& graph) const;
};

struct sorContext : public CnC::context<sorContext>
{
	CnC::step_collection< relaxation > m_rlxSteps;
	CnC::step_collection< iterationStep > m_itSteps;
	CnC::tag_collection<intPair> tileSpace;
	CnC::tag_collection<int> iterationSpace;

	CnC::item_collection<intPair, Tile> prevTileMatrix, currTileMatrix;
	CnC::item_collection<int, int> nRowsInTile, nColumnsInTile, nRowsOfTiles,nColumnsOfTiles;
	CnC::item_collection<int, double> omega;

	sorContext() 
        : CnC::context< sorContext >(),
          m_rlxSteps( *this ),
          m_itSteps( *this ),
          tileSpace(*this),
          iterationSpace(*this),
          prevTileMatrix(*this),
          currTileMatrix(*this), 
          nRowsInTile(*this),
          nColumnsInTile(*this), 
          nRowsOfTiles(*this),
          nColumnsOfTiles(*this),
          omega(*this)
	{
		iterationSpace.prescribes( m_itSteps, *this );
		tileSpace.prescribes( m_rlxSteps, *this );
	}
//	~sorContext() {std::cout<<"graph destruct"<<std::endl;}
};

int iterationStep::execute(int iterationNumber, sorContext& graph) const
{
	int nRowsOfTiles;
    graph.nRowsOfTiles.get(444, nRowsOfTiles);
	int nColumnsOfTiles;
    graph.nColumnsOfTiles.get(555, nColumnsOfTiles);

	for(int i = 0; i < nRowsOfTiles; ++i)
		for(int j = 0; j < nColumnsOfTiles; ++j)
			graph.tileSpace.put(makePair(i,j));
#if 0
	for(int i = nRowsOfTiles-1; i >= 0; --i)
		for(int j = nColumnsOfTiles-1; j >= 0 ; --j)
			graph.tileSpace.put(makePair(i,j));
#endif
	return CnC::CNC_Success;
}

int relaxation::execute(intPair tileIndex2D, sorContext& graph) const
{
	int rowIndexOfTile = getFirst(tileIndex2D);
	int columnIndexOfTile = getSecond(tileIndex2D);

	// these are from curr, already computed matrix
	bool hasAbove = rowIndexOfTile != 0;
	bool hasLeft = columnIndexOfTile != 0;

	int nRowsOfTiles;
    graph.nRowsOfTiles.get(444, nRowsOfTiles);
	int nColumnsOfTiles;
    graph.nColumnsOfTiles.get(555, nColumnsOfTiles);

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
	
	if(hasAbove) {
        graph.currTileMatrix.get(makePair(rowIndexOfTile-1, columnIndexOfTile), above);
		aboveArray = above.getArray();
	}

	if(hasLeft) {
		graph.currTileMatrix.get(makePair(rowIndexOfTile, columnIndexOfTile-1), left);
		leftArray = left.getArray();
	}

	if(hasBelow)
	{
		graph.prevTileMatrix.get(makePair(rowIndexOfTile+1, columnIndexOfTile), below);
		belowArray = below.getArray();
	}

	if(hasRight) {
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

				//printf("b:%lf r:%lf a:%lf l:%lf c:%lf\n",belowRow[0],valueRight,aboveRow[0],leftArray[currRowDist+nColumnsInTile-1] ,currRow[0]);
				currRow[0] = omegaOver4*(belowRow[0]+valueRight+aboveRow[0]+leftArray[currRowDist+nColumnsInTile-1]) + oneMinusOmega*currRow[0];
			}

			for(int j = 1; j < nColumnsInTile-1; ++j)
			{
				//printf("b:%lf r:%lf a:%lf l:%lf c:%lf\n",belowRow[j],currRow[j+1],aboveRow[j],currRow[j-1] ,currRow[j]);
				currRow[j] = omegaOver4*(belowRow[j]+currRow[j+1]+aboveRow[j]+currRow[j-1])+oneMinusOmega*currRow[j];
			}

			if(hasRight && nColumnsInTile > 1)
			{
				int j = nColumnsInTile-1;
				//printf("b:%lf r:%lf a:%lf l:%lf c:%lf\n",belowRow[j],rightArray[currRowDist],aboveRow[j],currRow[j-1] ,currRow[j]);
				currRow[j] = omegaOver4*(belowRow[j]+rightArray[currRowDist]+aboveRow[j]+currRow[j-1]) + oneMinusOmega*currRow[j];
			}
		}
		currRowDist += nColumnsInTile;
	}

//	std::cout<<"4winds bar"<<std::endl;
	// if(hasAbove) delete above;
	// if(hasLeft) delete left;
	// if(hasBelow) delete below;
	// if(hasRight) delete right;

	graph.currTileMatrix.put(tileIndex2D, tile);

	// delete tile;
	return CnC::CNC_Success;
}

class TiledInputArray{
	public:
		TiledInputArray(double** inputArray, int nRO, int nCO, int nRI, int nCI)
			:nRowsOfTiles(nRO), nColumnsOfTiles(nCO), nRowsInTile(nRI), nColumnsInTile(nCI)	
		{
			tileMatrix = new Tile**[nRowsOfTiles];
			int nElementsInTile = nRowsInTile*nColumnsInTile;

			for(int inputRowBegin = 0, i = 0; i < nRowsOfTiles; ++i)
			{
				int inputColumnBegin = 0;
				Tile** currTileArray = tileMatrix[i] = new Tile*[nColumnsOfTiles];
				for(int j = 0; j < nColumnsOfTiles; ++j)
				{
					Tile* currTile = currTileArray[j] = new Tile(nElementsInTile);
					double* tileData = currTile->getArray();
					int tileRow = 0;
					for(int k = 0; k < nRowsInTile; ++k)
					{
						int inputRow = inputRowBegin+k;
						for(int l = 0; l < nColumnsInTile; ++l)
							tileData[tileRow+l] = inputArray[inputRow][inputColumnBegin+l];
						tileRow += nColumnsInTile;
					}
					inputColumnBegin += nColumnsInTile;
				}
				inputRowBegin += nRowsInTile;
			}
		}
		~TiledInputArray()
		{
			for(int i = 0; i < nRowsOfTiles; ++i)
			{
				for(int j = 0; j < nColumnsOfTiles; ++j)
					delete tileMatrix[i][j];
				delete [] tileMatrix[i];
			}
			delete [] tileMatrix;

		}

		Tile* getTile(int i , int j)
		{	
			return tileMatrix[i][j];
		}

		void replace(int i, int j, const Tile& givenTile)
		{
			//std::cout<<"replace"<<std::endl;
			delete tileMatrix[i][j];
			tileMatrix[i][j] = new Tile(givenTile);
		}

		void prettyPrint()
		{
			for(int i = 0; i < nRowsOfTiles; ++i)
			{
				int tileArrayIndex = 0;
				for(int k = 0; k < nRowsInTile; ++k)
				{
					double* tileArray; 
					for(int j = 0; j < nColumnsOfTiles-1; ++j)
					{
						tileArray = tileMatrix[i][j]->getArray();

						for(int l = 0; l < nColumnsInTile; ++l)
							printf("%lf, ",tileArray[tileArrayIndex+l]);
					}

					tileArray = tileMatrix[i][nColumnsOfTiles-1]->getArray();
					for(int l = 0; l < nColumnsInTile-1; ++l)
						printf("%lf, ",tileArray[tileArrayIndex+l]);
					printf("%lf\n",tileArray[tileArrayIndex+nColumnsInTile-1]);
					tileArrayIndex += nColumnsInTile;
				}	
			}
		}
	private:
		int nRowsOfTiles, nColumnsOfTiles, nRowsInTile, nColumnsInTile;
		Tile*** tileMatrix;
};

int main(int argc, char* argv[])
{
	int toBeReturned = 0;
	setbuf(stdout,NULL);
	if(argc < 5)
	{
		std::cerr<<"usage: sor omega nRowsInMatrix nColumnsInMatrix nIterations nRowsInTile nColumnsInTile [-v]\n";
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

		int nRowsOfTiles = ceiling (nRowsInMatrix, nRowsInTile);
		int nColumnsOfTiles =ceiling (nColumnsInMatrix, nColumnsInTile);

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
		//	std::cout<<"loop begin"<<std::endl;
			sorContext graph;//	
			//CnC::debug::trace_all(graph, "tiled sor");
			graph.omega.put(111, omega);
			graph.nRowsInTile.put(222, nRowsInTile);
			graph.nColumnsInTile.put(333, nColumnsInTile);
			graph.nRowsOfTiles.put(444, nRowsOfTiles);
			graph.nColumnsOfTiles.put(555, nColumnsOfTiles);

			for(int i = 0; i < nRowsOfTiles; ++i)
				for(int j = 0; j < nColumnsOfTiles; ++j)
					graph.prevTileMatrix.put(makePair(i,j), *tiledInputArray.getTile(i,j));
			graph.iterationSpace.put(666+iteration);


			graph.wait();
			// since execution finished I can now write over **inputArray

			for(int i = 0; i < nRowsOfTiles; ++i)
				for(int j = 0; j < nColumnsOfTiles; ++j) {
                    graph.currTileMatrix.get(makePair(i,j), _tmp);
					tiledInputArray.replace(i, j, _tmp); 
                }
		//	std::cout<<"loop end"<<std::endl;
		}

		tbb::tick_count t1 = tbb::tick_count::now();
		printf("Computed in %g seconds\n", (t1-t0).seconds());
		if(verbose)
		{
			printf("after\n");
			tiledInputArray.prettyPrint();
		}

		for(int i = 0; i < nRowsInMatrix; ++i)
			delete [] inputArray[i];
		delete [] inputArray;
	}
	return toBeReturned;
}
