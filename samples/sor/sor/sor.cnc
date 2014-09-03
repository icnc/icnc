//********************************************************************************
// Copyright (c) 2007-2009 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

///////////////////////////////////////////////////////////////////////////////
// Sagnak Tasirlar, Summer '09
// sagnak@rice.edu

// This was my first attempt at CnC and SOR
//
// SOR is acquired from Java SciMark http://math.nist.gov/scimark2/index.html
// It is a stencil computation which iteratively walks over (i,j). For each 
// entry, it computes a weighted average of its neighbours and that entry.
// This action is done repeatedly for a user specified number of times. So
// the outer loop is of the kind for 1..N and the inner loop walks over i,j
// and therefore allows parallelism along the i+j=<constant> diagonal.
//
// For each iteration of the outermost loop, a CnC context is created. The
// iterationStep is prescribed for each iteration. That step creates the (i,j)
// relaxation space for that iteration. Since the first row, last row, first
// column and last column will not be relaxed (they lack a full set of
// neighbours), they will be handled by the copy space. For each iteration,
// the iteration step will be prescribing the copy spaces besides the
// relaxation step, too. 
//
// iterationStep is prescribed by an integer which is the iteration number. 
// In turn, it prescribes the copySteps and relaxationSteps by a pair of
// integers representing the row and column number to be operated on. The tag
// is represented by one integer, though. The first 16 bits for the row and the
// second 16 bits for the column for a 32bit integer representation.
//
// The matrix operated on is given to the context as the prevIterMatrix and
// the environment gets the currIterMatrix which is the walked over, relaxed
// version of the same matrix
//
// relaxationStep gets the entry that it is prescribed on, computes a weighted
// average of that entry and its neighbours and puts it under the same indices
// of the currIterMatrix.
//
// copierStep gets the entry that it is prescribed for, from the prevIterMatrix
// and puts it into the same index on currIterMatrix.
///////////////////////////////////////////////////////////////////////////////

// parameter for relaxation, it is a singleton collection
// tagged by an integer which only can be 0
[ double omega < int > ];

// number of rows in the matrix given
// same tag issue mentioned above [ omega ]
[ size_t M < int > ];

// number of columns in the matrix given
// same tag issue mentioned above [ omega ]
[ size_t N < int >];

// input to the current serial iteration
[ double prevIterMatrix < size_tPair > ];

// output from the current serial iteration
[ double currIterMatrix < size_tPair > ];

// used to prescribe the relaxation for the given element
< size_tPair matrixSpace >;

// used to prescribe the copy step for the boundaries
< size_tPair copySpace >;

// prescribe the iterationStep with iterationSpace
< size_t iterationSpace >;
<iterationSpace> :: (iterationStep);
// it gets the number of rows and columns
// prescribes relaxation for inner nodes with matrixSpace
// prescribes copier for outer shell nodes with copySpace
[M],[N] -> (iterationStep) -> <matrixSpace>, <copySpace>;

<copySpace> :: (copier);
// copier copies the entries from the previous iteration
// to the current iteration for the prescribed values
[prevIterMatrix] -> (copier) -> [currIterMatrix];

<matrixSpace> :: (relaxation);
// relaxation, calculates the weighted(omega) average for 
// the entries and its neighbors from the previous iteration
// and puts it to the current iteration for the prescribed values
[omega], [prevIterMatrix], [currIterMatrix] -> (relaxation) -> [currIterMatrix];

// environment provides the number of rows and columns, omega
// and the initial matrix
env -><iterationSpace>, [prevIterMatrix], [M], [N], [omega]; 

// environment reads the updated matrix
[currIterMatrix]-> env;

