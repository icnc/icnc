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

#ifndef  _CnC_SERVICE_TASK_H_
#define  _CnC_SERVICE_TASK_H_

#include <cnc/internal/schedulable.h>
#include <cnc/internal/scheduler_i.h>

namespace CnC {
    namespace Internal {

        /// A task that performance internal work.
        /// It gets scheduled to the same scheduler as step-instances.
        /// To launch a servide task simply create it like this
        ///    new service_task< my_function_type, my_arg_type >( my_function, my_arg );
        template< typename F, typename Arg >
        class service_task : public schedulable
        {
        public:
            service_task( scheduler_i & sched, const Arg & arg );
            service_task( scheduler_i & sched, const F & f, const Arg & arg );

			virtual int affinity() const { return scheduler_i::AFFINITY_HERE; }
			virtual StepReturnValue_t execute();
            virtual char prepare( step_delayer &, int &, const schedulable * );
            virtual void compute_on( int target ) { CNC_ABORT( "A service task cannot be passed on to another process." ); }
        private:
            F   m_func;
            Arg m_arg;
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename F, typename Arg >
        inline service_task< F, Arg >::service_task( scheduler_i & sched, const Arg & arg )
            : schedulable( 0, sched ),
              m_func(),
              m_arg( arg )
        {
            sched.prepare( this, false, true, true );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename F, typename Arg >
        inline service_task< F, Arg >::service_task( scheduler_i & sched, const F & f, const Arg & arg )
            : schedulable( 0, sched ),
              m_func( f ),
              m_arg( arg )              
        {
            sched.prepare( this, false, true, true );
        }

        template< typename F, typename Arg >
        inline char service_task< F, Arg >::prepare( step_delayer &, int &, const schedulable * )
        {
            m_status = CNC_Prepared;
            return CNC_Prepared;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename F, typename Arg >
        inline StepReturnValue_t service_task< F, Arg >::execute()
        {
            m_func.execute( m_arg );
            m_status = CNC_Done;
            return CNC_Success;
        }

    } // namespace Internal
} // end namespace CnC

#endif // _CnC_SERVICE_TASK_H_
