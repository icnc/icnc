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
#include <vector>
#include <math.h>
#include <string>
typedef std::string* myImageType;
#include "FaceDetection.h"

double do_something_to_take_time (int first, int last) {

   // Comment out the following return statement for work to take time
   return 0.0;

   int c;
   double x = 2.387;
   double d = 0;
   for( c = first; c <= last; c++ ) {
#ifdef WIN32
        d += _jn( c, x );
        d += _y0( x );
        d += _y1( x );
        d += _yn( c, x );
#else
        d += jn( c, x );
        d += y0( x );
        d += y1( x );
        d += yn( c, x );
#endif
   }
   return d;
}

// Classifier1 looks for eyes in the input image and if found add the tag
// to the Classifier2 tag collection.
int Classifier1::execute(const int & t, facedetector_context & c ) const
{
   // For each input item for this step retrieve the item using the proper tag
   myImageType image_item;
   c.image.get(t, image_item);
   // Determine if this "face" has "eyes"
   for (int i=0; i<3; i++) {
       double d = do_something_to_take_time (2, 500);
       if (image_item[i].compare("eyes") == 0) {
           // If we found "eyes", add the tag to the classifier2 tag collection
           c.classifier2_tags.put(t);
           return CnC::CNC_Success;
       }
   }
   return CnC::CNC_Success;
}

// Classifier2 looks for nose in the input image and if found add the tag
// to the classifier3 tag collection.
int Classifier2::execute(const int & t, facedetector_context & c ) const
{
   // For each input item for this step retrieve the item using the proper tag
   myImageType image_item;
   c.image.get(t, image_item);
   // Determine if this "face" has a "nose"
   for (int i=0; i<3; i++) {
       double d = do_something_to_take_time (2, 500);
       if (image_item[i].compare("nose") == 0) {
           // If we found a "nose", add the tag to the classifier3 tag collection
           c.classifier3_tags.put(t);
           return CnC::CNC_Success;
       }
   }
   return CnC::CNC_Success;
}

// Classifier3 looks for mouth in the input image and if found add the image
// to the face item collection.
int Classifier3::execute(const int & t, facedetector_context & c ) const
{
   // For each input item for this step retrieve the item using the proper tag
   myImageType image_item;
   c.image.get(t, image_item);
   // Determine if this "face" has a "mouth"
   for (int i=0; i<3; i++) {
       double d = do_something_to_take_time (2, 500);
       if (image_item[i].compare("mouth") == 0) {
           // If we found a "mouth", add the image to the face item collection
           c.face.put(t, image_item);
           return CnC::CNC_Success;
       }
   }
   return CnC::CNC_Success;
}

extern std::vector<myImageType>* facedetector(std::vector<myImageType>* images) {

    // Create an instance of the context class which defines the graph
    facedetector_context c;
    std::vector <myImageType>::iterator i_images;
    int i;

    // Env puts all the image items, and all classifier1_tags
    for (i=0, i_images = images->begin(); i_images != images->end(); i++, i_images++) {
        myImageType face = *i_images;
        // Put an image instance   
        c.image.put(i, face);
        // Put a classifier1_tags instance which prescribes classifier1
        c.classifier1_tags.put(i);
    }

    // Wait for all steps to finish
    c.wait();

    // Process output to the environment (ENV)
    // Iterate through the face item collection and build the vector of faces
    // to return to caller.
    std::vector<myImageType>* faces = new std::vector<myImageType>;
    CnC::item_collection<int, myImageType>::const_iterator cii;
    for (cii = c.face.begin(); cii != c.face.end(); cii++) {
        myImageType face = *(cii->second);
        faces->push_back(face);
    }
    return faces;
}
