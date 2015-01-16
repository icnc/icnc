/************************************************************************
* posumm_johnson/mm_johnson_cnc.cpp: This file is part of the POSUMM project.
*
* POSUMM: Portable OSU Matrix-Multiply, CnC implementations.
*
* Copyright (C) 2014,2015 Ohio State University, Columbus, Ohio
*
* This program can be redistributed and/or modified under the terms
* of the license specified in the LICENSE.txt file at the root of the
* project.
*
* Contact: Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>
*
*
* @file: posumm_johnson/mm_johnson_cnc.cpp
* @author: Martin Kong <kongm@cse.ohio-state.edu>
************************************************************************/

#include "../common/cnc_common.h"
#include "johnson_tuners.h"
#include <cnc/debug.h>
#include <math.h>

#ifndef ceild
#define ceild(x,y) (((x) > 0)? (1 + ((x) - 1)/(y)): ((x) / (y)))
#endif

#ifndef floord
#define floord(x,y) (((x) > 0)? ((x)/(y)): 1 + (((x) -1)/ (y)))
#endif

#ifndef max
#define max(x,y)    ((x) > (y)? (x) : (y))
#endif

#ifndef min
#define min(x,y)    ((x) < (y)? (x) : (y))
#endif

// Whole program timing structure
typedef struct {
  double init;
  double exec;
  double comm;
  double comp;
} cnc_time;

// Per step timing structure
typedef struct {
  double comm;
  double comp;

#if defined(_DIST_)
  void serialize( CnC::serializer &  ser )
  {
    ser & comm & comp;
  }
#endif
} cnc_core_time;



/***********************************************************************************
*
*  CnC Context:
*  The program can be thought of as being 3-level tiled (main memory, L3 and L1). 
*  The work decomposition and task mapping parameters are: matrix_size, block_size,
*  nodes (machines) and nb_procs (number of processes/ranks).  Uses 4 item
*  collections (mat_A_blocks,mat_B_blocks,mat_C_blocks and mat_C_final_blocks)
*  and 2 steps collections (mm_compute and mm_reduce).  
*  The graph for this program is the following:
*  env -> [mat_A_blocks:i,j,k], [mat_B_blocks:i,j,k],[mat_C_final_blocks:i,j,0];
*  [mat_C_final_blocks:i,j,num_blocks] -> env;
*  [mat_A_blocks:i,k,j],[mat_B_blocks:k,j,i] -> (mm_compute:i,j,k) -> 
*     [mat_C_blocks:i,j,k]
*  [mat_C_blocks:i,j,k],[mat_C_final_blocks:i,j,k] -> (mm_reduce:i,j,k) -> 
*     [mat_C_final_blocks:i,j,k+1]
*
*  The mm_compute step can execute in parallel all steps, whereas steps
*  mm_reduce can execute in parallel along the i and j dimensions.
*
*  Each item and the reduction step collection are associated to tuners. 
*  The later uses the dependency_consumer tuner to follow the reduction pattern.
*  Item collections mat_A_blocks and mat_B_blocks use item tuners (the produced_on
*  method) to push items to the required ranks (from rank-0). Collections
*  mat_C_blocks and mat_C_final_blocks use both produce_on and consumed_on methods.
*************************************************************************************/
struct context_johnson : public CnC::context< context_johnson >
{
  int matrix_size;
  int block_size;
  int nb_procs;
  int procs_dim;
  int nodes;
  int n_blocks;
  std::shared_ptr<float>  mat_C;

  // Tuners
  tuner_dist_3d main_tuner;
  tuner_dist_A item_tuner_A; // 3D tuner
  tuner_dist_B item_tuner_B; // 3D tuner
  tuner_dist_C item_tuner_C; // 3D tuner
  tuner_dist_C_reduction item_tuner_C_reduction; // 2D tuner
  tuner_dist_2d reduce_step_tuner; // 2D tuner  
  
  // Tag Collections
  CnC::tag_collection<int> start_graph;
  CnC::tag_collection<Triple> mm_tags;
  CnC::tag_collection<Triple> reduce_tags;
  // Step Collections
  CnC::step_collection< tag_gen_step_dist_2d, tuner_dist_3d> tag_generator;
  CnC::step_collection< compute_step_dist_3d, tuner_dist_3d> mm_compute;
  CnC::step_collection< reduce_step_dist_2d, tuner_dist_2d> mm_reduce;

