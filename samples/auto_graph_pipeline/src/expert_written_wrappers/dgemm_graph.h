#ifndef DGEMM_GRAPH_H
#define DGEMM_GRAPH_H

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <cnc/debug.h>
#include <cnc/reduce.h>
#include <assert.h>
#include <stdarg.h>
#include <memory>

#ifdef USE_MKL
    #include "mkl.h"
#else
    #include "cblas.h"
#endif

#include "wrapper.h"
#include "tuners.h"

// The multiply step invokes the original cblas_dgemm function to do the
// actual computation.
class multiply
{
public:
    template< typename graph_type >
    int execute(const RC & tag , const graph_type & graph) const {
        // A, B, C block. 
        std::shared_ptr<Block<double>> A_block; // read-only from collection
        std::shared_ptr<Block<double>> B_block; // read-only from collection
        std::shared_ptr<Block<double>> C_block = std::make_shared<Block<double>>(BLOCK_ROWS, graph.N);
    
        graph.A_collection->get(tag, A_block);
        graph.B_collection->get({0, 0}, B_block);
            
        double *A = A_block->data();
        double *B = B_block->data();
        double *C = C_block->data();
                
        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    BLOCK_ROWS, graph.N, graph.K, 1.0, A, graph.K, B, graph.N, 0.0, C, graph.N);
                    
        graph.C_collection->put(tag, C_block);
    
        return CnC::CNC_Success;
    }
};

// we allow providing one tuner for each collection to allow most flexible tuning/configuration.
template<typename Ctx, typename tag_type, typename block_type, typename data_type, 
         typename tuner_typeA, typename tuner_typeB, typename tuner_typeC, typename tuner_typeS >
class dgemm_graph : public wrapper, public CnC::Internal::no_copy
{
public:
    // A is an array of M*K, B of K*N, C of M*N
    dgemm_graph(const data_type *_A, const data_type *_B, data_type *_C,
                const int _M, const int _N, const int _K, CnC::context<Ctx> & _ctx, 
                CnC::item_collection< tag_type, block_type, tuner_typeA > *_A_collection, bool _A_is_SDSU, 
                CnC::item_collection< tag_type, block_type, tuner_typeB > *_B_collection, bool _B_is_SDSU, 
                CnC::item_collection< tag_type, block_type, tuner_typeC > *_C_collection, bool _C_is_SDSU, 
                distribution_type _dt, char* _graph_name,
                tuner_typeA * tunera,  tuner_typeB * tunerb,  tuner_typeC * tunerc, tuner_typeS * tuners)
    :   wrapper(_ctx), 
        m_tunerA(tunera), m_tunerB(tunerb), m_tunerC(tunerc), m_tunerS(tuners),
        A(_A), B(_B), C(_C), M(_M), N(_N), K(_K), 
        tags(_ctx),
        A_collection(_A_collection), 
        B_collection(_B_collection), 
        C_collection(_C_collection),
        C_is_SDSU(_C_is_SDSU), dt(_dt),
        multiply_steps(_ctx, "multiply", multiply(), *m_tunerS)
    {
                
        assert(dt == _ROW_BLOCK_CYCLIC_);
        // assign names for easier debugging
        strcpy(A_name, _graph_name);
        strcat(A_name, "_A");
        strcpy(B_name, _graph_name);
        strcat(B_name, "_B");
        strcpy(C_name, _graph_name);
        strcat(C_name, "_C");
                 
        fill_A = false; // at init time we only fill blocks on the host
        if (A_collection == NULL) {
            assert( m_tunerA );
            A_collection = new CnC::item_collection<tag_type, block_type, tuner_typeA>(_ctx, A_name, *m_tunerA);
            if (CnC::tuner_base::myPid() == 0) {
                // Fill A for host process. Other processes should get data from host
                fill_A = true;
            }
        }
        
        fill_B = false; // at init time we only fill blocks on the host
        if (B_collection == NULL) {
            assert( m_tunerB );
            B_collection = new CnC::item_collection<tag_type, block_type, tuner_typeB>(_ctx, B_name, *m_tunerB);
            if (CnC::tuner_base::myPid() == 0) {
                // Fill B for host process. Other processes should get data from host
                fill_B = true;
            }
        }
        
        if (C_collection == NULL) {
            assert( m_tunerC );
            C_collection = new CnC::item_collection<tag_type, block_type, tuner_typeC>(_ctx, C_name, *m_tunerC);
        }

        // Define the control flow
        tags.prescribes(multiply_steps, *this);
        // Define the data flow
        multiply_steps.consumes(*A_collection);
        multiply_steps.consumes(*B_collection);
        multiply_steps.produces(*C_collection);
         
#ifdef DEBUG_CNC
        CnC::debug::collect_scheduler_statistics (_ctx);
        CnC::debug::trace (*A_collection,3);
        CnC::debug::trace (*B_collection,3);
        CnC::debug::trace (*C_collection,3);
#endif
    }
    ~dgemm_graph()
    {
    }
    
