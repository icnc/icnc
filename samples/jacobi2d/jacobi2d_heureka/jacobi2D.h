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

#ifndef JACOBI_2D_H
#define JACOBI_2D_H

#include <iostream>
#include <cnc/cnc.h>
#include <cnc/debug.h>
#include "tbb/tick_count.h"

#include <utility>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <limits>

namespace Jacobi2D {

    //////////////////////////////
    // Tag types
    //////////////////////////////

    typedef std::pair<int,int> pair;
    static inline int first(const pair &p) { return p.first; }
    static inline int second(const pair &p) { return p.second; }
    #define T2(x, y) std::make_pair(x, y)

    typedef std::pair<int,std::pair<int, int>> triple;
    static inline int first(const triple &t) { return t.first; }
    static inline int second(const triple &t) { return t.second.first; }
    static inline int third(const triple &t) { return t.second.second; }
    #define T3(x, y, z) std::make_pair(x, std::make_pair(y, z))

    //////////////////////////////
    // Graph
    //////////////////////////////

    struct J2DTile;
    struct Jacobi2DCtx;

    struct IterationStep { int execute(int tag, Jacobi2DCtx &c) const; };
    struct BlockStep { int execute(const triple &tag, Jacobi2DCtx &c) const; };
    struct DeltaStep { int execute(int tag, Jacobi2DCtx &c) const; };

    // Delta steps should have higher priority in Eureka code
    struct delta_step_tuner : public CnC::cancel_tuner<int,false> {
        delta_step_tuner(Jacobi2DCtx &c) : CnC::cancel_tuner<int,false>(c) {}
        int priority(int /*tag*/, const Jacobi2DCtx &/*ctx*/) const {
            return 2;
        }
    };

    struct Jacobi2DCtx: public CnC::context<Jacobi2DCtx> {
        // Tuners
        typedef CnC::cancel_tuner<int,false> iter_tuner_type;
        iter_tuner_type m_iter_tuner;
        #ifdef USE_CNC_CANCEL
        typedef delta_step_tuner delta_tuner_type;
        #else
        typedef CnC::cancel_tuner<int,false> delta_tuner_type;
        #endif
        delta_tuner_type m_delta_tuner;
        typedef CnC::cancel_tuner<triple,false> block_tuner_type;
        block_tuner_type m_block_tuner;

        // Step collection
        CnC::step_collection<IterationStep, iter_tuner_type> m_itersteps;
        CnC::step_collection<BlockStep, block_tuner_type> m_blocksteps;
        CnC::step_collection<DeltaStep, delta_tuner_type> m_deltasteps;

        // Item collections
        CnC::item_collection<triple, const J2DTile*> m_blockitems;
        CnC::item_collection<int, int> m_resultitems;

        // Tag collections
        CnC::tag_collection<int> m_itertags;
        CnC::tag_collection<triple> m_blocktags;
        CnC::tag_collection<int> m_deltatags;

        // Configuration
        const int iterations, dataSize, blockSize, numBlocks;
        const double epsilon;
        const bool debug;

        // Context constructor
        Jacobi2DCtx(int i, int ds, int bs, int nb, double ep, bool dbg)
            : CnC::context<Jacobi2DCtx>(),
              // init tuners
              m_iter_tuner(*this),
              m_block_tuner(*this),
              m_delta_tuner(*this),
              // init step colls
              m_itersteps(*this, m_iter_tuner, "(Iter)"),
              m_blocksteps(*this, m_block_tuner, "(Block)"),         
              m_deltasteps(*this, m_delta_tuner, "(Delta)"),
              // Initialize each item collection
              m_blockitems(*this, "[Block]"),
              m_resultitems(*this, "[Res]"),
              // Initialize each tag collection
              m_itertags(*this, "<Iter>"),
              m_blocktags(*this, "<Block>"),
              m_deltatags(*this, "<Delta>"),
              // Graph parameters
              iterations(i),
              dataSize(ds),
              blockSize(bs),
              numBlocks(nb),
              epsilon(ep),
              debug(dbg)
        {
            // Prescriptions
            m_itertags.prescribes(m_itersteps, *this);
            m_blocktags.prescribes(m_blocksteps, *this);
            m_deltatags.prescribes(m_deltasteps, *this);

            // producer/consumer relations
            m_blocksteps.consumes(m_blockitems);
            m_blocksteps.produces(m_blockitems);
            m_deltasteps.consumes(m_blockitems);
            m_deltasteps.produces(m_resultitems);

            #if 0 // Debug
            CnC::debug::collect_scheduler_statistics(*this);
            CnC::debug::trace_all( *this );
            #endif
        }

    };

    //////////////////////////////
    // Data types
    //////////////////////////////

    struct J2DTile {
        const int startRow;
        const int endRow;
        const int startCol;
        const int endCol;
        double **data2D;
        double *data;

        J2DTile(int i, int j, Jacobi2DCtx &c):
            startRow(i*c.blockSize), endRow((i+1)*c.blockSize),
            startCol(j*c.blockSize), endCol((j+1)*c.blockSize)
        {
            const int rows = c.blockSize;
            const int cols = c.blockSize;
            data = new double[rows * cols];
            data2D = new double*[rows];
            for (int r=0; r<rows; r++) {
                data2D[r] = &data[r*cols];
            }
        }

        ~J2DTile() {
            delete[] data2D;
            delete[] data;
        }
    };

    static inline double tileGet(const J2DTile *t, int r, int c) {
        return (!t) ? 0 : t->data2D[r - t->startRow][c - t->startCol];
    }

    static inline double tileDelta(const J2DTile *a, const J2DTile *b) {
        if (!a) return tileDelta(b, a);
        double localDelta = 0;
        for (int i = a->startRow; i < a->endRow; i++) {
            for (int j = a->startCol; j < a->endCol; j++) {
                double delta = std::fabs(tileGet(a, i, j) - tileGet(b, i, j));
                localDelta = std::max(localDelta, delta);
            }
        }
        return localDelta;
    }

}

#endif /* JACOBI_2D_H */