  // Item Collections
  #ifdef TIMESTEP
  CnC::item_collection<Triple, cnc_core_time> t_core1;
  CnC::item_collection<Triple, cnc_core_time> t_core2;
  #endif
  CnC::item_collection<Triple, std::shared_ptr<Tile2d<float> >, tuner_dist_A> mat_A_blocks;
  CnC::item_collection<Triple, std::shared_ptr<Tile2d<float> >, tuner_dist_B> mat_B_blocks;
  CnC::item_collection<Triple, std::shared_ptr<Tile2d<float> >, tuner_dist_C> mat_C_blocks;
  CnC::item_collection<Triple, std::shared_ptr<Tile2d<float> >, tuner_dist_C_reduction> mat_C_final_block;

  context_johnson
  (int _nodes = 1, int _matrix_size = 0, int _block_size = 0, int _nb_procs = 0, float * p_mat_C = NULL)
    : CnC::context< context_johnson >(),
      // Initialize tuners 
      main_tuner(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),
      reduce_step_tuner(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),
      item_tuner_A(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),
      item_tuner_B(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),
      item_tuner_C(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),
      item_tuner_C_reduction(*this, CANNON_BLOCK_2D, _matrix_size, _block_size),

      // Initialize step collection
      tag_generator( *this, main_tuner),
      mm_compute( *this , main_tuner),
      mm_reduce( *this,reduce_step_tuner),
      // Initialize each item collection
      mat_A_blocks( *this, "A", item_tuner_A),
      mat_B_blocks( *this, "B", item_tuner_B),
      mat_C_blocks( *this, "C", item_tuner_C),
      mat_C_final_block ( *this, "C_red", item_tuner_C_reduction),
      #ifdef TIMESTEP
      t_core1 ( *this, "T1"),
      t_core2 ( *this, "T2"),
      #endif
      // Initialize tags
      mm_tags(*this),
      reduce_tags(*this),
      start_graph(*this),
      // Initialize other attributes
      nodes(_nodes),
      matrix_size(_matrix_size),
      block_size(_block_size)
  {
    start_graph.prescribes(tag_generator, *this);
    mm_tags.prescribes(mm_compute, *this);
    reduce_tags.prescribes(mm_reduce, *this);
    
    tag_generator.controls(mm_tags);
    tag_generator.controls(reduce_tags);

    #ifdef TIMESTEP
    mm_compute.produces(t_core1);
    mm_reduce.produces(t_core2);
    #endif

    mm_compute.consumes(mat_A_blocks);
    mm_compute.consumes(mat_B_blocks);
    mm_compute.produces(mat_C_blocks);
    mm_reduce.consumes(mat_C_blocks);
    mm_reduce.produces(mat_C_final_block);
    
    procs_dim = root (nb_procs);

    // For some reason, when using i_mpi + cnc on slurm program arguments
    // are not passed accordingly to each rank. To vent this impass we
    // use a text file in the format: <nbr.nodes> <matrix_size> <block_size> <nbr.ranks>
    FILE * f = fopen ("conf.txt","r");
    fscanf (f,"%d",&nodes);
    fscanf (f,"%d",&matrix_size);
    fscanf (f,"%d",&block_size);
    fscanf (f,"%d",&nb_procs);
    n_blocks = BLOCKS (matrix_size,block_size);
    fclose (f);

    mat_C = std::shared_ptr<float> (p_mat_C);

    // Propagate globals to tuners on all ranks
    main_tuner.setGlobals (nodes, matrix_size, block_size);
    item_tuner_A.setGlobals (nodes, matrix_size, block_size);
    item_tuner_B.setGlobals (nodes, matrix_size, block_size);
    item_tuner_C.setGlobals (nodes, matrix_size, block_size);
    item_tuner_C_reduction.setGlobals (nodes, matrix_size, block_size);

  }
  ~context_johnson()
  {
  }

#if defined(_DIST_)
  void serialize( CnC::serializer &  ser )
  {
    ser & matrix_size & block_size & nb_procs ;
  }
#endif
};


