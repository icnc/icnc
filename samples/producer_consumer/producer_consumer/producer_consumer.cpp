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

typedef int my_t; // data type for [inItem], [valueItem] and [outItem].
#include "producer_consumer.h"

// The "producer" step gets an inItem, and produces a valueItem.
int Producer::execute(const int & t, ProdCons_context & c ) const
{
    // Get the inItem
    my_t in;
    c.inItem.get(t, in);

    // Compute the valueItem
    my_t value = in * 10;

    // i is some function of my_tag_in
    int i = (int)t ;

    // Put the valueItem
    c.valueItem.put(i, value);

    return CnC::CNC_Success;
}

// The "consumer" step gets an valueItem, and produces an outItem.
int Consumer::execute(const int & t, ProdCons_context & c ) const
{
    // i is some function of my_tag_in
    int i = (int)t ;

    // Get the valueItem
    my_t value;
    c.valueItem.get(i, value);

    // Compute the outItem
    my_t out = value + 3;

    // Put the outItem
    c.outItem.put(t, out);

    return CnC::CNC_Success;
}

int main(
    int argc,
    char* argv[])
{
    // Create an instance of the context class which defines the graph
    ProdCons_context c;

    // User input for the number of instances of (producer) and (consumer)
    // to be executed.  It can be any positive integer.
    const unsigned int N = 10;

    // Put all inItem instances and all prodConsTag instances.
    // Each prodConsTag instance prescribes a "producer" step instance
    // and a "consumer" step instance.
    for (unsigned int i = 0; i < N; ++i){
        c.inItem.put(i, my_t(i*10));
        c.prodConsTag.put(i);
    }
    
    // Wait for all steps to finish
    c.wait();
 
    // Execution of the graph has finished.
    // Print the outItem instances.
    CnC::item_collection<int, my_t>::const_iterator cii;
    for (cii = c.outItem.begin(); cii != c.outItem.end(); cii++) {
        int tag = cii->first;
        my_t outVal = *(cii->second);
        std::cout << "outItem" << tag << " is " << outVal << std::endl;
    }
}
