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

    CnC::item_collection<intPair, double> prevIterMatrix, currIterMatrix;
    CnC::item_collection<int, int> nRowsInTile, nColumnsInTile, nRowsOfTiles,nColumnsOfTiles;
    CnC::item_collection<int, double> omega;

    sorContext()
        : CnC::context< sorContext >(),
          m_rlxSteps( *this ),
          m_itSteps( *this ),
          tileSpace(*this),
          iterationSpace(*this),
          prevIterMatrix(*this),
          currIterMatrix(*this), 
          nRowsInTile(*this),
          nColumnsInTile(*this), 
          nRowsOfTiles(*this),
          nColumnsOfTiles(*this),
          omega(*this)
    {
		iterationSpace.prescribes( m_itSteps, *this );
		tileSpace.prescribes( m_rlxSteps, *this );
    }
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
    graph.nRowsInTile.get(222,nRowsInTile ); 
    int nColumnsInTile;
    graph.nColumnsInTile.get(333, nColumnsInTile); 


    int firstElementI = rowIndexOfTile*nRowsInTile;
    int firstElementJ = columnIndexOfTile*nColumnsInTile;

    double *above, *left, *below, *right;
    if(hasAbove) {
        above = new double[nColumnsInTile];
        for(int j = 0; j < nColumnsInTile; ++j)
            graph.currIterMatrix.get(makePair(firstElementI-1,firstElementJ+j)+0x8000, above[j]);
    }
    if(hasLeft) {
        left = new double[nRowsInTile];
        for(int i = 0; i < nRowsInTile; ++i)
            graph.currIterMatrix.get(makePair(firstElementI+i,firstElementJ-1)+0x8000, left[i]);
    }
    if(hasBelow) {
        below = new double[nColumnsInTile];
        for(int j = 0; j < nColumnsInTile; ++j)
            graph.prevIterMatrix.get(makePair(firstElementI+nRowsInTile,firstElementJ+j), below[j]);
    }
    if(hasRight) {
        right = new double[nRowsInTile];
        for(int i = 0; i < nRowsInTile; ++i)
            graph.prevIterMatrix.get(makePair(firstElementI+i,firstElementJ+nColumnsInTile), right[i]);
    }

    double** tile = new double*[nRowsInTile];
    for(int i = 0; i < nRowsInTile; ++i) {
        double* currRow = tile[i] = new double[nColumnsInTile];
        for(int j = 0; j < nColumnsInTile; ++j)
            graph.prevIterMatrix.get(makePair(firstElementI+i,firstElementJ+j), currRow[j]);
    }


    double omega;
    graph.omega.get(111, omega);
    double omegaOver4 = omega * 0.25;
    double oneMinusOmega = 1-omega;

    for(int i = 0; i < nRowsInTile; ++i) {
        double* currRow = tile[i], *aboveRow = NULL, *belowRow = NULL;

        if(i) aboveRow = tile[i-1]; //non zero row
        else if(hasAbove) aboveRow = above; //zero row

        if(i != nRowsInTile-1) belowRow = tile[i+1]; //not last row
        else if(hasBelow) belowRow = below; //zero row

        if(aboveRow && belowRow) {
            if(hasLeft && (nColumnsInTile > 1 || hasRight)) {
                double valueRight;

                if(nColumnsInTile > 1) valueRight = tile[i][1];
                else valueRight = right[i];

                currRow[0] = omegaOver4*(belowRow[0]+valueRight+aboveRow[0]+left[i]) + oneMinusOmega*currRow[0];
            }

            for(int j = 1; j < nColumnsInTile-1; ++j)
                currRow[j] = omegaOver4*(belowRow[j]+currRow[j+1]+aboveRow[j]+currRow[j-1])+oneMinusOmega*currRow[j];

            if(hasRight && nColumnsInTile > 1) {
                int j = nColumnsInTile-1;
                currRow[j] = omegaOver4*(belowRow[j]+right[i]+aboveRow[j]+tile[i][j-1]) + oneMinusOmega*currRow[j];
            }
        }
    }

    if(hasAbove)
        delete [] above;
    if(hasLeft)
        delete [] left;
    if(hasBelow)
        delete [] below;
    if(hasRight)
        delete [] right;

    for(int i = 0; i < nRowsInTile; ++i) {
        double* currRow = tile[i];
        for(int j = 0; j < nColumnsInTile; ++j)
            graph.currIterMatrix.put(makePair(firstElementI+i,firstElementJ+j)+0x8000, currRow[j]);
    }

    for(int i = 0; i < nRowsInTile; ++i)
        delete [] tile[i];
    delete [] tile;

    return CnC::CNC_Success;
}