/*****************************************************************************
*
*  Tuners (rest of the implementation is in johnson_tuners.h)
*
*****************************************************************************/

tuner_dist_2d::tuner_dist_2d(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
  m_context(c), m_dist(dt), matrix_size(matrix_size), block_size(block_size)
{
}

tuner_dist_3d::tuner_dist_3d(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
  m_context(c), m_dist(dt), matrix_size(matrix_size), block_size(block_size)
{
}

tuner_dist_A::tuner_dist_A (context_johnson &c, dist_type dt,int _matrix_size, int _block_size) :
  generic_item_tuner(c, dt, _matrix_size, _block_size), matrix_size (_matrix_size),block_size(_block_size)
{
}

tuner_dist_B::tuner_dist_B(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
    generic_item_tuner(c, dt, matrix_size, block_size)
{
}

tuner_dist_C::tuner_dist_C(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
  generic_item_tuner(c, dt, matrix_size, block_size)
{
}

tuner_dist_C_reduction::tuner_dist_C_reduction(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
  generic_item_tuner(c, dt, matrix_size, block_size)
{
}


template< class dependency_consumer >
void tuner_dist_2d::depends
   ( const Triple & tag, context_johnson & c, 
     dependency_consumer & dC ) const
{
  int i = tag[0], j = tag[1], k = tag[2];
  dC.depends( c.mat_C_blocks, Triple(i,j,k));
  if (k > 0)
    dC.depends( c.mat_C_final_block, Triple(i,j,k-1));
}

template< class dependency_consumer >
void tuner_dist_3d::depends
   ( const Triple & tag, context_johnson & c, 
     dependency_consumer & dC ) const
{
}


template< class dependency_consumer >
void tuner_dist_C_reduction::depends
   ( const Triple & tag, context_johnson & c, 
     dependency_consumer & dC ) const
{
  int i = tag[0], j = tag[1], k = tag[2];
  dC.depends( c.mat_C_blocks, Triple(i,j,k));
  if (k > 0)
    dC.depends( c.mat_C_final_block, Triple(i,j,k-1));
}





/*****************************************************************************
*
*
* Tag generator step. Simply generate all (i,j,k) values.
* 
* 
*****************************************************************************/

int tag_gen_step_dist_2d::execute(const int & p, context_johnson & c ) const
{
  int nb_tiles = BLOCKS(c.matrix_size,c.block_size);

  int tasks = 0;
  for (int i = 0; i < nb_tiles; ++i)
    for (int j = 0; j < nb_tiles; ++j)
  {
    for (int k = 0; k < nb_tiles; ++k)
    {
      c.mm_tags.put(Triple(i, j, k));
      c.reduce_tags.put(Triple(i,j,k)); 
      tasks++;
    }
  }
  return CnC::CNC_Success;
}



/*****************************************************************************
*                                                                            
*  Block-multiply step: Each step <i,j,k> fetches a block A_{ik} and a
*  block B_{kj} to produce a block C_{ij}.
*  The block-reduction is performed by a following step which uses a 
*  dependency tuner.
*                                                                           
*****************************************************************************/
int compute_step_dist_3d::execute(const Triple &t, context_johnson & c ) const
{
  int s = c.block_size;
  int i = t[0];
  int j = t[1];
  int k = t[2];

  #ifdef TIMESTEP
  double t_start;
  double t_stop;
  cnc_core_time ct;
  #endif

  std::shared_ptr<Tile2d<float> > block_ik_A  = std::make_shared<Tile2d<float> >(s,s);; 
  std::shared_ptr<Tile2d<float> > block_kj_B  = std::make_shared<Tile2d<float> >(s,s);; 
  std::shared_ptr<Tile2d<float> > block_ij_C_out = std::make_shared<Tile2d<float> >(s,s);

  // Fetch the blocks according to typical matmul: Cij = Aik * Bkj 
  // unused index in matrix A and B is used as the replication ID.
  c.mat_A_blocks.get(Triple(i,k,j), block_ik_A);
  c.mat_B_blocks.get(Triple(k,j,i), block_kj_B);
   
  // Obtain the raw pointers to the matrix block to perform the kernel
  // computation (which is tiled for L3 and L1)
  float * A = (*block_ik_A).addr();
  float * B = (*block_kj_B).addr();
  float * C = (*block_ij_C_out).addr();
  
  // Kernel is 2-level tiled, for L3 and L1.
  // The given tile sizes 240x256x128 and 80x128x16 are for an L3 and L1 cache
  // of 12MB and 32KB, respectively, assuming single precision.
  // L3 tile sizes are chosen such that a 12MB is equally shared among 4 cores.
  // If double precision were to be used with the same number of cores, 
  // halve any L3 tile size dimension, preferrably 240 or 128.
  //
  #ifdef TIMESTEP
  // Invoke and time the step's kernel
  t_start = rtclock();
  mm_kernel_l3_tiled (s, A, B, C, 240, 256, 128, 80, 128, 16);
  t_stop = rtclock();
  ct.comp = t_stop - t_start;
  #else
  // Invoke the step's kernel
  mm_kernel_l3_tiled (s, A, B, C, 240, 256, 128, 80, 128, 16);
  #endif
            
  // Store the produced block using the same tag as the step
  c.mat_C_blocks.put(t, block_ij_C_out);

  // For stats purpose, store the compute time in t_core1 collection
  #ifdef TIMESTEP
  c.t_core1.put(t,ct);
  #endif

  return CnC::CNC_Success;
}

/*****************************************************************************
*                                                                            
*  Reduction step: collect all C_{ijk} and reduce along dimension K to 
*  produce blocks C_{ij}.                                      
*  A dependency tuner is used to guarantee the proper reduction, and
*  all C_{ij*} are mapped to the same rank.
*                                                                           
*****************************************************************************/
int reduce_step_dist_2d::execute(const Triple &t, context_johnson & c ) const
{
  int s = c.block_size;
  int i = t[0];
  int j = t[1];
  int k = t[2];
  #ifdef TIMESTEP
  double t_start;
  double t_stop;
  cnc_core_time ct;
  #endif

  std::shared_ptr<Tile2d<float> > block_ijk_C_in;
  std::shared_ptr<Tile2d<float> > block_ij_C_out;

  // Fetch a Cijk and the previously reduced Cij  block
  c.mat_C_blocks.get(Triple(i,j,k), block_ijk_C_in);
  c.mat_C_final_block.get(Triple(i,j,k), block_ij_C_out);
   

  float * Cin = (*block_ijk_C_in).addr();
  float * Cout = (*block_ij_C_out).addr();
  #ifdef TIMESTEP
  t_start = rtclock();
  mm_reduction_l3_tiled (s, Cin, Cout, 240, 256, 80, 128);
  t_stop = rtclock();
  ct.comp = t_stop - t_start;
  #else
  mm_reduction_l3_tiled (s, Cin, Cout, 240, 256, 80, 128);
  #endif
    
  // Store the updated block using a new tag, the last one is the
  // reduced/final block on C.
  int next_k = k + 1;
  c.mat_C_final_block.put(Triple(i,j,next_k), block_ij_C_out);

  // For stats purpose, store the compute time in t_core1 collection
  //
  #ifdef TIMESTEP
  c.t_core2.put(t,ct);
  #endif

  return CnC::CNC_Success;
}



/*****************************************************************************
*                                                                            *
*                                  Main MM routine                           *
*                                                                            *
*****************************************************************************/

int mm_johnson_cnc
  ( context_johnson & c, 
    int nodes, int matrix_size, int block_size, int num_proc, 
    float* mat_A, float* mat_B, float* mat_C, cnc_time * cnct )
{
  double t_start, t_end;
  int num_blocks = BLOCKS (matrix_size,block_size);
  
  // Enable this macro to see more CNC stats
  //#define CNC_DEBUG
  #ifdef CNC_DEBUG
  CnC::debug::collect_scheduler_statistics (c);
  CnC::debug::trace (c.mat_A_blocks,3);
  CnC::debug::trace (c.mat_B_blocks,3);
  CnC::debug::trace (c.mat_C_blocks,3);
  CnC::debug::trace (c.mat_C_final_block,5);
  CnC::debug::trace (c.mm_reduce,5);
  CnC::debug::trace (c.mm_compute,5);
  CnC::debug::trace (c.tag_generator,3);
  #endif


  printf ("Starting block distribution...(%d)\n",num_blocks);
  t_start = rtclock();

  // push blocks of A and B into item collections.
  for(int i = 0; i < num_blocks; i++) {
    for(int j = 0; j < num_blocks; j++) {
      for (int k = 0; k < num_blocks; k++) {
        std::shared_ptr<Tile2d<float> > tile = 
        std::make_shared<Tile2d<float> >(block_size, block_size);
        for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) {
          for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) {
            (*tile)( t_i, t_j ) = mat_A[org_i*matrix_size + org_j];
          }
        }
        c.mat_A_blocks.put(Triple(i,j,k), tile);
      }
    }
  }
  for(int i = 0; i < num_blocks; i++) {
    for(int j = 0; j < num_blocks; j++) {
      for (int k = 0; k < num_blocks; k++) {
        std::shared_ptr<Tile2d<float> > tile = std::make_shared<Tile2d<float> >(block_size, block_size);
        for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) {
          for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) {
            (*tile)( t_i, t_j ) = mat_B[org_i*matrix_size + org_j];
          }
        }
        c.mat_B_blocks.put(Triple(i,j,k), tile);
      }
    }
  }

  // Push in the initial C blocks used during the reduction steps
  for(int i = 0; i < num_blocks; i++) {
    for(int j = 0; j < num_blocks; j++) {
      std::shared_ptr<Tile2d<float> > tile = std::make_shared<Tile2d<float> >(block_size, block_size);
      for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) {
        for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) {
          (*tile)( t_i, t_j ) = 0.0;
        }
      }
      c.mat_C_final_block.put(Triple(i,j,0), tile);
    }
  }


  t_end = rtclock();
  cnct->init = t_end - t_start;
  printf("CnC init time: %0.6lf seconds\n", t_end - t_start);

  // Start the graph.
  t_start = rtclock();
  c.start_graph.put(0);
  c.wait();
  t_end = rtclock();

  for(int i = 0; i < num_blocks; i++) 
    for(int j = 0; j < num_blocks; j++) 
  {
    // Copy the matrix blocks to rank 0
    std::shared_ptr<Tile2d<float> > tile;
    Triple tag = Triple(i,j,num_blocks);
    int block_size = c.block_size;
    c.mat_C_final_block.get(tag, tile);
    #ifdef CNC_VERBOSE
      printf ("Gathering tile <%d,%d,%d> on P_%d from item C(%d,%d)\n",
        i,j,k,CnC::tuner_base::myPid(),i*block_size,j*block_size);
    #endif
    for(int org_i = i*block_size,t_i = 0; t_i < block_size; org_i++,t_i++) 
    {
      for(int org_j = j*block_size,t_j = 0; t_j < block_size; org_j++,t_j++) 
      {
        c.mat_C.get()[org_i*matrix_size + org_j] = (*tile)( t_i, t_j );
      }
    }
  }

  cnct->exec = t_end - t_start;
  printf("CnC execution time: %0.6lf seconds\n", t_end - t_start);

  // Reassemble matrix C
  printf ("Explicit copy out of matrix C (not accounted for time)\n");
  for (int i = 0; i < matrix_size; i++)
  {
    for (int j = 0; j < matrix_size; j++)
    {
      mat_C[i * matrix_size + j] = c.mat_C.get()[i * matrix_size + j];
    }
  }

  // Retrieve the compute time. This metric can be averaged afterwards,
  // for instance, by the number of ranks.
  // Dominant time is that of collection t_core1. 
 
  #ifdef TIMESTEP
  double t1 = 0;
  double t2 = 0;
  for(int i = 0; i < num_blocks; i++) 
    for(int j = 0; j < num_blocks; j++) 
      for(int k = 0; k < num_blocks; k++) 
  {
        cnc_core_time ct1;
        cnc_core_time ct2;
        Triple tag = Triple (i,j,k);
        c.t_core1.get (tag,ct1);
        t1 += ct1.comp;
        c.t_core2.get (tag,ct2);
        t2 += ct2.comp;
  }
  cnct->comp = t1+t2;

  printf ("Cumulative kernel computation time: %0.6lf seconds\n",t1);
  printf ("Cumulative reduction computation time: %0.6lf seconds\n",t2);
  #endif

  return 0;
}


