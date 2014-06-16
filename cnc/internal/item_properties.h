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

#ifndef _CnC_ITEM_PROPERTIES_H_
#define _CnC_ITEM_PROPERTIES_H_

#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/scalable_vector.h>

namespace CnC {
    namespace Internal {

        /// This holds properties of an item: getcount, suspendgroup, subscriber list and owner
        /// The getcount is an atomic var, hence we can access them without additional protection.
        /// Subscriber list and suspend group must be protected when being accessed.
        /// To save space, we encode ownership (distCnC) in the getcount. This is a little hacky, but 
        /// the addtional storage attached to each item is already huge.
        struct item_properties
        {
            typedef scalable_vector< int > subscriber_list_type;
            enum {
                NO_GET_COUNT    = (int)0x80000000,
                UNSET_GET_COUNT = (int)0x80000001
            };
            
            item_properties( int getcount = UNSET_GET_COUNT )
                : m_suspendGroup( NULL ),
#ifdef _DIST_CNC_
                  m_subscribers( NULL ),
                  m_owner( UNSET_OWNER ),
#endif
                  m_getCount()
            {
                m_getCount = getcount;
            }

            int get_count() const
            {
                return m_getCount;
            }

            int decrement_get_count( int cnt = 1 )
            {
                return m_getCount -= cnt;
            }
            
            int increment_get_count( int cnt = 1 )
            {
                return m_getCount += cnt;
            }

            int set_get_count( int cnt )
            {
                return m_getCount = cnt;
            }

            int set_or_increment_get_count( int cnt )
            {
                int _tmp = m_getCount.compare_and_swap( cnt, UNSET_GET_COUNT );
                if( _tmp == UNSET_GET_COUNT ) return _tmp;
                CNC_ASSERT( static_cast< unsigned int >( _tmp ) != NO_GET_COUNT );
                return increment_get_count( cnt );
            }


#ifdef _DIST_CNC_
            int owner() const
            {
                return m_owner & ~CREATOR_MASK;
            }
            
            bool am_owner() const
            {
                return ( ( m_owner & ~CREATOR_MASK ) == distributor::myPid() );
            }

            bool has_owner() const
            {
                return m_owner != static_cast< int >( UNSET_OWNER );
            }

            void set_owner( int o )
            {
                m_owner = o;
            }

            // always set_owner prior to set_creator!
            void unset_creator()
            {
                CNC_ASSERT( m_owner != -1 );
                m_owner |= CREATOR_MASK;
            }

            bool am_creator() const
            {
                return ( m_owner & CREATOR_MASK ) == 0;
            }

            bool has_content()
            {
                return m_suspendGroup != NULL || m_subscribers != NULL || get_count() != static_cast< int >( item_properties::UNSET_GET_COUNT );
            }
#else
            int owner() const
            {
                return 0;
            }
            
            bool am_owner() const
            {
                return true;
            }

            bool am_creator() const
            {
                return true;
            }

            bool has_owner() const
            {
                return true;
            }

            void set_owner( int ) 
            {
            }

            bool has_content()
            {
                return m_suspendGroup != NULL || get_count() != static_cast< int >( item_properties::UNSET_GET_COUNT );
            }
#endif

            scheduler_i::suspend_group * m_suspendGroup;
#ifdef _DIST_CNC_
            subscriber_list_type       * m_subscribers;
        private:
            int                          m_owner;
            enum { CREATOR_MASK = 0x80000000, UNSET_OWNER = 0xffffffff };
#else
        private:
#endif
            tbb::atomic< int >           m_getCount;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CnC_ITEM_PROPERTIES_H_
