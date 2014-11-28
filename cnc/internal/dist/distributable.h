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

#ifndef _CnC_DISTRIBUTABLE_H_
#define _CnC_DISTRIBUTABLE_H_

#include <cnc/internal/traceable.h>
#include <cnc/internal/cnc_stddef.h>

namespace CnC {
    
    class serializer;

    namespace Internal {

        enum { NO_COMPUTE_ON = -77777 };

        /// Collections must be derived from this if they want to distribute their
        /// data across processes.
        /// \see creatable for objects which need explicit creation on a remote client
        /// \see CnC::Internal::dist_cnc
        class distributable : public virtual traceable
        {
        public:
            distributable( const std::string & name, int level = 0 ) : traceable( name, level ), m_gid( -1 ) {}
            distributable() : traceable( "distributable", 0 ), m_gid( -1 ) {}
            virtual ~distributable() {}

            /// de-serialize message from serializer and consume it
            /// a distributable class must implement its own protocol
            /// messages can only be sent through the distributor/dist_context and only to its clones
            virtual void recv_msg( serializer * ) = 0;

            /// returns global id of this distributable
            int gid() const { return m_gid; }

            /// sets global id
            void set_gid( int gid ) { m_gid = gid; }

            /// reset instance, e.g. remove entries from collection
            /// \param dist if false, the current context is actually not distributed -> don't sync with remote siblings
            virtual void unsafe_reset( bool dist ) = 0;

            /// cleanup instance, e.g. collect garbage
            virtual void cleanup() {}

            bool subscribed() const { return m_gid >= 0; }

            /// type id from creatable
            //            virtual int factory_id() { CNC_ABORT( "Creatable interface not implemented" ); }
            /// serialize object
            //  virtual void serialize( serializer & ) { CNC_ABORT( "Creatable interface not implemented" ); }

        private:
            int m_gid;
        };

    } // namespace Internal
} // namespace CnC

#endif // _CnC_DISTRIBUTABLE_H_