    void start() {
        // Only host can reach here
        assert(CnC::tuner_base::myPid() == 0);
        
        assert(this->M % BLOCK_ROWS == 0);
        int num_blocks = this->M / BLOCK_ROWS;
        if (fill_A) {
            // Fill blocks of A and push them into item collections.
            // TODO: avoid copying from raw data to collections. Just modify the pointers
            // inside the collections to point to the right positions in the raw data
            for(int i = 0; i < num_blocks; i++) {
                std::shared_ptr<Block<double>> A_block = std::make_shared<Block<double>>(BLOCK_ROWS, this->K);
                memcpy(A_block->data(), this->A + i * BLOCK_ROWS * this->K, BLOCK_ROWS * this->K * sizeof(data_type));    
                tag_type tag = {i * BLOCK_ROWS, 0};
                A_collection->put(tag, A_block);
            }
        }

        if (fill_B) {
            // Put the entire B into a block.
            std::shared_ptr<Block<double>> B_block = std::make_shared<Block<double>>(this->K, this->N);
            memcpy(B_block->data(), this->B, this->K * this->N * sizeof(data_type));
            tag_type tag = {0, 0};
            B_collection->put(tag, B_block);
        }
        
        // Start execution
        for(int i = 0; i < num_blocks; i++) {
            tag_type tag = {i * BLOCK_ROWS, 0};
            tags.put(tag);
        }

        // A potential wait goes somewhere on the outside
    }

    void copyout()
    {
        // Only host can reach here
        assert(CnC::tuner_base::myPid() == 0);
        assert(C != NULL);
        assert(M % BLOCK_ROWS == 0);

        int num_blocks = M / BLOCK_ROWS;
        for(int i = 0; i < num_blocks; i++) {
            std::shared_ptr<Block<double>> temp_block;
            C_collection->get({i * BLOCK_ROWS, 0}, temp_block);
            double *temp = temp_block->data();
            memcpy((void*)(C + i * BLOCK_ROWS * N), (void*) temp, BLOCK_ROWS * N * sizeof(double));
        }
    }
    
    void init(int total, ...) {
        va_list vl;
        va_start(vl, total);
        assert(total == 3);
        M = va_arg(vl, int);
        N = va_arg(vl, int);
        K = va_arg(vl, int);
        va_end(vl);
    }
    
    friend class multiply;
    
private:
    // tuners, one for each collection
    tuner_typeA * m_tunerA;
    tuner_typeB * m_tunerB;
    tuner_typeC * m_tunerC;
    tuner_typeS * m_tunerS;
    // raw matrices A, B and C
    const data_type* A;
    const data_type* B;
    const data_type* C;
    // matrix sizes
    int M;
    int N;
    int K;
    
    bool  fill_A;
    bool  fill_B;
    bool  C_is_SDSU; // SDSU represents Single-Definition Single-Use
    distribution_type dt;

    // tiles/blocks of matrices A, B, C
    CnC::item_collection<tag_type, block_type, tuner_typeA> *A_collection;
    CnC::item_collection<tag_type, block_type, tuner_typeB> *B_collection;
    CnC::item_collection<tag_type, block_type, tuner_typeC> *C_collection;
    CnC::tag_collection<tag_type>                            tags;
    CnC::step_collection<multiply, tuner_typeS>              multiply_steps;
    
    // For debugging purpose
    char A_name[10];
    char B_name[10];
    char C_name[10];
};

/********************         Interface functions     **********************/
// For each lbrary function f, we have such an interface named "make_f_graph"
// The parameters are: original parameters, followed by additional parameters: 
// context, [collection, is_SDSU]+, distribution type, graph name, tuners
// A, B, C can have different tuners and most probably we want them to have 
// different tuners. The auto-generated code has to make sure the different
// distribution plans defined by the tuners' consumed_on/compute_on functions
// cooperate correctly.
template<typename Ctx, typename tag_type, typename block_type, typename data_type, 
         typename tuner_typeA, typename tuner_typeB, typename tuner_typeC, typename tuner_typeS >
wrapper *make_dgemm_graph(const data_type *_A, const data_type *_B, data_type *_C,
                               const int _M, const int _N, const int _K, CnC::context<Ctx> & _ctx, 
                               CnC::item_collection< tag_type, block_type, tuner_typeA > *_A_collection, bool _A_is_SDSU, 
                               CnC::item_collection< tag_type, block_type, tuner_typeB > *_B_collection, bool _B_is_SDSU, 
                               CnC::item_collection< tag_type, block_type, tuner_typeC > *_C_collection, bool _C_is_SDSU, 
                               distribution_type _dt, char* _graph_name,
                               tuner_typeA * tunera,  tuner_typeB * tunerb, tuner_typeC * tunerc, tuner_typeS * tuners)
{
    return new dgemm_graph<Ctx, tag_type, block_type, data_type, tuner_typeA, tuner_typeB, tuner_typeC, tuner_typeS>(_A, _B, _C, _M, _N, _K, _ctx, _A_collection, _A_is_SDSU, _B_collection, _B_is_SDSU, _C_collection, _C_is_SDSU, _dt, _graph_name, tunera, tunerb, tunerc, tuners);
}

#endif


