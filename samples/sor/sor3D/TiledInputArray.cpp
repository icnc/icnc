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

#include "stdio.h"
#include "TiledInputArray.h"

TiledInputArray::TiledInputArray(double** inputArray, int nRowsOf, int nColumnsOf, int nRowsIn, int nColumnsIn)
:nRowsOfTiles(nRowsOf), nColumnsOfTiles(nColumnsOf), nRowsInTile(nRowsIn), nColumnsInTile(nColumnsIn)
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

TiledInputArray::~TiledInputArray()
{
	for(int i = 0; i < nRowsOfTiles; ++i)
	{
		for(int j = 0; j < nColumnsOfTiles; ++j)
			delete tileMatrix[i][j];
		delete [] tileMatrix[i];
	}
	delete [] tileMatrix;
}

Tile* TiledInputArray::getTile(int i , int j)
{	
	return tileMatrix[i][j];
}

void TiledInputArray::replace(int i, int j, const Tile& givenTile)
{
	delete tileMatrix[i][j];
	tileMatrix[i][j] = new Tile(givenTile);
}

void TiledInputArray::prettyPrint()
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
