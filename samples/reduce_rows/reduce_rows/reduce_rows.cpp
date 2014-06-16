//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
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
//
#include <utility>
typedef std::pair<int,int> pair;

#include <cnc/cnc.h>

#include "reduce_rows.h"

int ProcessFixedItems::execute(const pair & t, redRows_context & c ) const
{
    // Get previous sum and current item
    int row = t.first;
    int col = t.second;
    int prev_sum;
    c.sum.get(pair(row, col-1), prev_sum);
    int val;
    c.readOnlyItem.get(t, val);

    // Put new sum
    c.sum.put(t, prev_sum + val);

    return CnC::CNC_Success;
}

#define NROW 3
#define NCOL 5

int main(int argc, char* argv[])
{
    // Create an instance of the context class which defines the graph
    redRows_context c;
    int i;
    int j;

    // Put initial instances of sum
    for (i = 1; i <= NROW; i++) {
        c.sum.put(pair(i,0), 0);
    }

    // Put all instances of readOnlyItem and all instances of fixedIterations.
    // row 1: 11 12 13 14 15  -> final sum 65
    // row 2: 21 22 23 24 25  -> final sum 115
    // row 3: 31 32 33 34 35  -> final sum 165
    for (i = 1; i <= NROW; i++) {
        for (j = 1; j <= NCOL; j++) {
            c.readOnlyItem.put(pair(i,j), i*10+j);
            c.fixedIterations.put(pair(i,j));
        }
    }

    // Wait for all steps to finish
    c.wait();

    // Execution of the graph has finished.
    // Print the final sums.
    for (i = 1; i <= NROW; i++) {
        int sum;
        c.sum.get(pair(i,NCOL), sum);
        std::cout << "Row " << i << "  sum = " <<
            sum << std::endl;
    }
}
