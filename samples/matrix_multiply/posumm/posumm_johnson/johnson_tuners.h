/************************************************************************
* posumm_johnson/johnson_tuners.h: This file is part of the POSUMM project.
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
* @file: posumm_johnson/johnson_tuners.h
* @author: Martin Kong <kongm@cse.ohio-state.edu>
************************************************************************/

#ifndef JOHNSON_TUNERS_H
# define JOHNSON_TUNERS_H
#include "../common/cnc_common.h"

/* Kernel headers. Declared here for convenience */
void mm_kernel_l3_tiled (int n, float * A, float * B, float * C, int Si, int Sj, int Sk, int Ti, int Tj, int Tk);
void mm_reduction_l3_tiled (int n, float * A, float * C, int Si, int Sj, int Ti, int Tj);


#define BLOCKS(n,b) ((n) % (b) ? (n)/(b)+1 : (n)/(b))

// Context declaration. It's definition can be found in mm_johnson_cnc.cpp
struct context_johnson;


// Structure used for the mm_compute step (3D parallel)
struct compute_step_dist_3d
{
    int execute( const Triple &t , context_johnson & c ) const;
};

// Structure used for the mm_reduce step (2D parallel)
struct reduce_step_dist_2d
{
    int execute( const Triple &t , context_johnson & c ) const;
};

// Structure used to generate all tags offline
struct tag_gen_step_dist_2d
{
    int execute(const int &p, context_johnson &c) const;
};


// No comments =(
static inline int root (int n)
{
  return exp(log(n)/3.0);
}



// compute_on for reduction steps (2D grid). 
// We assume that a block distribution of ranks is used.
inline int _compute_on_2d(int i, int j, int k, int matrix_size, int block_size,
		int numproc, int shift_x, int shift_y)
{
  assert (numproc);
  assert (block_size);
  int nb = BLOCKS(matrix_size,block_size);
  int node = ((i * nb)  + j) % numproc;
  return node;
}

// compute_on for block-multiply steps (3D grid)
// We assume that a a block distribution of ranks is used.
inline int _compute_on_3d(int i, int j, int k, int matrix_size, int block_size,
		int numproc, int shift_x, int shift_y)
{
  assert (numproc);
  assert (block_size);
  int nb = BLOCKS(matrix_size,block_size);
  int node = (i * nb * nb + j * nb + k) % numproc; 
  return node;
}




/********************************************************************************************
 *
 * Step tuner for mm_reduce step
 * Assumes a 2D grid, and uses the compute_on method, which determines on which 
 * rank/process should each step (i,j,k) be executed.
 *
 ********************************************************************************************/
struct tuner_dist_2d : public CnC::step_tuner<>
{
  public:
  context_johnson & m_context;
  int matrix_size;
  int block_size;
  int nodes;
  int n_blocks;
  dist_type m_dist;


  tuner_dist_2d(context_johnson &c, dist_type dt, int matrix_size, int block_size) ;

  template< class dependency_consumer >
  void depends( const int & tag, context_johnson & c, dependency_consumer & dC ) const {}

  template< class dependency_consumer >
  void depends( const Triple & tag, context_johnson & c, dependency_consumer & dC ) const ;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes; 
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  // compute_on for start_graph
  int compute_on( const int & tag, context_johnson & c) const
  {
    return 0;//tag % numProcs();
  }

  // compute_on for the main graph
  int compute_on( const Triple & tag, context_johnson & c) const
  {
    int k = tag[2];
    if (k < BLOCKS (matrix_size,block_size))
      return _compute_on_2d(tag[0], tag[1], tag[2],
			  matrix_size, block_size, numProcs(), 0, 0);
    else
      return 0;
  }



#ifdef _DIST_
  void serialize( CnC::serializer & ser )
  {
    ser & matrix_size & block_size & m_dist;
  }
#endif
};


/********************************************************************************************
 *
 * Step tuner for mm_compute step
 * Assumes a 3D grid, and uses the compute_on method, which determines on which 
 * rank/process should each step (i,j,k) be executed.
 *
 ********************************************************************************************/
struct tuner_dist_3d : public CnC::step_tuner<>
{
  public:
  context_johnson & m_context;
  int matrix_size;
  int block_size;
  int nodes;
  int n_blocks;
  dist_type m_dist;

  tuner_dist_3d(context_johnson &c, dist_type dt, int matrix_size, int block_size) ;

  template< class dependency_consumer >
  void depends( const int & tag, context_johnson & c, dependency_consumer & dC ) const {}

  template< class dependency_consumer >
  void depends( const Triple & tag, context_johnson & c, dependency_consumer & dC ) const ;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes; 
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  // compute_on for start_graph
  int compute_on( const int & tag, context_johnson & c) const { return 0; }

  // compute_on for the main graph
  int compute_on( const Triple & tag, context_johnson & c) const
  {
    return _compute_on_3d(tag[0], tag[1], tag[2],
			matrix_size, block_size, numProcs(), 0, 0);
  }


#ifdef _DIST_
  void serialize( CnC::serializer & ser )
  {
    ser & matrix_size & block_size & m_dist;
  }
#endif
};


