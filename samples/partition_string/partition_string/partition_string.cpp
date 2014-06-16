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

#include <string>
using namespace std;
#include "partition_string.h"

int CreateSpan::execute(const int & t, partStr_context & c ) const
{
    // Get input string
    string in;
    c.input.get(t, in);

    if (! in.empty()) {
        // construct span
        char ch = in[0];
        int len = 0;
        unsigned int i = 0;
        unsigned int j = 0;
        while (i < in.length()) {
            if (in[i] == ch) {
                i++;
                len++;
            } else {
                c.span.put(j, in.substr(j, len));
                c.spanTags.put(j);
                ch = in[i];
                len = 0;
                j = i;
            }
            // Put the last span
        }
        c.span.put(j, in.substr(j, len));
        c.spanTags.put(j);
    }

    return CnC::CNC_Success;
}

int ProcessSpan::execute(const int & t, partStr_context & c ) const
{
    // Get the span
    string span;
    c.span.get(t, span);
    // Test for odd length
    if ((span.length())%2 != 0) {  // odd length
        c.results.put(t, span);
    }
    return CnC::CNC_Success;
}

int main(int argc, char* argv[])
{
    // Create an instance of the context class which defines the graph
    partStr_context c;

    // Use an arbitrary value (e.g. 0) as the single tag value for
    // <singletonTag> and [input].
    const int singleton = 0;
    const string in_string = "aaaffqqqmmmmmmm";
    c.input.put(singleton, in_string);
    c.singletonTag.put(singleton);

    // Wait for all steps to finish
    c.wait();

    // Execution of the graph has finished.
    // Print the result instances.
    cout << "Input string: " << in_string << endl << endl
        << "The subparts with odd length:" << endl;

    CnC::item_collection<int, string>::const_iterator cii;
    for (cii = c.results.begin(); cii != c.results.end(); cii++) {
        string result = *(cii->second);
        cout << result << endl;
    }
}
