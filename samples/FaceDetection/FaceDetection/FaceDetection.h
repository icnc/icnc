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
#ifndef facedetector_H_ALREADY_INCLUDED
#define facedetector_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct facedetector_context;

// The step classes

struct Classifier1
{
    int execute( const int & t, facedetector_context & c ) const;
};

struct Classifier2
{
    int execute( const int & t, facedetector_context & c ) const;
};

struct Classifier3
{
    int execute( const int & t, facedetector_context & c ) const;
};


// The context class
struct facedetector_context : public CnC::context< facedetector_context >
{
    // Step collections
    CnC::step_collection< Classifier1 > classifier1;
    CnC::step_collection< Classifier2 > classifier2;
    CnC::step_collection< Classifier3 > classifier3;


    // Item collections
    CnC::item_collection< int, myImageType > face;
    CnC::item_collection< int, myImageType > image;

    // Tag collections
    CnC::tag_collection< int > classifier1_tags;
    CnC::tag_collection< int > classifier2_tags;
    CnC::tag_collection< int > classifier3_tags;

    // The context class constructor
#pragma warning(push)
#pragma warning(disable: 4355)
    facedetector_context()
        : CnC::context< facedetector_context >(),
	  // Initialize each step collection
          classifier1( *this ),
          classifier2( *this ),
          classifier3( *this ),
          // Initialize each item collection
          face( *this ),
          image( *this ),
          // Initialize each tag collection
          classifier1_tags( *this ),
          classifier2_tags( *this ),
          classifier3_tags( *this )
    {
        // Prescriptive relations
        classifier1_tags.prescribes( classifier1, *this );
        classifier2_tags.prescribes( classifier2, *this );
        classifier3_tags.prescribes( classifier3, *this );

	classifier1.consumes( image );
	classifier1.controls( classifier2_tags );
     
	classifier2.consumes( image );
	classifier2.controls( classifier3_tags );
     
	classifier3.consumes( image );
	classifier3.produces( face );
     
    }
#pragma warning(pop)
};

#endif // facedetector_H_ALREADY_INCLUDED
