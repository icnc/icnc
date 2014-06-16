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
#ifndef genIter_H_ALREADY_INCLUDED
#define genIter_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct genIter_context;

// The step classes

struct ExecuteIterations
{
    int execute( const int & t, genIter_context & c ) const;
};

struct GenerateIterations
{
    int execute( const int & t, genIter_context & c ) const;
};


// The context class
struct genIter_context : public CnC::context< genIter_context >
{
    // Step collections
    CnC::step_collection< ExecuteIterations > executeIterations;
    CnC::step_collection< GenerateIterations > generateIterations;

    // Item collections
    CnC::item_collection< int, int > imax;
    CnC::item_collection< int, myType > result;

    // Tag collections
    CnC::tag_collection< int > iterations;
    CnC::tag_collection< int > single;

    // The context class constructor
    genIter_context()
        : CnC::context< genIter_context >(),
	  // Initialize each step collection
          executeIterations( *this ),
          generateIterations( *this ),
          // Initialize each item collection
          imax( *this ),
          result( *this ),
          // Initialize each tag collection
          iterations( *this ),
          single( *this )
    {
        // Prescriptive relations
        iterations.prescribes( executeIterations, *this );
        single.prescribes( generateIterations, *this );

	generateIterations.consumes( imax );
	generateIterations.controls( iterations );

	executeIterations.produces( result );
    }
};

#endif // genIter_H_ALREADY_INCLUDED
