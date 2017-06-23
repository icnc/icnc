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

#include <cassert>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <time.h>
#include "tbb/tick_count.h"

using namespace std;

typedef std::string* myImageType;

// NOTE: "images" in this simulation consist of 3-element string arrays.
//       Each string specifies a facial characteristic, such as "eyes", "nose",
//       "mouth".

extern vector <myImageType>* facedetector(vector <myImageType> *images);

vector <myImageType>* 
faces_init (int n)
{
    //  Allocate n 3-element vectors and randomly set the
    //  face characteristics

    //  Seed the random-number generator with current time so that
    //  the numbers will be different every time we run.
    //srand( (unsigned)time( NULL ) );
    //  Use fixed seed so we get reproducible output for testing and timing.
    srand( 1000 );

    vector <myImageType>* images = new vector <myImageType>;
    for (int i=0; i<n; i++) {
        string * characteristics = new string[3];
        int b;		//  Will be set to 0 or 1
        b = rand() % 2;
        characteristics[0] = (b == 0) ? "n/a" : "eyes"; 
        b = rand() % 2;
        characteristics[1] = (b == 0) ? "n/a" : "nose"; 
        b = rand() % 2;
        characteristics[2] = (b == 0) ? "n/a" : "mouth"; 
        images->push_back(characteristics);
    }
    return images;
}

void
faces_free(vector <myImageType>* faces) {
    vector <myImageType>::iterator i_faces;
    for (i_faces = faces->begin(); i_faces != faces->end(); i_faces++) {
        delete [] *i_faces;
    }
    delete faces;
}

void
faces_dump(vector <myImageType>* faces)
{
    vector <myImageType>::iterator i_faces;
    for (i_faces = faces->begin(); i_faces != faces->end(); i_faces++) {
        string * characteristics = *i_faces;
        std::cout << characteristics[0].c_str() << ", " 
                  << characteristics[1].c_str() << ", " 
                  << characteristics[2].c_str() << std::endl;
    }
}

void
faces_verify(vector <myImageType>* faces)
{
    //  In this simulation of a "face", a face is defined to be a 3-element
    //  vector with elements of the vector being "eyes", "nose", "mouth" in
    //  that order.  All of these images should be "faces".
    vector <myImageType>::iterator i_faces;
    for (i_faces = faces->begin(); i_faces != faces->end(); i_faces++) {
        string * characteristics = *i_faces;
        assert(characteristics[0].compare("eyes") == 0);		
        assert(characteristics[1].compare("nose") == 0);		
        assert(characteristics[2].compare("mouth") == 0);		
    }
}

int
main(int argc, char* argv[])
{
    //  Process the command line:  facedetector [-v] number_of_images
    bool verbose = false;
    int n = 0;
    int argn = 1;

    if (argc >= 2) 
    {
        if (0 == strcmp("-v", argv[argn])) 
        {
            argn++;
            verbose = true;
            if (argc == 2) {
                fprintf(stderr,"Usage: facedetector [-v] number_of_images\n");
                return -1;
            }
        }
        n = atoi(argv[argn]);
        argn++;
        if (argc > argn) {
            fprintf(stderr,"Usage: facedetector [-v] number_of_images\n");
            return -1;
        }
    } 
    else {
        fprintf(stderr,"Usage: facedetector [-v] number_of_images\n");
        return -1;
    }

    vector <myImageType>* images;
    images = faces_init(n);

    if (verbose) {
        std::cout << "Images:" << std::endl;
        faces_dump(images);
        std::cout << std::endl;
    }

#pragma warning(disable: 4533)
    tbb::tick_count t0 = tbb::tick_count::now();
    vector <myImageType>* faces = facedetector(images);
    tbb::tick_count t1 = tbb::tick_count::now();
    
    faces_verify(faces);
    
    if (verbose) {
        std::cout << "Faces:" << std::endl;
        faces_dump(faces);
        std::cout << std::endl;
    }
    std::cout << "Detected " << faces->size() << " faces from " << n << " images in " << (t1-t0).seconds() << " seconds" << std::endl;
    faces_free(images);
    return 0;
}
