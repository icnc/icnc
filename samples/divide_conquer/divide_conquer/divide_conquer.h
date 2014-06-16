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
#ifndef DivConq_H_ALREADY_INCLUDED
#define DivConq_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct DivConq_context;

// The step classes

struct Conquer
{
    int execute( const int & t, DivConq_context & c ) const;
};

struct Divide
{
    int execute( const int & t, DivConq_context & c ) const;
};


// The context class
struct DivConq_context : public CnC::context< DivConq_context >
{
    // Step collections
    CnC::step_collection< Conquer > conquer;
    CnC::step_collection< Divide >  divide;

    // Item collections
    CnC::item_collection< int, int > conquerItem;
    CnC::item_collection< int, int > divideItem;

    // Tag collections
    CnC::tag_collection< int > conquerTag;
    CnC::tag_collection< int > divideTag;

    // The context class constructor
    DivConq_context()
        : CnC::context< DivConq_context >(),
	  // Initialize each step collection
          conquer( *this ),
          divide( *this ),
          // Initialize each item collection
          conquerItem( *this ),
          divideItem( *this ),
          // Initialize each tag collection
          conquerTag( *this ),
          divideTag( *this )
    {
        // Prescriptive relations
        conquerTag.prescribes( conquer, *this );
        divideTag.prescribes( divide, *this );

	divide.consumes( divideItem );
	divide.controls( divideTag );
	divide.controls( conquerTag );
	divide.produces( divideItem );
	divide.produces( conquerItem );

	conquer.consumes( conquerItem );
	conquer.controls( conquerTag );
	conquer.produces( conquerItem );
    }
};

#endif // DivConq_H_ALREADY_INCLUDED
