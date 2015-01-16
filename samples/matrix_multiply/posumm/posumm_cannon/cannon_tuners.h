/************************************************************************
* posumm_cannon/cannon_tuners.h: This file is part of the POSUMM project.
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
* @file: posumm_cannon/cannon_tuners.h
* @author: Martin Kong <kongm@cse.ohio-state.edu>
************************************************************************/
#ifndef CANNON_TUNERS_H
# define CANNON_TUNERS_H
#include "../common/cnc_common.h"

/* Kernel headers. Declared here for convenience */
void mm_kernel_l3_tiled (int n, float * A, float * B, float * C, int Si, int Sj, int Sk, int Ti, int Tj, int Tk);

// Shift macros for initial distribution of data blocks
#define SHIFTB0(i,j,k,s) Triple ((i-j+s)%(s),j,k)
#define SHIFTA0(i,j,k,s) Triple (i,(j-i+s)%(s),k)
// Shift macros for step data block movement (left for A, up for B)
#define SHIFTB1(i,j,k,s) Triple ((i-1+s)%(s),j,k)
#define SHIFTA1(i,j,k,s) Triple (i,(j-1+s)%(s),k)
#define SHIFTC(i,j,k,s) Triple (i,j,k)

struct s_global_data_t 
{
  int matrix_size;
  int block_size;
  int n_blocks;
  int n_nodes;

  #ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
      ser & matrix_size & block_size &  n_blocks & n_nodes;
    }
  #endif
} ; 

#define BLOCKS(n,b) ((n) % (b) ? (n)/(b)+1 : (n)/(b))

// Context declaration. It's definition can be found in mm_cannon_cnc.cpp
struct context_cannon_2d;

// Structure used for the mm_compute step (2D parallel)
struct compute_step_dist_2d
{
    int execute( const Triple &t , context_cannon_2d & c ) const;
};

// Structure used to generate all tags offline
struct tag_gen_step_dist_2d
{
    int execute(const int &p, context_cannon_2d &c) const;
};


// compute_on for block-multiply steps (2D grid)
// We assume that a a block distribution of ranks is used.
inline int _compute_on(int i, int j, int k, int matrix_size, int block_size,
		int numproc, int shift_x, int shift_y)
{
  assert (numproc);
  assert (block_size);
  int nb = BLOCKS(matrix_size,block_size);
  int node = ((i * nb)  + j) % numproc; 
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
  context_cannon_2d & m_context;
  int matrix_size;
  int block_size;
  int nodes;
  int n_blocks;
  dist_type m_dist;


  tuner_dist_2d(context_cannon_2d &c, dist_type dt, int matrix_size, int block_size) ;

  template< class dependency_consumer >
  void depends( const int & tag, context_cannon_2d & c, dependency_consumer & dC ) const {}

  template< class dependency_consumer >
  void depends( const Triple & tag, context_cannon_2d & c, dependency_consumer & dC ) const ;

  void setGlobals (int _nodes, int ms, int bs){
    nodes = _nodes; 
    matrix_size = ms;
    block_size = bs;
    n_blocks = BLOCKS (ms,bs);
  }


  // compute_on for start_graph
  int compute_on( const int & tag, context_cannon_2d & c) const
  {
    return 0;
  }

  // compute_on for the main graph
  int compute_on( const Triple & tag, context_cannon_2d & c) const
  {
    return _compute_on(tag[0], tag[1], tag[2],
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

  context_cannon_2d & m_context;
  int matrix_size;
  int block_size;
  int nodes;
  int n_blocks;
  dist_type m_dist;

 generic_item_tuner(context_cannon_2d &c, dist_type dt, int matrix_size, int block_size) :
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
 * Make our life easy. Since it is Cannon's implementation, it is MUCH simpler
 * to say where is an item consumed rather than to say where it is produced.
 * Hence, we only use the consumed_on with the compute_on function (see top of
 * file).
 *
 ********************************************************************************************/
struct tuner_dist_A : public generic_item_tuner 
{
  public: 
    int matrix_size;
    int block_size;
    int n_blocks;
    int nodes;

  tuner_dist_A (context_cannon_2d &c, dist_type dt,int matrix_size, int block_size) ;

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    int i = tag[0];
    int j = tag[1];
    int k = tag[2];
  	return _compute_on(i,j,k, matrix_size, block_size, numProcs(), 0, 0);
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
    int i = tag[0];
    int j = tag[1];
    int k = tag[2];
    return CnC::PRODUCER_UNKNOWN;
  }

};


/********************************************************************************************
 *
 * Item tuner for B blocks:
 * Make our life easy. Since it is Cannon's implementation, it is MUCH simpler
 * to say where is an item consumed rather than to say where it is produced.
 * Hence, we only use the consumed_on with the compute_on function (see top of
 * file).
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

  tuner_dist_B(context_cannon_2d &c, dist_type dt, int matrix_size, int block_size);

  template <class Triple>
  int consumed_on(const Triple& tag) const
  {
    int i = tag[0];
    int j = tag[1];
    int k = tag[2];
  	return _compute_on(i,j,k, matrix_size, block_size, numProcs(), 0, 0);
  }

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    return CnC::PRODUCER_UNKNOWN;
  }

};


/********************************************************************************************
 *
 * Item tuner for C blocks:
 *
 * If (k = 0) [i,j,k] is produced by rank-0 
 * else (mm_compute:i,j,k) produces a [i,j,k+1] assuming a 2D grid, 
 * wherever this mm_compute might be.
 *
 * If (k = num_blocks) then [i,j,num_blocks] is consumed by rank-0
 * else (mm_compute:i,j,k) consumes a [i,j,k] assuming a 2D grid,
 * wherever this mm_compute might be.
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

  tuner_dist_C(context_cannon_2d &c, dist_type dt, int matrix_size, int block_size) ;

  template <class Triple>
  int consumed_on(const Triple& tag) const;

  template <class Triple>
  int produced_on(const Triple& tag) const
  {
    int k = tag[2];
    if (k == 0)
      return 0;
    else
      return CnC::PRODUCER_UNKNOWN;
  }

  template <class Triple>
  int compute_on (const Triple& tag) const
  {
    int k = tag[2];
    int num_blocks = BLOCKS (matrix_size,block_size);
    if (k < num_blocks)
      return _compute_on(tag[0], tag[1], tag[2],
			  matrix_size, block_size, numProcs(), 0, 0);
    else
      return _compute_on(0, 0, 0,
			  matrix_size, block_size, numProcs(), 0, 0);
  }


  template< class dependency_consumer >
  void depends
    ( const Triple & tag, context_cannon_2d & c, 
      dependency_consumer & dC ) const;

};



#endif // !CANNON_TUNERS_H
