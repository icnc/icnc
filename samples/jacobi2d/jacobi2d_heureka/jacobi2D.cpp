/*
Copyright (c) 2014, Habanero Extreme Scale Software Research Project, Rice University
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Rice University nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Rice University BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "jacobi2D.h"

using namespace Jacobi2D;

const double tolerance = 0.20;

static Jacobi2DCtx *parseArgs(int argc, char **argv) {
    int iterations = 200;
    int dataSize = 400;
    int blockSize = 100;
    int numBlocks = dataSize / blockSize;
    double epsilon = 5e-3;
    bool debug = false;
    int i = 1;
    while (i < argc) {
        const char *loopOptionKey = argv[i];
        if (strcmp(loopOptionKey, "-ds") == 0) {
            dataSize = atoi(argv[++i]);
        }
        else if (strcmp(loopOptionKey, "-bs") == 0) {
            blockSize = std::max(2, atoi(argv[++i]));
        }
        else if (strcmp(loopOptionKey, "-iter") == 0) {
            iterations = atoi(argv[++i]);
        }
        else if (strcmp(loopOptionKey, "-e") == 0) {
            epsilon = atof(argv[++i]);
        }
        else if (strcmp(loopOptionKey, "-debug") == 0 || strcmp(loopOptionKey, "-verbose") == 0) {
            debug = true;
        }
        i++;
    }
    numBlocks = dataSize / blockSize;
    assert(dataSize % blockSize == 0 && "Block size does not evenly divide the data size");
    return new Jacobi2DCtx(iterations, dataSize, blockSize, numBlocks, epsilon, debug);
}

static const char *argOutputFormatStr   = "%25s = %10s \n";
static const char *argOutputFormatFloat = "%25s = %10.5f \n";
static const char *argOutputFormatInt   = "%25s = %10d \n";

static void printArgs(Jacobi2DCtx &c) {
    printf(argOutputFormatInt, "Data Size", c.dataSize);
    printf(argOutputFormatInt, "Block Size", c.blockSize);
    printf(argOutputFormatFloat, "Epsilon", c.epsilon);
    printf(argOutputFormatInt, "Iterations", c.iterations);
    printf(argOutputFormatStr, "debug", c.debug ? "true" : "false");
}

void initializeBlocks(Jacobi2DCtx &c) {
    for (int i = 0; i < c.numBlocks; i++) {
        for (int j = 0; j < c.numBlocks; j++) {
            J2DTile *tile = new J2DTile(i, j, c);
            double **data = tile->data2D;
            if (i == 0) {
                for (int a = 0; a < c.blockSize; a++) {
                    data[0][a] = 1;
                }
            } else if (i == c.numBlocks - 1) {
                for (int a = 0; a < c.blockSize; a++) {
                    data[c.numBlocks - 1][a] = 1;
                }
            }
            if (j == 0) {
                for (int a = 0; a <c. blockSize; a++) {
                    data[a][0] = 1;
                }
            } else if (j == c.numBlocks - 1) {
                for (int a = 0; a <c. blockSize; a++) {
                    data[a][c.numBlocks - 1] = 1;
                }
            }

            c.m_blockitems.put(T3(0, i, j), tile);
        }
    }
    c.m_blockitems.put(T3(0, -1, -1), NULL);
}

int main(int argc, char **argv) {
    Jacobi2DCtx *c = parseArgs(argc, argv);
    printArgs(*c);

    initializeBlocks(*c);

    tbb::tick_count startTime = tbb::tick_count::now();

    c->m_itertags.put(1);
    c->wait();

    tbb::tick_count endTime = tbb::tick_count::now();

    int resultIndex = std::numeric_limits<int>::max();
    auto end = c->m_resultitems.end();
    for(auto ci = c->m_resultitems.begin(); ci != end; ci++) {
        int value = *ci->second;
        resultIndex = std::min(resultIndex, value);
    }

    printf("Eureka at iteration: %d\n", resultIndex);
    printf("Calculations took %fs\n", (endTime-startTime).seconds());

    // TODO - count how many tasks we skipped
    // System.out.println("Tasks bypassed: " + oneShotEureka.get());

    // exit without freeing the graph or it'll complain about suspended
    // steps that never executed (because we cancelled them!)
    return 0;
}
