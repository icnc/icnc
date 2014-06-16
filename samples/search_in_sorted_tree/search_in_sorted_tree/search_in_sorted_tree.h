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
#ifndef searchInSortedTree_H_ALREADY_INCLUDED
#define searchInSortedTree_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/default_tuner.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct searchInSortedTree_context;

// The step classes

struct Search
{
    int execute( const pair & t, searchInSortedTree_context & c ) const;
};


// The context class
struct searchInSortedTree_context : public CnC::context< searchInSortedTree_context >
{
    // Step collections
    CnC::step_collection< Search > search;

    // Item collections
    CnC::item_collection< int, int > searchVal;
    CnC::item_collection< int, myNodeStruct > treeNode;
    CnC::item_collection< int, int > success;

    // Tag collections
    CnC::tag_collection< pair > searchTag;

    // The context class constructor
#pragma warning(push)
#pragma warning(disable: 4355)
    searchInSortedTree_context()
        : CnC::context< searchInSortedTree_context >(),
          // Initialize each step collection
	  search( *this ),
          // Initialize each item collection
          searchVal( *this ),
          treeNode( *this ),
          success( *this ),
          // Initialize each tag collection
          searchTag( *this )
    {
        // Prescriptive relations
        searchTag.prescribes( search, *this );
    }
#pragma warning(pop)
};

#endif // searchInSortedTree_H_ALREADY_INCLUDED
