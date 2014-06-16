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

typedef struct {
    int value;
    int sum;
} myType;

#include "time.h"
#define MAXSUM 200
#include <utility>
typedef std::pair<int,int> pair;

#include <cnc/cnc.h>

#include "red_rows_until_pred.h"
#include <cstdio>

using namespace CnC;

int Process::execute(const pair & t, redRowsP_context & c ) const
{
    // Get value and running sum
    myType data;
    c.dataValues.get(t, data);
    int value = data.value;
    int sum = data.sum;
    int new_sum = sum + value;
    int inputID = t.first;
    int current = t.second;

    // Predicate test
    if (new_sum > MAXSUM) {
        // Processing for inputId is complete. put the result
        c.results.put(inputID, data.value);
        printf("in process: inputId=%d current=%d value=%d sum=%d new_sum=%d\n",
            inputID, current, value, sum, new_sum);

    } else {
        // Generate new value between 0..100
        srand((unsigned)value);
        rand();
        int RANGE_MAX = 100;
        int new_val = (int)(((double)rand()/(double)RAND_MAX) * RANGE_MAX);
        printf("in process: inputId=%d current=%d value=%d sum=%d new_sum=%d new_val=%d\n",
            inputID, current, value, sum, new_sum, new_val);

        myType new_data = {new_val, new_sum};
        pair new_tag = pair(inputID, current+1);

        c.dataValues.put(new_tag, new_data);
        c.controlTags.put(new_tag);
    }

    return CNC_Success;
}

#define INPUTID_COUNT 5

int main(int argc, char* argv[])
{
    // Create an instance of the context class which defines the graph
    redRowsP_context c;
    int inputID;

    // Put input items and input tags
    for (inputID = 1; inputID <= INPUTID_COUNT; inputID++) {
        int val = inputID;
        int sum = 0;
        myType data = {val, sum};
        int current = 1;
        pair tag(inputID, current);
        c.dataValues.put(tag, data);
        c.controlTags.put(tag);
    }

    // Wait for all steps to finish
    c.wait();

    // Execution of the graph has finished.
    // Print the result instances.
    printf("\nResults:\n");
    for (inputID = 1; inputID <= INPUTID_COUNT; inputID++) {
        int result;
        c.results.get(inputID, result);
        printf("inputID = %d, result = %d\n",
            inputID, result);
    }
}