/******************************************************************************
*                                                                             *
*         Parsing arguments                                                   *
*                                                                             *
*                                                                             *
******************************************************************************/
// @samples/floyd-warshall
void parse_args(int argc, char **argv, int * p_machines, int *p_matrix_size, int * p_num_procs,
                int *p_block_size,
                bool *p_run_seq, bool *p_run_omp, bool *p_run_cnc_dist_3d,
                bool *p_comp_results)
{
    int i;
    *p_matrix_size = 0; 
    *p_block_size = 0; 
    *p_num_procs = 1;
    *p_machines = 1;
    *p_run_omp = false; 
    *p_run_seq = false; 
    *p_run_cnc_dist_3d = false;
    *p_comp_results = false;


    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0) {
            char *argptr;
            *p_machines = strtol(argv[++i], &argptr, 10);
            if (!(*p_machines))
            {
              *p_machines = 1;
            }
        }

        if (strcmp(argv[i], "-n") == 0) {
            char *argptr;
            *p_matrix_size = strtol(argv[++i], &argptr, 10);
        }

        if (strcmp(argv[i], "-p") == 0) {
            char *argptr;
            *p_num_procs = strtol(argv[++i], &argptr, 10);
        }

        if (strcmp(argv[i], "-b") == 0) {
            // used for tiling within a step
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
            printf("usage : %s -n matrix_size -b block_size -p num_procs [-cnc][-check][-seq][-omp]\n", argv[0]);
    }

    if( *p_matrix_size == 0) {
        printf("%s require argument: -n matrix_size\n",argv[0]);
        exit( 11 );
    }
    if( *p_run_cnc_dist_3d && *p_block_size == 0) {
        printf("usage : %s -n matrix_size -p num_procs [-cnc][-check]\n", argv[0]);
        exit( 11 );
    }
    if ( *p_run_cnc_dist_3d && *p_matrix_size % *p_block_size )
    {
      printf ("Mode -cnc requires a block size that divides the matrix size\n");
      exit (11);
    }
}


