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

/*
  Implementation of CnC::context
  Mostly very thin wrappers calling context_base.
  The wrappers hide internal functionality from the API.
*/

#ifndef _CnC_CONTEXT_H_
#define _CnC_CONTEXT_H_

#include <cnc/internal/tbbcompat.h>
#include <cnc/internal/strided_range.h>
#include <cnc/internal/range_step_instance.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/dist/creator.h>
#include <cnc/default_tuner.h>
#include <cnc/itac.h>

namespace CnC {
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline context< Derived >::context()
        : Internal::context_base()
    {
        init();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline context< Derived >::context( bool d )
        : Internal::context_base( d )
    {
        init();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline void context< Derived >::init()
    {
        CNC_CHECK_TBB_VERSION();
#ifndef _DIST_CNC_
        VT_INIT();
#endif
        //grrr, cannot do this earlier
        m_distributionEnabled = factory_id() >= 0;
#ifdef _DIST_CNC_
        if( m_distributionEnabled && Internal::distributor::distributed_env() ) {
            Internal::distributor::distribute( this );
            CNC_ASSERT( subscribed() );
        }
        // if not in distributed env, remote copies are created only when needed (lazily in new_serializer)
#endif
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline context< Derived >::~context()
    {
#ifdef _DIST_CNC_
        if( m_distributionEnabled ) {
            Internal::distributor::undistribute( this );
        }
#endif
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline error_type context< Derived >::wait()
    {
        Internal::context_base::wait( NULL );
        return 0;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline void context< Derived >::flush_gets()
    {
        Internal::context_base::flush_gets();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline int context< Derived >::factory_id()
    {
        return Internal::creator< Derived >::type_ident();
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Derived >
    inline void context< Derived >::unsafe_reset()
    {
        Internal::context_base::reset_distributables();
    }

    template< class Derived >
    inline void context< Derived >::unsafe_reset( bool )
    {
        CNC_ABORT( "context::unsafe_reset( bool ) should never get called." )
    }

} // namespace CnC

#endif // _CnC_CONTEXT_H_
