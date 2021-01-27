/* *******************************************************************************
 *  Copyright (c) 2007-2021, Intel Corporation
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

#ifndef _HASH_TAG_TABLE_INCLUDED_
#define _HASH_TAG_TABLE_INCLUDED_

#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_unordered_map.h>
#include <cnc/internal/cnc_tag_hash_compare.h>

namespace CnC {
    namespace Internal {

        struct empty_type {};

        /// Used for to memoize tags in tag-collections.
        /// Simply forwards to tbb::concurrent_unordered_map.
        /// If you want a different container, simply implement this interface.
        template< typename Tag, typename H = cnc_hash< Tag >, typename E = cnc_equal< Tag > >
        class hash_tag_table : private tbb::concurrent_unordered_map< Tag, int, H, E  >
        {
        private:
            typedef tbb::concurrent_unordered_map< Tag, int, H, E  > parent;
        public:
            typedef typename parent::const_iterator const_iterator;

            inline const_iterator begin() const
            {
                return parent::begin();
            }

            inline const_iterator end() const
            {
                return parent::end();
            }

            inline size_t size() const
            {
                return parent::size();
            }

            inline bool empty() const
            {
                return parent::empty();
            }

            inline void clear()
            {
                parent::clear();
            }

            // returns the bits of v that were already set before the insert
            int insert( const Tag & t, const int v )
            {
                typename parent::iterator _i = parent::insert( typename parent::value_type( t, 0 ) ).first;
                int _r = _i->second & v;
                _i->second |= v;
                return _r;
            }
        };

    } // namespace Internal
} // namespace CnC

#endif // _HASH_TAG_TABLE_INCLUDED_
