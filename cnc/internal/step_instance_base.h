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

#include <cnc/internal/chronometer.h>

#ifndef STEP_INSTANCE_BASE_ALREADY_INCLUDED
#define STEP_INSTANCE_BASE_ALREADY_INCLUDED

#include <iostream>
#include <ostream>
#include <sstream>
#include <list>

#include <cnc/internal/tbbcompat.h>
#include <tbb/queuing_rw_mutex.h>

#include <cnc/internal/schedulable.h>
#include <cnc/internal/get_list.h>
#include <cnc/internal/scalable_object.h>

namespace CnC {
    namespace Internal {

        class tag_base;
        class step_launcher_i;
        class context_base;
        class scheduler_i;

        /// the base class for every step-instance.
        /// We have no tag here, so we handle things that are tag-independent, like timing
        /// and a global id.
        class CNC_API step_instance_base : public schedulable, public scalable_object, CnC::Internal::no_copy
        {
        public:
            // Normal constructor
            step_instance_base( context_base & graph, int priority, scheduler_i & sched );
            ~step_instance_base();

            inline int id() { return m_id; }

            /// we return the tag_base as a tag (no type info).
            virtual const tag_base & tag() const = 0;

            inline context_base & context() const;
    
            inline void start_timing();
            inline void stop_timing( const StepReturnValue_t rt, const std::string & name );
            inline void start_get();
            inline void stop_get();
            inline void start_put();
            inline void stop_put();
    
            /// set time log collection for this instance
            inline void enable_timing();

            inline get_list& glist();

        protected:
            static tbb::queuing_rw_mutex m_globalMutex;

            context_base       & m_context;
            get_list             m_getList;
        private:
            chronometer::step_timer * m_timer;
            int                  m_id;
            
        }; // class step_instance_base 


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline context_base & step_instance_base::context() const
        {
            return m_context; 
        }

        inline void step_instance_base::start_timing()
        {
            if( ! m_timer ) return;
            m_timer->start();
        }

        inline void step_instance_base::stop_timing( const StepReturnValue_t rt, const std::string & name )
        {
            if( ! m_timer ) return;
            m_timer->log( name.c_str(), id(), rt );
        }

        inline void step_instance_base::start_get()
        {
            if( ! m_timer ) return;
            m_timer->start_get();
        }

        inline void step_instance_base::stop_get()
        {
            if( ! m_timer ) return;
            m_timer->stop_get(); 
        }

        inline void step_instance_base::start_put()
        { 
            if( ! m_timer ) return;
            m_timer->start_put();
        }

        inline void step_instance_base::stop_put()
        {
            if( ! m_timer ) return;
            m_timer->stop_put();
        }

        inline void step_instance_base::enable_timing()
        {
            m_timer = new chronometer::step_timer;
        }

        inline get_list& step_instance_base::glist()
        {
            return m_getList;
        }

    } //    namespace Internal
} // end namespace CnC

#endif //STEP_INSTANCE_BASE_ALREADY_INCLUDED
