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

#ifndef _CNC_SUSPEND_GROUP_INCLUDED_
#define _CNC_SUSPEND_GROUP_INCLUDED_

#define _CRT_SECURE_NO_DEPRECATE

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/scalable_vector.h>

#include <cnc/internal/cnc_stddef.h>
#include <iostream>

namespace CnC {
    namespace Internal {

        /// Represents a group of step-instances (schedulables) which wait for one thing
        /// (usually a item-instance) to become availble.
        /// A suspend group is not thread-safe. Make sure you use appropriate locking mechanisms.
        class CNC_API scheduler_i::suspend_group
        {
        public:
            /// create suspend group with one member
            suspend_group( schedulable * sI ) : m_group() { add( sI ); }
            ~suspend_group() { CNC_ASSERT( m_group.size() == 0 ); }

            /// add new member to group
            void add( schedulable * sI ) { m_group.push_back( sI ); }

            /// apply an action to all members and remove all members from it.
            template< class for_each, class T >
            void for_all( for_each & forEach, T arg );
            //void dump() const;

        private:
            typedef scalable_vector_p< schedulable * > my_concurrent_queue;
            my_concurrent_queue m_group;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class for_each, class T >
        void scheduler_i::suspend_group::for_all( for_each & forEach, T arg )
        {
            for( my_concurrent_queue::iterator i = m_group.begin(); i != m_group.end(); ++ i ) {
                forEach( *i, arg );
            }
            m_group.resize( 0 );
        }

    } // namespace Internal
} // end namespace CnC
#endif // _CNC_SUSPEND_GROUP_INCLUDED_
