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

#pragma once
#ifndef _CNC_GET_LIST_H_ALREADY_INCLUDED
#define _CNC_GET_LIST_H_ALREADY_INCLUDED

#include <cnc/internal/item_collection_i.h>



namespace CnC {
    namespace Internal {

        typedef std::pair< item_collection_i *, const tag_base * > get_item_t;
        typedef std::list< get_item_t, tbb::scalable_allocator< get_item_t > > get_list_t;

        /// A user might provide a definition of item_tuner::get_count.
        /// This count indicates the number of "Gets" that will be
        /// made to this item.  When the number reaches the ref_count, then the
        /// item is deleted from the item collection.
        ///
        /// Steps instances may be replayed.  We cannot update the ref_counts for
        /// a step instance until we know the step instance has committed, and
        /// will not replay again.  The design is to maintain a "get_list" for
        /// each step instance, that contains the items accessed by Gets in this
        /// step.  
        ///
        /// If the step is replayed, the get_list is cleared.  When the
        /// step is committed, the get_list is traversed, and the ref_counts are
        /// decremented.  If a ref_count is decremented to zero, the corresponding
        /// item is deleted from the item_collection.
        ///
        /// Summary:
        ///
        /// 1. We add to get list whenever we retrieve an item in a call to Get.
        /// 2. We update the counts when we commit a step instance in step_instance::execute,
        ///    and clear the get_list.
        /// 3. If we replay, we clear the get list where ever we catch data_not_ready.
        ///
        /// Cases
        /// 1. Normal get.
        ///    If we find the data in the item collection, we update the get_list and return.
        /// 
        ///    If we do not find the data, we call wait_for_put.  If we find the data
        ///    in wait_for_put, we update the get_list and return.  Otherwise we
        ///    queue the step instance, throw data not ready, and clear the get_list.
        /// 
        /// 2. depends.  Calls wait_for_put directly, bypassing the Get
        ///    routine. The items retrieved by depends are discarded.  No get list
        ///    updates are required.  Note this means we cannot update the
        ///    get_list in wait_for_put.
        /// 
        /// 3. unsafe_get
        ///    A different Get method.  It calls wait_for_put directly.  If the
        ///    data is found, we must update the get list.  When done_getting is
        ///    called, if any of the Gets have been queued, we throw data not
        ///    ready and clear the get list.  Otherwise we continue.  Note
        ///    data_not_ready may be caught by step_instance::prepare.  We need to
        ///    clear the get list here as well as in step_instance::execute.
        class get_list
        {
        public:
            void push( item_collection_i *itemCollection, const tag_base *tag_ptr )
            {
                m_getList.push_back( get_item_t( itemCollection, tag_ptr ) );
            }

            void clear()
            {
                // free the storage referenced by tag
                for ( get_list_t::iterator it = m_getList.begin();
                      it != m_getList.end(); it++ ) {

                    const tag_base *tag = it->second;
                    delete tag;
                }
                m_getList.clear();
            }

            void decrement()
            {
                for ( get_list_t::iterator it = m_getList.begin();
                      it != m_getList.end(); it++ ) {
                    item_collection_i *itemCollection = it->first;
                    const tag_base *tag = it->second;
                    itemCollection->decrement_ref_count( tag );
                }
            }
            

        private:
            get_list_t m_getList;
        };

    } // namespace Internal
} // end namespace CnC


#endif //_CNC_STEP_DELAYER_H_ALREADY_INCLUDED

