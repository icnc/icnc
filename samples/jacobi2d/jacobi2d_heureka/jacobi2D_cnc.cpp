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
#include <limits>

using namespace Jacobi2D;

double get(
        const J2DTile &dest,
        const J2DTile &src,
        const J2DTile &top,
        const J2DTile &left,
        const J2DTile &bottom,
        const J2DTile &right,
        const int i, const int j) {
    if (i >= dest.startRow && i < dest.endRow && j >= dest.startCol && j < dest.endCol) {
        return tileGet(&src, i, j);
    } else if (i < dest.startRow && j >= dest.startCol && j < dest.endCol) {
        return tileGet(&top, i, j);
    } else if (i >= dest.startRow && i < dest.endRow && j < dest.startCol) {
        return tileGet(&left, i, j);
    } else if (i >= dest.endRow && j >= dest.startCol && j < dest.endCol) {
        return tileGet(&bottom, i, j);
    } else if (i >= dest.startRow && i < dest.endRow && j >= dest.endCol) {
        return tileGet(&right, i, j);
    }
    assert(!"unreachable"); return std::numeric_limits<double>::quiet_NaN();
}

void jacobiIterationKernel(
        const J2DTile &dest,
        const J2DTile &src,
        const J2DTile &top,
        const J2DTile &left,
        const J2DTile &bottom,
        const J2DTile &right) {
    for (int i = dest.startRow; i < dest.endRow; i++) {
        for (int j = dest.startCol; j < dest.endCol; j++) {

            dest.data2D[i - dest.startRow][j - dest.startCol] = 0.25 * (
                    get(dest, src, top, left, bottom, right, i, j - 1) +
                            get(dest, src, top, left, bottom, right, i - 1, j) +
                            get(dest, src, top, left, bottom, right, i, j + 1) +
                            get(dest, src, top, left, bottom, right, i + 1, j));

        }
    }
}

int Jacobi2D::IterationStep::execute(int tag, Jacobi2DCtx &c) const {
    if (c.debug) {
        printf("IterationStep: %d\n", tag);
    }

    const int iteration = tag;
    const int numBlocks = c.numBlocks;

    for (int i = 0; i < numBlocks; i++) {
        for (int j = 0; j < numBlocks; j++) {
            c.m_blocktags.put(T3(iteration, i, j));
        }
    }

    c.m_deltatags.put(tag);

    if (iteration < c.iterations) {
        c.m_itertags.put(iteration + 1);
    }

    return CnC::CNC_Success;
}

int Jacobi2D::BlockStep::execute(const triple &tag, Jacobi2DCtx &c) const {
    if (c.debug) {
        printf("BlockStep: %d\n", tag);
    }

    const int iteration = first(tag);
    const int i = second(tag);
    const int j = third(tag);

    const int prevIteration = iteration - 1;

    const J2DTile *old;
    c.m_blockitems.get(T3(prevIteration, i, j), old);

    const J2DTile *top;
    if (i > 0) { // top
        c.m_blockitems.get(T3(prevIteration, i - 1, j), top);
    } else {
        c.m_blockitems.get(T3(0, -1, -1), top);
    }

    const J2DTile *left;
    if (j > 0) { // left
        c.m_blockitems.get(T3(prevIteration, i, j - 1), left);
    } else {
        c.m_blockitems.get(T3(0, -1, -1), left);
    }

    const J2DTile *bottom;
    if (i < c.numBlocks - 1) { // bottom
        c.m_blockitems.get(T3(prevIteration, i + 1, j), bottom);
    } else {
        c.m_blockitems.get(T3(0, -1, -1), bottom);
    }

    const J2DTile *right;
    if (j < c.numBlocks - 1) { // right
        c.m_blockitems.get(T3(prevIteration, i, j + 1), right);
    } else {
        c.m_blockitems.get(T3(0, -1, -1), right);
    }

    const J2DTile *dest = new J2DTile(i, j, c);

    jacobiIterationKernel(*dest, *old, *top, *left, *bottom, *right);

    c.m_blockitems.put(tag, dest);


    return CnC::CNC_Success;
}

int Jacobi2D::DeltaStep::execute(int tag, Jacobi2DCtx &c) const {
    if (c.debug) {
        printf("DeltaStep: %d\n", tag);
    }

    const int iteration = tag;
    const int prevIteration = iteration - 1;
    double localDelta = 0;

    for (int i = 0; i < c.numBlocks; i++) {
        for (int j = 0; j < c.numBlocks; j++) {
            const J2DTile *old;
            c.m_blockitems.get(T3(prevIteration, i, j), old);

            const J2DTile *cur;
            c.m_blockitems.get(T3(iteration, i, j), cur);

            const double loopDelta = tileDelta(old, cur);
            localDelta = std::max(localDelta, loopDelta);
        }
    }

    if (localDelta < c.epsilon) {
        if (c.debug) {
            printf("Eureka! Iteration-%d delta: %f\n", iteration, localDelta);
        }
        #ifdef USE_CNC_CANCEL
        // Cancel the remaining work
        c.m_iter_tuner.cancel_all();
        c.m_block_tuner.cancel_all();
        for (int i=iteration+1; i<c.iterations; i++) {
            c.m_delta_tuner.cancel(i);
        }
        #endif
        c.m_resultitems.put(tag, iteration);
    } else {
        if (c.debug) {
            printf("Iteration-%d delta: %f\n", iteration, localDelta);
        }
    }

    return CnC::CNC_Success;
}
