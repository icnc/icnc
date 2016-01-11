#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif

#include "interface.h"
#include "wrapper.h"
#include "tuners.h"
#include "dgemm_graph.h"


// This is the high-level CnC representation of the double matrix-multiply.
//  (simple enough to be auto-generated).
// For each multiply we/the generator use/s a CnC graph as defined by the expert.
class dgemm_dgemm_context : public CnC::context< dgemm_dgemm_context >
{
public:
    // the input/output parameters are plain C-arrays as found in the orig code
    //  together with sizes etc.
    dgemm_dgemm_context( int _M = 0, int _N = 0, int _K = 0, double *_A = NULL,
                         double *_B = NULL, double *_C = NULL, double *_D = NULL,
                         double *_E  = NULL )
        : M(_M), N(_N), K(_K), A(_A), B(_B), C(_C), D(_D), E(_E),
          m_row_tuner(BLOCK_ROWS), m_col_tuner(N)
    {
        // Create shared collection and its tuner
        C_collection = new CnC::item_collection<RC, std::shared_ptr<Block<double>>, 
                row_wise_distribution_tuner>( *this, "sharedC", m_row_tuner);

        // Create a graph for each library function call. Connect them by sharing the collections
        // C=A*B
        graph1 = make_dgemm_graph<dgemm_dgemm_context, RC, std::shared_ptr<Block<double>>, double, row_wise_distribution_tuner>
            (A, B, C, M, M, M, *this, 
             (CnC::item_collection< RC, std::shared_ptr<Block<double>>, row_wise_distribution_tuner > *)NULL, false, 
             (CnC::item_collection< RC, std::shared_ptr<Block<double>>, col_wise_distribution_tuner > *)NULL, false,
             C_collection, true, 
             _ROW_BLOCK_CYCLIC_, (char*)"graph1",
             &m_row_tuner, &m_col_tuner, &m_row_tuner, &m_row_tuner );

        // E=C*D
        // Note that C is an output parameter for graph1 and an input to graph2
        // CnC takes care of the ordering dependencies.
        // FIXME Here we use one tuner for all arrays except B. This one tuner for
        // all is for convenience only. It might not be what we want for performance.
        graph2 = make_dgemm_graph<dgemm_dgemm_context, RC, std::shared_ptr<Block<double>>, double, row_wise_distribution_tuner>
            (C, D, E, M, M, M, *this, 
             C_collection, true, 
             (CnC::item_collection< RC, std::shared_ptr<Block<double>>, col_wise_distribution_tuner > *)NULL, false, 
             (CnC::item_collection< RC, std::shared_ptr<Block<double>>, row_wise_distribution_tuner > *)NULL, false,
             _ROW_BLOCK_CYCLIC_, (char*)"graph2",
             &m_row_tuner, &m_col_tuner, &m_row_tuner, &m_row_tuner );
    }
    
    void init() {
        graph1->init(3, M, N, K);
        graph2->init(3, M, N, K);
    } 

#ifdef _DIST_
    void serialize( CnC::serializer & s ) {
        s & M & N & K;
        if(s.is_unpacking()) {
            init();
        }
    }
#endif
    
    ~dgemm_dgemm_context()
    {
        delete graph2;
        delete graph1;
        delete C_collection;
    }

    // Graphs
    wrapper * graph1, * graph2;

private:
    // All input/output parameters of the graphs should be remembered 
    int M, N, K;
    double *A, *B, *C, *D, *E;

    // Collections for all the arrays that connect different library functions calls. 
    // The compiler needs to decide a type of data distribution between the 
    // functions so that they can work together. Here we simply use row bock cyclic.
    // NOTE: the shared collection has to be a pointer, since we pass in other
    // collections as pointers. They have to be all as objects, or all as pointers. 
    CnC::item_collection< RC, std::shared_ptr< Block< double > >, row_wise_distribution_tuner > *C_collection;
    
    // Tuners
    row_wise_distribution_tuner m_row_tuner;
    col_wise_distribution_tuner m_col_tuner;
};

/****************************** Interface functions ***************************/
extern "C"
void dgemm_dgemm( int M, int N, int K, double * A, double * B, double * C, 
                  double * D, double * E )
{
    // Create a context
    dgemm_dgemm_context ctxt( M, N, K, A, B, C, D, E );
    
    // Now run!
    ctxt.graph1->start();
    ctxt.graph2->start();

    ctxt.wait();
    
    // Now copy results back. The compiler directly points out which
    // graph should copy out its result.
    ctxt.graph2->copyout();
}

#ifdef _DIST_
static CnC::dist_cnc_init< dgemm_dgemm_context > * _dc;
#endif

extern "C" void initialize_CnC()
{
#ifdef _DIST_
    _dc = new CnC::dist_cnc_init< dgemm_dgemm_context >;
#endif
}
extern "C" void finalize_CnC()
{
#ifdef _DIST_
    delete _dc;
#endif
}
