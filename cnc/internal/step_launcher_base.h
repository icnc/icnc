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

#ifndef STEP_LAUNCHER_BASE_H_ALREADY_INCLUDED
#define STEP_LAUNCHER_BASE_H_ALREADY_INCLUDED

#include <cnc/internal/step_launcher_i.h>
#include <cnc/internal/tagged_step_instance.h>
#include <cnc/internal/no_range.h>

#ifdef _DIST_CNC_
# include <cnc/internal/dist/distributor.h>
# include <cnc/internal/context_base.h>
#endif

namespace CnC {
    namespace Internal {

        class scheduler_i;

        /// step_launcher functionality that can be done without tuners/collections.
        /// We have the tag/range, though.
        /// Mostly takes care of un/subscription with context.
        template< class Tag, class Range >
        class step_launcher_base : public step_launcher_i
        {
        public:
            step_launcher_base( context_base & ctxt );
            virtual ~step_launcher_base();

            //virtual int getPriority( const T & tag ) const = 0;
            virtual tagged_step_instance< Tag > * create_step_instance( const Tag &, context_base &, bool ) const = 0;
            virtual tagged_step_instance< Range > * create_range_step_instance( const Range &, context_base & ) const = 0;
            context_base & context() const { return m_context; }
            int id() const { return m_id; }
            void set_id( const int id ) { m_id = id; }

        private:
            context_base & m_context;
            int            m_id;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Tag, class Range >
        step_launcher_base< Tag, Range >::step_launcher_base( context_base & ctxt )
            : m_context( ctxt ),
              m_id( -1 )
        {
            m_context.subscribe( this );
        }

        template< class Tag, class Range >
        step_launcher_base< Tag, Range >::~step_launcher_base()
        {
            m_context.unsubscribe( this );
        }

    } // namespace Internal
} // end namespace CnC


#endif //STEP_LAUNCHER_BASE_H_ALREADY_INCLUDED
