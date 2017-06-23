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

#include "divide_conquer.h"

typedef int my_t;
int ROOT_TAG = 1;

// arbitrary encoding of node numbers. path from root to node. 
// 0 is left child. 1 is right child
int leftChildTag(int nodeTag) { return nodeTag*10; }
int rightChildTag(int nodeTag) { return nodeTag*10+1; }

int parentTag(int nodeTag)
{
    if (nodeTag == ROOT_TAG) 
    {
        std::ostringstream oss;
        oss << " Error: requested parent of ROOT_TAG"
                  << std::endl;
        std::cout << oss.str();
        return ROOT_TAG;
    } else return (nodeTag/10);
}

// Can nodeContents be divided?
bool divideP(int nodeContents)
{
    return (nodeContents >= 2);
}

// Is nodeTag the root tag?
bool isRootP(int nodeTag)
{
    return (nodeTag == ROOT_TAG);
}

// Is nodeTag a left child tag?
bool leftChildTagP(int nodeTag)
{  // note that root is not a left child
    return (nodeTag%10 == 0);
}

my_t leftChildContents(my_t nodeContents)
{
    return (nodeContents / 2);
}

my_t rightChildContents(my_t nodeContents)
{
    return (nodeContents - leftChildContents(nodeContents));
}

// Divide a node into two child nodes if it can be divided
int Divide::execute(const int & t, DivConq_context & c ) const
{
    my_t temp, tempL, tempR;  // hold contents of items
    c.divideItem.get(t, temp);

    if (divideP(temp))  // contents can be divided
    {
        tempL = leftChildContents(temp); 
        tempR = rightChildContents(temp);

        // print some tracing info
        std::ostringstream oss;
        oss << "divide<" << t
            << ">: [" << temp << "<" << t << ">] -> [" 
            << tempL << "<" << leftChildTag(t) << ">], ["
            << tempR << "<" << rightChildTag(t) << ">], <" 
            << leftChildTag(t) << ">, <"
            << rightChildTag(t) << ">"
            << std::endl;
        std::cout << oss.str();

        // Put child items and tags
        c.divideItem.put(leftChildTag(t), tempL); 
        c.divideTag.put(leftChildTag(t)); 
        c.divideItem.put(rightChildTag(t), tempR);
        c.divideTag.put(rightChildTag(t));

    } else {  // contents can not be divided
        // Put conquerItem with current contents
        c.conquerItem.put(t, temp);

        // If the node is a left child, put conquerTag for the parent node
        if (leftChildTagP(t))
        {
            c.conquerTag.put(parentTag(t));

            // print some tracing info
            std::ostringstream oss;
            oss << "divide<" << t << ">: ["
                << temp << "<" << t << ">] -> conquerItem[" 
                << temp << "<" << t << ">], conquerTag<"
                << parentTag(t) << ">"
                << std::endl;
            std::cout << oss.str();
        } else {
            // print some tracing info
            std::ostringstream oss;
            oss << "divide<" << t << ">: ["
                << temp << "<" << t << ">] -> conquerItem[" 
                << temp << "<" << t << ">]"
                << std::endl;
            std::cout << oss.str();
        }
    };
    return CnC::CNC_Success;
}

// Combine two child nodes into a parent node
int Conquer::execute(const int & t, DivConq_context & c ) const
{
    my_t tempL, tempR;  // hold contents of items

    c.conquerItem.get(leftChildTag(t), tempL);     
    c.conquerItem.get(rightChildTag(t), tempR); 

    // Put conquerItem with combined contents from its children
    c.conquerItem.put(t, tempL+tempR);

    // If the node is a left child, put conquerTag for the parent node
    if (leftChildTagP(t)) 
    {
        c.conquerTag.put(parentTag(t));

        // print some tracing info
        std::ostringstream oss;
        oss << "conquer<" << t << ">: ["
            << tempL << "<" << leftChildTag(t) << ">], [" 
            << tempR << "<" << rightChildTag(t) << ">] -> [" 
            << tempL+tempR << "<" << t << ">], <"
            << parentTag(t) << ">"
            << std::endl;
        std::cout << oss.str();
    } else {
        // print some tracing info
        std::ostringstream oss;
        oss << "conquer<" << t << ">: ["
            << tempL << "<" << leftChildTag(t) << ">], [" 
            << tempR << "<" << rightChildTag(t) << ">] -> [" 
            << tempL+tempR << "<" << t << ">]"
            << std::endl;
        std::cout << oss.str();
    }
    return CnC::CNC_Success;
}


int main(
     int argc,
     char* argv[])
{
    // Create an instance of the context class which defines the graph
    DivConq_context c;

    // User input for the value of the root node.
    // This is the contents as opposed to tag.
    // It can be any positive integer
    const int INPUT_CONTENTS = 18;
    std::cout << "starting program  INPUT_CONTENTS = " <<
        INPUT_CONTENTS << std::endl;

    // For each item from the environment (ENV), put the item using the  
    // proper tag    
    c.divideItem.put(ROOT_TAG, my_t(INPUT_CONTENTS));

    // For each tag value from the environment (ENV), put the tag into
    // the proper tag-collection
    c.divideTag.put(ROOT_TAG);

    // Wait for all steps to finish
    c.wait();

    // For each output to the environment (ENV), get the item using the 
    // proper tag    
    int conquerItem_ENV;
    c.conquerItem.get(ROOT_TAG, conquerItem_ENV);

    // Output the result
    std::cout << "result = " << conquerItem_ENV << std::endl;
}