/*****************************************************************************

  Other MM implementations

*****************************************************************************/

void mm_ref (int n, float * A, float * B, float * C)
{
  int i,j,k;
  for (i = 0; i<n; ++i)
    for (j = 0; j<n; ++j)
      for (k = 0; k<n; ++k)
        C[i*n+j] += A[i*n+k] * B[k*n+j];
}

void  mm_omp (int n, float * A, float * B, float * C)
{
  int i,j,k;
  #pragma omp parallel for
  for (int i = 0; i<n; ++i)
    for (int k = 0; k<n; ++k)
      #pragma simd
      #pragma vector
      #pragma ivdep
      for (int j = 0; j<n; ++j)
        C[i*n+j] += A[i*n+k] * B[k*n+j];
}


/*****************************************************************************

  Convenience functions

*****************************************************************************/

int check (int n, float * refC, float * cncC, double * p_diff)
{
  int i,j;
  double diff = 0;
  for (i = 0; i<n; i++)
    for (j = 0; j<n; j++)
  {
    double c1 = refC[i*n + j];
    double c2 = cncC[i*n + j];
    diff += ((c1 > c2) ? (c1-c2) : (c2-c1));
  }
  *p_diff = diff;
  if (diff > 0.00001)
    return 0;
  return 1;
}

