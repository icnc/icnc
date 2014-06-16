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
#include "floyd_tuners.h"
#include "../common/floyd_base.h"

struct context_dist_2d : public CnC::context< context_dist_2d >
{
    int matrix_size, block_size;
    // Tuner
    tuner_dist_2d m_tuner;
    tuner_col_transfer m_col_transfer_tuner;
    tuner_row_transfer m_row_transfer_tuner;
    // Tag Collections
    CnC::tag_collection< int >                                                         m_singleton, m_tag_control;
    CnC::tag_collection<Triple>                                                        m_tags;
    // Step Collections
    CnC::step_collection< node_gen_step_dist_2d>                                       m_node_gen_step;
    CnC::step_collection< tag_gen_step_dist_2d, tuner_dist_2d>                         m_tag_gen_steps;
    CnC::step_collection< compute_step_dist_2d, tuner_dist_2d>                         m_compute_steps;
    // Item Collections
    CnC::item_collection<Triple, std::shared_ptr<Tile2d<double> >, tuner_dist_2d>      m_blocks;
    CnC::item_collection<Pair, std::shared_ptr<Tile2d<double> >, tuner_col_transfer>   m_col_transfer;
    CnC::item_collection<Pair, std::shared_ptr<Tile2d<double> >, tuner_row_transfer>   m_row_transfer;

    context_dist_2d(int _matrix_size = 0, int _block_size = 0) 
        : CnC::context< context_dist_2d >(),
        // Initialize tuner  
        m_tuner(*this, ROW_CYCLIC, _matrix_size, _block_size),
        m_row_transfer_tuner(*this, ROW_CYCLIC, _matrix_size, _block_size),
        m_col_transfer_tuner(*this, ROW_CYCLIC, _matrix_size, _block_size),
        // Initialize step collection
        m_node_gen_step(*this, m_tuner),        
        m_compute_steps( *this, m_tuner),
        m_tag_gen_steps( *this, m_tuner),
        // Initialize each item collection
        m_blocks( *this, m_tuner),
        m_col_transfer( *this, m_col_transfer_tuner),
        m_row_transfer( *this, m_row_transfer_tuner),
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

        m_compute_steps.consumes(m_blocks);
        m_compute_steps.consumes(m_col_transfer);
        m_compute_steps.consumes(m_row_transfer);

        m_compute_steps.produces(m_blocks);
        m_compute_steps.produces(m_col_transfer);
        m_compute_steps.produces(m_row_transfer);

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
    ~context_dist_2d()
    {
        //CnC::debug::finalize_timer("log");
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & matrix_size & block_size & m_tuner & m_col_transfer_tuner & m_row_transfer_tuner;
    }
#endif
};

int node_gen_step_dist_2d::execute(const int & t, context_dist_2d & c ) const
{
    int p = c.m_tuner.numProcs();
    for(int k = 0; k < p; k++) {
        c.m_tag_control.put(k);
    }
    return CnC::CNC_Success;
}

