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
#ifndef partStr_H_ALREADY_INCLUDED
#define partStr_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct partStr_context;

// The step classes

struct CreateSpan
{
    int execute( const int & t, partStr_context & c ) const;
};

struct ProcessSpan
{
    int execute( const int & t, partStr_context & c ) const;
};


// The context class
struct partStr_context : public CnC::context< partStr_context >
{
    // Step collections
    CnC::step_collection< CreateSpan > createSpan;
    CnC::step_collection< ProcessSpan > processSpan;

    // Item collections
    CnC::item_collection< int, string > input;
    CnC::item_collection< int, string > results;
    CnC::item_collection< int, string > span;

    // Tag collections
    CnC::tag_collection< int > singletonTag;
    CnC::tag_collection< int > spanTags;

    // The context class constructor
    partStr_context()
        : CnC::context< partStr_context >(),
	  // Initialize each step collection
	  createSpan( *this ),
	  processSpan( *this ),
          // Initialize each item collection
          input( *this ),
          results( *this ),
          span( *this ),
          // Initialize each tag collection
          singletonTag( *this ),
          spanTags( *this )
    {
        // Prescriptive relations
        singletonTag.prescribes( createSpan, *this );
        spanTags.prescribes( processSpan, *this );

	createSpan.consumes( input );
	createSpan.controls( spanTags );
	createSpan.produces( span );

	processSpan.consumes( span );
	processSpan.produces( results );

    }
};

#endif // partStr_H_ALREADY_INCLUDED
