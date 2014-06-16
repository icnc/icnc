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
[my_t inItem <int>];     // items consumed by producer
[my_t valueItem <int>];  // items produced by producer and consumed by consumer
[my_t outItem <int>];    // items produced by consumer
<int prodConsTag>;      // tags prescribing producer and consumer steps

// For each instance of prodConsTag there will be an instance of producer and
// an instance of consumer
<prodConsTag> :: (producer);
<prodConsTag> :: (consumer);

// environment produces all inItems and prodConsTags
env -> [inItem], <prodConsTag>;

// Producer consumes inItem and produces valueItem
[inItem] -> (producer) -> [valueItem];

// Consumer consumes valueItem and produces outItem
[valueItem] -> (consumer) -> [outItem];

// outItems are consumed by the environment
[outItem] -> env;
