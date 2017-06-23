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

typedef int myType; // data type for [result]
#include "gen_new_iter_space.h"

int GenerateIterations::execute(const int & t, genIter_context& c ) const
{
    // Get upper bound
    int ubound;
    c.imax.get(t, ubound);

    // Generate iterations tags
    for (int i = 1; i <= ubound; i ++) {
        c.iterations.put(i);
    }
    return CnC::CNC_Success;
}

int ExecuteIterations::execute(const int & t, genIter_context & c ) const
{
    // step_tag is the interation index
    myType i = t;

    // Execute the iteration and put the result.
    // In this example simply use the tag value as result.
    c.result.put(t, i);
    return CnC::CNC_Success;
}

int main(int argc, char* argv[])
{
    // Create an instance of the context class which defines the graph
    genIter_context c;

    // User input for the number of iterations to generate.
    // It can be any positive integer.
    const int numIters = 100;

    // Use an arbitrary value (e.g. 0) as the single tag value for "single".
    // Also use this value as the single tag value for "imax".
    const int rootTag = 0;

    // Put the input item
    c.imax.put(rootTag, numIters);

    // Put the input tag
    c.single.put(rootTag);

    // Wait for all steps to finish
    c.wait();

    // Execution of the graph has finished.
    // Print the result instances.
    std::cout << "Should print out values from 1 to " << numIters <<
        " in random order:" << std::endl << std::endl;
    int count = 0;
    CnC::item_collection<int, myType>::const_iterator cii;
    for (cii = c.result.begin(); cii != c.result.end(); cii++) {
        myType resultVal = *(cii->second);
        // Print result values 10 in a row
        std::cout << resultVal << " ";
        count++;
        if (count == 10) {
            std::cout << std::endl;
            count = 0;
        }
    }
}
