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

#ifndef STEP_LAUNCHER_I_H_ALREADY_INCLUDED
#define STEP_LAUNCHER_I_H_ALREADY_INCLUDED

#include <cnc/internal/schedulable.h>
#include <cnc/internal/tag_base.h>
#include <cnc/internal/dist/distributable.h>
#include <cnc/internal/cnc_api.h>

#include <string>


namespace CnC {
    namespace Internal {

        class step_instance_base;
        class step_delayer;


        /// Everyting we can in a step_launcher that doesn't require type information
        /// That's almost nothing.
        /// FIXME: step_launchers need revision
        ///        It should be possible to reduce it to a simple and static proxy to the Step
        class step_launcher_i : public distributable
        {
        public:
            step_launcher_i() : distributable( "step_launcherNN" ) {}

            virtual bool timing() const = 0;

            virtual void unsafe_reset( bool ) {};
        }; // class step_launcher_i

    } // namespace Internal
} // end namespace CnC


#endif //STEP_LAUNCHER_I_H_ALREADY_INCLUDED
