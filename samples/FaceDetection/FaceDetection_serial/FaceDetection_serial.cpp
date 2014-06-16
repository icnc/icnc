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

using namespace std;

double do_something_to_take_time (int first, int last) {

   // Comment out the following return statement for work to take time
   return 0.0;
/*
   int c;
   double x = 2.387;
   double d = 0;
   for( c = first; c <= last; c++ ) {
		d += _jn( c, x );
		d += _y0( x );
		d += _y1( x );
		d += _yn( c, x );
   }
   return d;
*/
}


bool classifier1(string* image_item) {
   // Determine if this "face" has "eyes"
   for (int i=0; i<3; i++) {
	   double d = do_something_to_take_time (2, 500);
	   if (image_item[i].compare("eyes") == 0) {
		   return true;
	   }
   }
   return false;
}

bool classifier2(string* image_item) {
   // Determine if this "face" has a "nose"
   for (int i=0; i<3; i++) {
	   double d = do_something_to_take_time (2, 500);
	   if (image_item[i].compare("nose") == 0) {
		   return true;
	   }
   }
   return false;
}

bool classifier3(string* image_item) {
   // Determine if this "face" has a "mouth"
   for (int i=0; i<3; i++) {
	   double d = do_something_to_take_time (2, 500);
	   if (image_item[i].compare("mouth") == 0) {
		   return true;
	   }
   }
   return false;
}

extern vector<string*>* facedetector(vector<string*>* images) {
	vector <string*>::iterator i_images;
	vector<string*>* faces = new vector<string*>;
	int i;
	for (i=0, i_images = images->begin(); i_images != images->end(); i++, i_images++) {
		string* face = *i_images;
		bool b;
		b = classifier1(face);
		if (b) b = classifier2(face);
		if (b) b = classifier3(face);
		if (b) faces->push_back(face);
	}
	return faces;
}
