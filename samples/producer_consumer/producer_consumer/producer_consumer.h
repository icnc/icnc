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
#ifndef ProdCons_H_ALREADY_INCLUDED
#define ProdCons_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct ProdCons_context;

// The step classes

struct Consumer
{
    int execute( const int & t, ProdCons_context & c ) const;
};

struct Producer
{
    int execute( const int & t, ProdCons_context & c ) const;
};


// The context class
struct ProdCons_context : public CnC::context< ProdCons_context >
{
    // Step collections
    CnC::step_collection< Producer > producer;
    CnC::step_collection< Consumer > consumer;

    // Item collections
    CnC::item_collection< int, my_t > inItem;
    CnC::item_collection< int, my_t > outItem;
    CnC::item_collection< int, my_t > valueItem;

    // Tag collections
    CnC::tag_collection< int > prodConsTag;

    // The context class constructor
    ProdCons_context()
        : CnC::context< ProdCons_context >(),
	  // Initialize each step collection
	  producer( *this ),
	  consumer( *this ),
          // Initialize each item collection
          inItem( *this ),
          outItem( *this ),
          valueItem( *this ),
          // Initialize each tag collection
          prodConsTag( *this )
    {
        // Prescriptive relations
        prodConsTag.prescribes( consumer, *this );
        prodConsTag.prescribes( producer, *this );

	producer.consumes( inItem );
	producer.produces( valueItem );

	consumer.consumes( valueItem );
	consumer.produces( outItem );
    }
};

#endif // ProdCons_H_ALREADY_INCLUDED