static
void init_square_matrix_val(float* matrix, int matrix_size, int val)
{
    int i, j;
    for(i= 0; i < matrix_size; i++) 
      for(j= 0; j < matrix_size; j++) 
            matrix[i*matrix_size + j] = 0;
    for(i= 0; i < matrix_size; i++) 
        matrix[i*matrix_size + i] = val;
}

/*****************************************************************************
*
* Create a configuration file in the format:
*   nodes matrix_size block_size tasks
* The main program will create this file and it will then be read by
* each task. This is to circumvent some Slurm + Intel MPI specific
* problems when passing command line arguments.
*
******************************************************************************/
static
void create_conf_file (int nodes, int tasks, int matrix_size, int block_size)
{
  FILE * file;
  file = fopen ("conf.txt","w");
  fprintf (file,"%d %d %d %d",nodes,matrix_size,block_size,tasks);
  fclose (file);
}


/*****************************************************************************

  Main function

*****************************************************************************/

int main(int argc, char **argv)
{
#ifdef _DIST_
  CnC::dist_cnc_init< context_johnson > _dinit;
#endif
  double t_start, t_end;

  int    matrix_size = 0;
  int    block_size = 0;
  int    num_proc = 0;
  int    machines = 1;
  bool   run_seq, run_omp, run_cnc_dist_3d, comp_results;
  float *mat_A = NULL;
  float *mat_B = NULL;
  float *mat_C = NULL;
  float *mat_D = NULL;
  cnc_time cnct;

  parse_args(argc, argv, &machines, &matrix_size, &num_proc, &block_size, &run_seq, &run_omp, &run_cnc_dist_3d, &comp_results);

  create_conf_file (machines,num_proc,matrix_size,block_size);

  printf ("Machines = %d\nBlock size = %d\nMatrix size = %d\nNum. processes = %d\n",
    machines,block_size,matrix_size,num_proc);

  printf ("=============================================================================\n");


  if (run_cnc_dist_3d) 
  {
    mat_A = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_B = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_C = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));

    init_square_matrix_val(mat_A, matrix_size, 1.0);
    init_square_matrix_val(mat_B, matrix_size, 42.0);
    init_square_matrix_val(mat_C, matrix_size, 0);

    printf ("Using CnC implementation\n");

    // Must create context here in order to correctly retrieve mat_C.
    // Otherwise, the shared_ptr in which mat_C is stored in the context object
    // will be destroyed when mm_johnson_cnc finishes, leading to a lost value a C[0][0] ...
    context_johnson c(machines, matrix_size, block_size, num_proc, mat_C);
    // It's actually useless to pass parameters in the previous call.
    // By updating the global fields here changes should be replicated up to
    // before the first call to a PUT method.

    int res = mm_johnson_cnc(c, machines, matrix_size, block_size, num_proc, mat_A, mat_B, mat_C, &cnct);
    printf("CnC total time: %0.6lf seconds\n", cnct.init + cnct.exec);
    if (comp_results)
    {
      mat_D = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
      init_square_matrix_val(mat_D, matrix_size, 42.0);
      double diffs = 0;
      int res = check (matrix_size,mat_D,mat_C,&diffs);
      if (!res)
        printf ("Failed correctness check. Accumulated error exceeds 0.0001 (%lf)\n",diffs);
      else
        printf ("Passed correctness check\n");
      free (mat_D);
    }
    printf ("=============================================================================\n");
  }

  if (run_omp)
  {
    mat_A = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_B = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_C = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));

    init_square_matrix_val(mat_A, matrix_size, 1.0);
    init_square_matrix_val(mat_B, matrix_size, 42.0);
    init_square_matrix_val(mat_C, matrix_size, 0);

    printf ("Using OMP + SIMD implementation\n");
    t_start = rtclock();
    mm_omp (matrix_size,mat_A,mat_B,mat_C);
    t_end = rtclock();
    printf("Time %0.6lf seconds\n", t_end - t_start);
    printf ("=============================================================================\n");

    free (mat_A);
    free (mat_B);
    free (mat_C);
  }

  if (run_seq)
  {
    mat_A = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_B = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));
    mat_C = (float*) malloc (sizeof(float) * (matrix_size) * (matrix_size));

    init_square_matrix_val(mat_A, matrix_size, 1.0);
    init_square_matrix_val(mat_B, matrix_size, 42.0);
    init_square_matrix_val(mat_C, matrix_size, 0);

    printf ("Using sequential implementation (includes -parallel flag if compiled with icpc)\n");
    t_start = rtclock();
    mm_ref (matrix_size,mat_A,mat_B,mat_C);
    t_end = rtclock();
    printf("Time %0.6lf seconds\n", t_end - t_start);
    printf ("=============================================================================\n");

    free (mat_A);
    free (mat_B);
    free (mat_C);
  }

  return 0;
}