int tag_gen_step_dist_2d::execute(const int & p, context_dist_2d & c ) const
{
    int num_blocks = c.matrix_size/c.block_size;
    for(int k = 0; k < c.matrix_size; k++) {
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

int compute_step_dist_2d::execute(const Triple &t, context_dist_2d & c ) const
{
    int k = t[0], i = t[1], j = t[2]; 
    std::shared_ptr<Tile2d<double> > block_ij, block_kj, block_ik;

    c.m_blocks.get(Triple(k, i, j), block_ij);

    if ((k/c.block_size != i))   
        c.m_row_transfer.get(Pair(k, j), block_kj);
    else
        block_kj = block_ij;

    if ((k/c.block_size != j))
        c.m_col_transfer.get(Pair(k, i), block_ik);
    else
        block_ik = block_ij;

    int r1 = (k/c.block_size != i) ? 0 : k%c.block_size;
    int r2 = (k/c.block_size != j) ? 0 : k%c.block_size;
    for (int p = 0; p < c.block_size; p++) {
        for (int q = 0; q < c.block_size; q++) {    
            (*block_ij)(p, q) = min((*block_ik)(p, r2) + (*block_kj)(r1, q), (*block_ij)(p, q));
        }
    }

    if ((k+1)/c.block_size == i) {
        std::shared_ptr<Tile2d<double> > row_tile = std::make_shared<Tile2d<double> >(1, c.block_size);
        for (int p = 0; p < c.block_size; p++) {
            (*row_tile)(0, p) = (*block_ij)((k+1)%c.block_size, p);
        }
        c.m_row_transfer.put(Pair(k+1, j), row_tile);
    }

    if ((k+1)/c.block_size == j) {
        std::shared_ptr<Tile2d<double> > col_tile = std::make_shared<Tile2d<double> >(c.block_size, 1);
        for (int p = 0; p < c.block_size; p++) {
            (*col_tile)(p, 0) = (*block_ij)(p, (k+1)%c.block_size);
        }
        c.m_col_transfer.put(Pair(k+1, i), col_tile);
    }

    c.m_blocks.put(Triple(k+1, i, j), block_ij);

    return CnC::CNC_Success;
}

// *******************************************************

template< class dependency_consumer >
void tuner_dist_2d::depends( const Triple & tag, context_dist_2d & c, dependency_consumer & dC ) const
{
    int k = tag[0], i = tag[1], j = tag[2];

    dC.depends( c.m_blocks, Triple(k, i, j));

    if ((k/c.block_size != j))
        dC.depends( c.m_col_transfer, Pair(k, i));

    if ((k/c.block_size != i))
        dC.depends( c.m_row_transfer, Pair(k, j));
}

// *******************************************************
// main driver
// *******************************************************

void floyd_dist_2d_cnc(double* path_distance_matrix, int matrix_size, int block_size)
{
    double t_start, t_end;
    context_dist_2d c(matrix_size, block_size);
    int num_blocks = matrix_size/ block_size;

    printf("Num Tasks (%d * %d * %d) = %d\n", matrix_size, num_blocks, num_blocks, matrix_size * num_blocks * num_blocks);

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

    for (int j = 0; j < num_blocks; j++) {
        std::shared_ptr<Tile2d<double> > row_tile = std::make_shared<Tile2d<double> >(1, c.block_size);
        for(int offset = j*block_size,t_j = 0; t_j < block_size; t_j++) {
            (*row_tile)(0, t_j) = path_distance_matrix[offset + t_j];
        }
        c.m_row_transfer.put(Pair(0, j), row_tile);
    }

    for (int i = 0; i < num_blocks; i++) {
        std::shared_ptr<Tile2d<double> > col_tile = std::make_shared<Tile2d<double> >(c.block_size, 1);
        for(int offset = i*block_size, t_i = 0; t_i < block_size; t_i++) {
            (*col_tile)(t_i, 0) = path_distance_matrix[(offset + t_i) * matrix_size];
        }
        c.m_col_transfer.put(Pair(0, i), col_tile);
    }

    t_end = rtclock();
    printf("CnC tile creation time %0.6lfs seconds\n", t_end - t_start);

    c.m_singleton.put(1);

    t_start = rtclock();            
    c.wait(); 
    t_end = rtclock();
    printf("CnC task execution wait time %0.6lfs seconds\n", t_end - t_start);
    
    // Gather data back
    printf("Block Collection Size %li\n", c.m_blocks.size());
    t_start = rtclock();
    for(int i = 0; i < num_blocks; i++) {
        for(int j = 0; j < num_blocks; j++) {
            std::shared_ptr<Tile2d<double> > tile;
            c.m_blocks.get(Triple(matrix_size, i , j), tile);
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
                bool *p_run_seq, bool *p_run_omp, bool *p_run_cnc_dist_2d, 
                bool *p_comp_results)
{
    int i;  
    *p_matrix_size = 0; *p_block_size = 0;  
    *p_run_omp = false; *p_run_seq = false; *p_run_cnc_dist_2d = false; 
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
            *p_run_cnc_dist_2d = true;
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
    CnC::dist_cnc_init< context_dist_2d > _dinit;
#endif
    int ret;
    int i, j, k;
    double t_start, t_end;
   
    int    matrix_size = 0;
    int    block_size = 0;
    bool   run_seq, run_omp, run_cnc_dist_2d, comp_results;
    double *path_distance_matrix = NULL;

    parse_args(argc, argv, &matrix_size, &block_size, &run_seq, &run_omp, 
               &run_cnc_dist_2d, &comp_results);

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

    if (run_cnc_dist_2d) {
        init_square_matrix(path_distance_matrix, matrix_size);
        t_start = rtclock();
        floyd_dist_2d_cnc(path_distance_matrix, matrix_size, block_size); 
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
