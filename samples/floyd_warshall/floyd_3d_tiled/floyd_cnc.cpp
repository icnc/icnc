/*Copyright (c) 2013-, Ravi Teja Mullapudi, CSA Indian Institue of Science
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of the Indian Institute of Science nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../common/cnc_common.h"
#include "../common/floyd_base.h"
#include "floyd_tuners.h"

struct context_dist_3d : public CnC::context< context_dist_3d >
{
    int matrix_size, block_size;
    // Tuner
    tuner_dist_3d m_tuner;
    // Tag Collections
    CnC::tag_collection< int >                                                    m_singleton, m_tag_control;
    CnC::tag_collection<Triple>                                                   m_tags;
    // Step Collections
    CnC::step_collection< node_gen_step_dist_3d>                                  m_node_gen_step;
    CnC::step_collection< tag_gen_step_dist_3d, tuner_dist_3d>                    m_tag_gen_steps;
    CnC::step_collection< compute_step_dist_3d, tuner_dist_3d>                    m_compute_steps;
    // Item Collections
    CnC::item_collection<Triple, std::shared_ptr<Tile2d<double> >, tuner_dist_3d> m_blocks;

    context_dist_3d(int _matrix_size = 0, int _block_size = 0) 
        : CnC::context< context_dist_3d >(),
        // Initialize tuner  
        m_tuner(*this, ROW_CYCLIC, _matrix_size, _block_size),
        // Initialize step collection
        m_node_gen_step(*this, m_tuner),        
        m_compute_steps( *this, m_tuner),
        m_tag_gen_steps( *this, m_tuner),
        // Initialize each item collection
        m_blocks( *this, m_tuner),
        // Initialize each tag collection
        m_tag_control(*this),
        m_tags( *this ),
        m_singleton( *this),
        matrix_size(_matrix_size),
        block_size(_block_size)
    {

        m_singleton.prescribes(m_node_gen_step, *this);
        m_tag_control.prescribes(m_tag_gen_steps, *this );
        m_tags.prescribes(m_compute_steps, *this );

        m_node_gen_step.controls(m_tag_control);
        m_tag_gen_steps.controls(m_tags);

        m_compute_steps.produces(m_blocks);

        // Scheduler Statistics
        //CnC::debug::collect_scheduler_statistics(*this);
        //CnC::debug::init_timer(true);
        //CnC::debug::time(m_compute_steps);
        //CnC::debug::trace(m_compute_steps);
        //CnC::debug::trace(m_tags);
        //CnC::debug::trace(m_tags);
        //CnC::debug::trace(m_blocks, 100);
        //CnC::debug::trace_all(*this, 10);
    }
    ~context_dist_3d()
    {
        //CnC::debug::finalize_timer("log");
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & matrix_size & block_size & m_tuner;
    }
#endif
};

template< class dependency_consumer >
void tuner_dist_3d::depends( const Triple & tag, context_dist_3d & c, dependency_consumer & dC ) const
{
    int k = tag[0], i = tag[1], j = tag[2];
    dC.depends( c.m_blocks, Triple(k, i, j));

    if (k != j)
        dC.depends( c.m_blocks, Triple(k + 1, i, k));

    if (k != i)
        dC.depends( c.m_blocks, Triple(k + 1, k, j));
}

int node_gen_step_dist_3d::execute(const int & t, context_dist_3d & c ) const
{
    int p = c.m_tuner.numProcs();
    for(int k = 0; k < p; k++) {
        c.m_tag_control.put(k);
    }
    return CnC::CNC_Success;
}

int tag_gen_step_dist_3d::execute(const int & p, context_dist_3d & c ) const
{
    int num_blocks = c.matrix_size/c.block_size;
    for(int k = 0; k < num_blocks; k++) {
        for (int i = 0; i < num_blocks; i++) {
            for (int j = 0; j < num_blocks; j++) {   
                if (p == (j%c.m_tuner.numProcs())) {
                    c.m_tags.put(Triple(k, i, j));
                }
            }
        }
    }
    return CnC::CNC_Success;
}

int compute_step_dist_3d::execute(const Triple &t, context_dist_3d & c ) const
{
    int k = t[0], i = t[1], j = t[2]; 
    std::shared_ptr<Tile2d<double> > block_ij, block_rows, block_cols;

    c.m_blocks.get(Triple(k, i, j), block_ij);

    std::shared_ptr<Tile2d<double> > new_block = std::make_shared<Tile2d<double> >(c.block_size, c.block_size);
    for (int r = 0; r < c.block_size; r++) {
        for (int p = 0; p < c.block_size; p++) {
                (*new_block)(r, p) = (*block_ij)(r, p);
        }
    }

    if ((k != i))     
        c.m_blocks.get(Triple(k + 1, k, j), block_rows);
    else 
        block_rows = new_block;

    if ((k != j))
        c.m_blocks.get(Triple(k + 1, i, k), block_cols);
    else 
        block_cols = new_block;

    for (int r = 0; r < c.block_size; r++) { 
        for (int p = 0; p < c.block_size; p++) {
            double cols_pr = (*block_cols)(p, r);
            for (int q = 0; q < c.block_size; q++) {    
                (*new_block)(p, q) = min(cols_pr + (*block_rows)(r, q), (*new_block)(p, q));
            }
        }
    }

    c.m_blocks.put(Triple(k+1, i, j), new_block);

    return CnC::CNC_Success;
}

void floyd_dist_3d_cnc(double* path_distance_matrix, int matrix_size, int block_size)
{
    double t_start, t_end;
    context_dist_3d c(matrix_size, block_size);
    int num_blocks = matrix_size/block_size;

    printf("Num Tasks (%d * %d * %d) = %d\n", num_blocks, num_blocks, num_blocks, num_blocks * num_blocks * num_blocks);

    t_start = rtclock();
    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < num_blocks; j++) {
           std::shared_ptr<Tile2d<double> > tile = std::make_shared<Tile2d<double> >(block_size, block_size);
            for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) {
                for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) {
                    (*tile)( t_i, t_j ) = path_distance_matrix[org_i*matrix_size + org_j];
                }
            }
            c.m_blocks.put(Triple(0, i, j), tile);
        }
    }
    t_end = rtclock();
    printf("CnC tile creation time %0.6lfs seconds\n", t_end - t_start);

    c.m_singleton.put(1);

    t_start = rtclock();            
    c.wait(); 
    t_end = rtclock();
    printf("CnC task execution wait time %0.6lfs seconds\n", t_end - t_start);

    // Gather data back
    printf("Block Collection Size %ld\n", c.m_blocks.size());
    t_start = rtclock();
    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < num_blocks; j++) {
            std::shared_ptr<Tile2d<double> > tile;
            c.m_blocks.get(Triple(num_blocks, i , j), tile);
            for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) {
                for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) {
                    path_distance_matrix[org_i*matrix_size + org_j] = (*tile)( t_i, t_j );
                }
            }
        }
    }
    t_end = rtclock();
    printf("CnC data gather time %0.6lfs seconds\n", t_end - t_start);
}

void parse_args(int argc, char **argv, int *p_matrix_size, int *p_block_size, 
                bool *p_run_seq, bool *p_run_omp, bool *p_run_cnc_dist_3d, 
                bool *p_comp_results)
{
    int i;  
    *p_matrix_size = 0; *p_block_size = 0;  
    *p_run_omp = false; *p_run_seq = false; *p_run_cnc_dist_3d = false; 
    *p_comp_results = false;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            char *argptr;
            *p_matrix_size = strtol(argv[++i], &argptr, 10);
        }
        if (strcmp(argv[i], "-b") == 0) {
            char *argptr;
            *p_block_size = strtol(argv[++i], &argptr, 10);
        }
        if (strcmp(argv[i], "-seq") == 0) 
            *p_run_seq = true;
        if (strcmp(argv[i], "-omp") == 0)
            *p_run_omp = true;
        if (strcmp(argv[i], "-cnc") == 0)
            *p_run_cnc_dist_3d = true;
        if (strcmp(argv[i], "-check") == 0)
            *p_comp_results = true;
        if (strcmp(argv[i], "-h") == 0)
            printf("usage : %s -n matrix_size -b block_size [-seq][-omp][-cnc][-check]\n", argv[0]);
    }
    if( *p_matrix_size == 0 || *p_block_size == 0 ) {
        printf("usage : %s -n matrix_size -b block_size [-seq][-omp][-cnc][-check]\n", argv[0]);
        exit( 11 );
    }
    assert((*p_matrix_size)%(*p_block_size) == 0);
}

int main(int argc, char **argv)
{
#ifdef _DIST_
    CnC::dist_cnc_init<context_dist_3d> _dinit;
#endif
    int ret;
    int i, j, k;
    double t_start, t_end;
   
    int    matrix_size = 0;
    int    block_size = 0;
    bool   run_seq, run_omp, run_cnc_dist_3d, comp_results;
    double *path_distance_matrix = NULL;

    parse_args(argc, argv, &matrix_size, &block_size, &run_seq, &run_omp, 
               &run_cnc_dist_3d, &comp_results);

    path_distance_matrix = (double*) malloc (sizeof(double) * (matrix_size) * (matrix_size));

    double *seq_results = NULL;
    
    if (run_seq || comp_results) {
        double *matrix_used = path_distance_matrix;
        if (comp_results) {
            seq_results = (double*) malloc (sizeof(double) * (matrix_size) * (matrix_size));
            matrix_used = seq_results;
        }
        init_square_matrix(matrix_used, matrix_size);
        t_start = rtclock();
        floyd_seq(matrix_used, matrix_size);
        t_end = rtclock();

        //print_square_matrix(matrix_used, matrix_size);
        printf("Sequential Time %0.6lfs seconds\n\n", t_end - t_start);
    }

    if (run_omp) {
        init_square_matrix(path_distance_matrix, matrix_size);
        t_start = rtclock();
        floyd_tiled_omp(path_distance_matrix, matrix_size, block_size);
        t_end = rtclock();

        //print_square_matrix(path_distance_matrix, matrix_size);
        if (comp_results)
            check_square_matrices(path_distance_matrix, seq_results, matrix_size);
        printf("OpenMP Time %0.6lfs seconds\n\n", t_end - t_start);
    }

    if (run_cnc_dist_3d) {
        init_square_matrix(path_distance_matrix, matrix_size);
        t_start = rtclock();
        floyd_dist_3d_cnc(path_distance_matrix, matrix_size, block_size); 
        t_end = rtclock();

        //print_square_matrix(path_distance_matrix, matrix_size);
        if (comp_results) {
            if( check_square_matrices(path_distance_matrix, seq_results, matrix_size) >  0 ) {
                exit( 99 );
            }
        }
        printf("CnC Time %0.6lfs seconds\n\n", t_end - t_start);
    }

    printf("Done\n");
    if (comp_results)
       free(seq_results); 
    free(path_distance_matrix);
    return 0;
}
