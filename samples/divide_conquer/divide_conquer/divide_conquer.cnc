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
[int divideItem <int>];
[int conquerItem <int>];
<int divideTag>;
<int conquerTag>;

// Step Prescriptions
<divideTag> :: (divide);
<conquerTag> :: (conquer);

// Input from the caller: a root item and a root tag
env -> [divideItem: rootTag], <divideTag: rootTag>;

// Step Executions
[divideItem: tag] -> (divide: tag);
(divide: tag)    -> [divideItem: leftChild(tag)], 
                    [divideItem: rightChild(tag)], 
                    <divideTag: leftChild(tag)>, 
                    <divideTag: rightChild(tag)>,
                    [conquerItem: tag], 
                    <conquerTag: parent(tag)>; 
[conquerItem: leftChild(tag)], [conquerItem: rightChild(tag)] -> (conquer: tag);
(conquer: tag) -> [conquerItem: tag], <conquerTag: parent(tag)>; 

// Return to the caller
[conquerItem: rootTag] -> env;
