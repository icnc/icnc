/* *******************************************************************************
 *  Copyright (c) 2007-2014, Intel Corporation
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/

#ifndef _CNC_STEP_DELAYER_H_ALREADY_INCLUDED
#define _CNC_STEP_DELAYER_H_ALREADY_INCLUDED

//#include <cnc/default_tuner.h>
#include <cnc/internal/item_collection_i.h>
//#include <cnc/internal/dist/distributor.h>

namespace CnC {
    namespace Internal {

        class step_instance_base;

        /// Forwards depends calls to internal item-collection
        /// We hope that the collection being a template provides
        /// much optimization potential to the compiler.
        class step_delayer
        {
        public:
            step_delayer() {}

            /// \param iC             item-collection where the item of interest lives in
            /// \param tag            the tag of the item this step depends on
            template< class i_collection, class tag_type > // FIXME no second arg required
            void depends( i_collection & iC, const /*FIXME why doesn't this work anymore? typename i_collection::*/ tag_type & tag ) const
            {
                iC.m_itemCollection.delay_step( tag );
            }
        };

    } // namespace Internal
} // end namespace CnC


#endif //_CNC_STEP_DELAYER_H_ALREADY_INCLUDED
