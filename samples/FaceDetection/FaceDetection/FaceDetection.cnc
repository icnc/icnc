//********************************************************************************
// Copyright (c) 2007-2010 Intel Corporation. All rights reserved.              **
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

// Declarations
[myImageType image <int>: imageID];   	// Input images
[myImageType face <int>: imageID];		// Images recognized as a face
<int classifier1_tags: imageID>;		// First classifier tag collection
<int classifier2_tags: imageID>;		// Second classifier tag collection
<int classifier3_tags: imageID>;		// Third classifier tag collection

// Step Prescriptions
<classifier1_tags> :: (classifier1);
<classifier2_tags> :: (classifier2);
<classifier3_tags> :: (classifier3);

// Input from the caller; a set of images and a set of
// tags that assign the images to be processed by classifier1
env -> [image], <classifier1_tags>;

// Step Executions
[image] -> (classifier1) -> <classifier2_tags>;
[image] -> (classifier2) -> <classifier3_tags>;
[image] -> (classifier3) -> [face];

// Return the faces to the caller
[face] -> env;