/********************************************************************************************
 *
 * Generic item tuner.
 * All item tuners inherit from this structure.
 *
 ********************************************************************************************/
struct generic_item_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
  public:

  context_johnson & m_context;
  int matrix_size;
  int block_size;
  int nodes;
  int n_blocks;
  dist_type m_dist;

 generic_item_tuner(context_johnson &c, dist_type dt, int matrix_size, int block_size) :
    m_context(c),
    m_dist(dt),
    matrix_size(matrix_size),
    block_size(block_size)
    { }

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes;
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }


#ifdef _DIST_
  void serialize( CnC::serializer & ser )
  {
    ser & matrix_size & block_size & m_dist;
  }
#endif
};




/********************************************************************************************
 *
 * Item tuner for A blocks:
 * All blocks are produced by the environment.
 * Each (mm_compute:i,j,k) consumes a [mat_A_blocks:i,k,j] assuming a 3D grid.
 *
 ********************************************************************************************/
struct tuner_dist_A : public generic_item_tuner 
{
  public: 
    int matrix_size;
    int block_size;
    int n_blocks;
    int nodes;

  tuner_dist_A (context_johnson &c, dist_type dt,int matrix_size, int block_size) ;

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    int i = tag[0];
    int j = tag[1];
    int k = tag[2];
  	return _compute_on_3d(i,k,j, matrix_size, block_size, numProcs(), 0, 0);
  }

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes;
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    int k = tag[2];
    return _compute_on_3d(0, 0, 0,
	  		  matrix_size, block_size, numProcs(), 0, 0);
  }

};

/********************************************************************************************
 *
 * Item tuner for B blocks:
 * All blocks are produced by the environment.
 * Each (mm_compute:i,j,k) consumes a [mat_B_blocks:k,j,i] assuming a 3D grid.
 *
 ********************************************************************************************/
struct tuner_dist_B : public generic_item_tuner 
{
  public: 
    int matrix_size;
    int block_size;
    int n_blocks;
    int nodes;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes;
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  tuner_dist_B(context_johnson &c, dist_type dt, int matrix_size, int block_size);

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    int i = tag[0];
    int j = tag[1];
    int k = tag[2];
  	return _compute_on_3d(k,j,i, matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    int k = tag[2];
    return _compute_on_3d(0, 0, 0,
	  		  matrix_size, block_size, numProcs(), 0, 0);
  }

};


/********************************************************************************************
 *
 * Item tuner for C blocks:
 * Each (mm_compute:i,j,k) produces a [mat_C_blocks:i,j,k] assuming a 3D grid.
 * Each (mm_reduce:i,j,k) consumes a [mat_C_blocks:i,j,k] assuming a 2D grid.
 *
 ********************************************************************************************/
struct tuner_dist_C : public generic_item_tuner 
{
  public:
    int matrix_size;
    int block_size;
    int n_blocks;
    int nodes;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes;
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  tuner_dist_C(context_johnson &c, dist_type dt, int matrix_size, int block_size) ;

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    return _compute_on_2d(tag[0], tag[1], tag[2],
        matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    return _compute_on_3d(tag[0], tag[1], tag[2], matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int compute_on (const Triple& tag) const
  {
    return _compute_on_3d(tag[0], tag[1], tag[2],
			matrix_size, block_size, numProcs(), 0, 0);
  }


  template< class dependency_consumer >
  void depends
    ( const Triple & tag, context_johnson & c, 
      dependency_consumer & dC ) const;

};

/********************************************************************************************
 *
 * Item tuner for reduction C blocks:
 * These blocks are computed on a 2D grid, hence they use the compute_on_2d function.
 * All [i,j,0] are produced on rank-0, while [i,j,k>0] are produced by step (i,j,k-1).
 * Every step (i,j,k) consumes item [i,j,k]. The environment then consumes all [i,j,num_blocks].
 *
 ********************************************************************************************/
struct tuner_dist_C_reduction : public generic_item_tuner 
{
  public:
    int matrix_size;
    int block_size;
    int n_blocks;
    int nodes;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes;
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }

  tuner_dist_C_reduction (context_johnson &c, dist_type dt, int matrix_size, int block_size) ;

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    int k = tag[2];
    // All [i,j,num_blocks] items are consumed on rank_0
    if (k == n_blocks)
      return 0; 
    // item [i,j,k<num_blocks] is consumed by step (i,j,k)
    return _compute_on_2d(tag[0], tag[1], tag[2],
			matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    int k = tag[2];
    // All [i,j,0] items are produced on rank_0, but 
    if (k == 0)  
      return 0; 
    // item [i,j,k>0] is produced by step (i,j,k-1)
    return _compute_on_2d(tag[0], tag[1], tag[2]-1,
			matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int compute_on (const Triple& tag) const 
  {
    return _compute_on_2d(tag[0], tag[1], tag[2],
			matrix_size, block_size, numProcs(), 0, 0);
  }

  template< class dependency_consumer >
  void depends
    ( const Triple & tag, context_johnson & c, 
      dependency_consumer & dC ) const;

};


#endif // !JOHNSON_TUNERS_H
