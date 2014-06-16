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

typedef struct {
    int value;
    int has_children;
} myNodeStruct;

typedef std::pair<int,int> pair;

#include <cnc/cnc.h>
#include "search_in_sorted_tree.h"

int Search::execute(const pair & t, searchInSortedTree_context & c ) const
{
    // Get value to be searched
    int node = t.first;
    int i = t.second;
    int search_val;
    c.searchVal.get(i, search_val);

    // Get node value
    myNodeStruct content;
    c.treeNode.get(node, content);
    int node_val = content.value;
    int has_children = content.has_children;
    //printf("node=%d i=%d search_val=%d node_val=%d has_children=%d\n",
    //    node, i, search_val, node_val, has_children);

    if (has_children) {  // at intermediate node
        if (search_val <= node_val) {
            // Put tag to search left
            c.searchTag.put(pair(node*10, i));
        } else {
            // Put tag to search right
            c.searchTag.put(pair(node*10+1, i));
        }
    } else {  // at leave node
        if (search_val == node_val) {
            // Found. Put <success>
            c.success.put(node, i);
        }
    }

    return CnC::CNC_Success;
}

#define N_NODES 15
#define N_SEARCH_VALS 5

int main(int argc, char* argv[])
{
    // Create an instance of the context class which defines the graph
    searchInSortedTree_context c;
    int i;

    // Here we made up a simple sorted tree (see Readme.txt for description).
    // The sorted values are at the leaves: 5 15 25 35 45 55 65 75
    //
    // (a node is shown as value<node-tag>)
    //
    //                      35<1> (the root treeNode) 
    //                    /                           \
    //               15<10>                            55<11>
    //             /        \                         /        \
    //       5<100>          25<101>            45<110>         65<111>
    //        /\                /\                /\                /\
    //  5<1000> 15<1001> 25<1010> 35<1011> 45<1100> 55<1101> 65<1110> 75<1111> 

    // Put all [treeNode]
    int vals[N_NODES] = {35,15,55,5,25,45,65,5,15,25,35,45,55,65,75};
    int has_children[N_NODES] = {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0};
    int nodes[N_NODES] = {1,10,11,100,101,110,111,1000,1001,1010,1011,1100,
                          1101,1110,1111};
    for (i = 0; i < N_NODES; i++) {
        myNodeStruct content = {vals[i], has_children[i]};
        c.treeNode.put(nodes[i], content);
    }

    // Put all [searchVal] and Put initial set of <searchTag>
    int search_val[N_SEARCH_VALS] = {20, 35, 11, 99, 65};
    int root = 1;
    for (i = 0; i < N_SEARCH_VALS; i++) {
        c.searchVal.put(i, search_val[i]);
        c.searchTag.put(pair(root, i));
    }

    // Wait for all steps to finish
    c.wait();

    // Execution of the graph has finished.
    // Iterate over <success> and print values and associated tree nodes.
    std::cout << "These values are found in the sorted tree:" << std::endl;
    CnC::item_collection<int, int >::const_iterator cii;
    for (cii = c.success.begin(); cii != c.success.end(); cii++) { 
        int node_id = cii->first;
        int search_val_id = *cii->second;
        std::cout << search_val[search_val_id] << " is found at node <" 
                  << node_id << ">" << std::endl;
    }
}