// may use math.h ceil but do not know how efficient

int main(int argc, char* argv[])
{
    int toBeReturned = 0;
    if(argc < 5) {
        std::cerr<<"usage: sor omega nRowsInMatrix nColumnsInMatrix nIterations nRowsInTile nColumnsInTile [-v]\n";
        toBeReturned = 1;
    } else{
        //        srand(0xdeadbeef);
        double omega = atof(argv[1]);
        int nRowsInMatrix = (int)atoi(argv[2]);
        int nColumnsInMatrix = (int)atoi(argv[3]);
        int nIterations = (int)atoi(argv[4]);

        int nRowsInTile = (int)atoi(argv[5]);
        int nColumnsInTile = (int)atoi(argv[6]);

        int nRowsOfTiles = ceiling (nRowsInMatrix, nRowsInTile);
        int nColumnsOfTiles =ceiling (nColumnsInMatrix, nColumnsInTile);

        bool verbose = argc==8 && !strcmp("-v", argv[7]);

        double** inputArray = new double*[nRowsInMatrix];

        // not the best rand but this is a test program anyway
        for(int i = 0; i < nRowsInMatrix; ++i) {
            double* currRow = inputArray[i] = new double[nColumnsInMatrix];
            for(int j = 0; j < nColumnsInMatrix; ++j)
                //                currRow[j] = (rand() % 100) *0.01;
				currRow[j] = ((j*11) % 100) *0.01;
        }

        if(verbose) {
            printf("omega=%lf, M=%d, N=%d, nIterations=%d, nRowsInTile=%d and nColumnsInTile=%d\n",
                   omega, nRowsInMatrix, nColumnsInMatrix, nIterations, nRowsInTile, nColumnsInTile);
            printf("before\n");
            for(int i = 0; i < nRowsInMatrix; ++i) {
                for(int j = 0; j < nColumnsInMatrix-1; ++j)
                    printf("%lf, ",inputArray[i][j]);
                printf("%lf\n",inputArray[i][nColumnsInMatrix-1]);
            }
        }
        tbb::tick_count t0 = tbb::tick_count::now();

        for(int iteration = 0; iteration < nIterations; ++iteration) {
            sorContext graph;
            //						CnC::debug::trace_all(graph, "tiled sor");
            graph.omega.put(111, omega);
            graph.nRowsInTile.put(222, nRowsInTile);
            graph.nColumnsInTile.put(333, nColumnsInTile);
            graph.nRowsOfTiles.put(444, nRowsOfTiles);
            graph.nColumnsOfTiles.put(555, nColumnsOfTiles);

            graph.iterationSpace.put(666+iteration);
            for(int i = 0; i < nRowsInMatrix; ++i)
                for(int j = 0; j < nColumnsInMatrix; ++j)
                    graph.prevIterMatrix.put(makePair(i,j), inputArray[i][j]);


            graph.wait();
            // since execution finished I can now write over **inputArray

            for(int i = 0; i < nRowsInMatrix; ++i)
                for(int j = 0; j < nColumnsInMatrix; ++j)
                    graph.currIterMatrix.get(0x8000+makePair(i,j), inputArray[i][j]); 
        }

        tbb::tick_count t1 = tbb::tick_count::now();
        printf("Computed in %g seconds\n", (t1-t0).seconds());
        if(verbose) {
            printf("after\n");
            for(int i = 0; i < nRowsInMatrix; ++i) {
                for(int j = 0; j < nColumnsInMatrix-1; ++j)
                    printf("%lf, ",inputArray[i][j]);
                printf("%lf\n",inputArray[i][nColumnsInMatrix-1]);
            }
        }

        for(int i = 0; i < nRowsInMatrix; ++i)
            delete [] inputArray[i];
        delete [] inputArray;
    }
    return toBeReturned;
}
