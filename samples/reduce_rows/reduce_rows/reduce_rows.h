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
#ifndef redRows_H_ALREADY_INCLUDED
#define redRows_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct redRows_context;

// The step classes

struct ProcessFixedItems
{
    int execute( const pair & t, redRows_context & c ) const;
};


// The context class
struct redRows_context : public CnC::context< redRows_context >
{
    // Step collections
    CnC::step_collection< ProcessFixedItems > processFixedItems;

    // Item collections
    CnC::item_collection< pair, int > readOnlyItem;
    CnC::item_collection< pair, int > sum;

    // Tag collections
    CnC::tag_collection< pair > fixedIterations;

    // The context class constructor
    redRows_context()
        : CnC::context< redRows_context >(),
          // Initialize each step collection
	  processFixedItems( *this ),
          // Initialize each item collection
          readOnlyItem( *this ),
          sum( *this ),
          // Initialize each tag collection
          fixedIterations( *this )
    {
        // Prescriptive relations
        fixedIterations.prescribes( processFixedItems, *this );

	processFixedItems.consumes( readOnlyItem );
	processFixedItems.consumes( sum );
	processFixedItems.produces( sum );
    }
};

#endif // redRows_H_ALREADY_INCLUDED
