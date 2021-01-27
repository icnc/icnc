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

/*
  CnC debug interface.
*/

#ifndef _CnC_DEBUG_H_
#define _CnC_DEBUG_H_

#include <cnc/internal/cnc_api.h>
#include <cnc/internal/chronometer.h>
#include <cnc/internal/step_launcher.h>

namespace CnC {

    class graph;
    template< class T > class context;
    template< typename Tag, typename Tuner > class tag_collection;
    template< typename Tag, typename Item, typename Tuner > class item_collection;
    template< typename UserStep, typename Tuner > class step_collection;
    

    /// \brief Debugging interface providing tracing and timing capabilities
    ///
    /// \#include <cnc/debug.h>
    ///
    /// For a meaningful tracing the runtime requires a function cnc_format for
    /// every non-standard item and tag type. The expected signature is:
    /// \code std::ostream & cnc_format( std::ostream & os, const your_type & p ) \endcode
    /// Make sure it is defined in the corresponding namespace.
    struct debug {
        /// \brief sets the number of threads used by the application
        ///
        /// Overwrites environment variable CNC_NUM_THREADS.
        /// To be effective, it must be called prior to context creation.
        static void CNC_API set_num_threads( int n );

        /// \brief enable tracing of a given tag collection at a given level
        ///
        /// To be used in a safe environment only (no steps in flight)
        /// \param tc    the tag collection to be traced
        /// \param level trace level
        template< typename Tag, typename Tuner >
        static void trace( tag_collection< Tag, Tuner > & tc, int level = 1 )
        { tc.m_tagCollection.set_tracing( level ); }

        /// \brief enable tracing of a given item collection at a given level
        ///
        /// To be used in a safe environment only (no steps in flight).
        /// \param ic    the item collection to be traced
        /// \param level trace level
        template< typename Tag, typename Item, typename HC >
        static void trace( item_collection< Tag, Item, HC > & ic, int level = 1 )
        { ic.m_itemCollection.set_tracing( level ); }

        /// \brief enable tracing of a given step-collection at a given level (off=0)
        ///
        /// To be used in a safe environment only (no steps in flight)
        /// \param sc  the step-collection to be traced
        /// \param level trace level
        template< typename UserStep, typename Tuner >
        static void trace( step_collection< UserStep, Tuner > & sc, int level = 1 )
        {
            sc.set_tracing( level );
            CNC_ASSERT( sc.trace_level() == level );
        }
        
        /// \brief enable tracing of graph internals (not including its I/O collections)
        ///
        /// To be used in a safe environment only (no steps in flight)
        /// names of collections are unavailable unless tracing them was enabled explicitly.
        /// \param g     the graph to be traced
        /// \param level trace level
        static void CNC_API trace( ::CnC::graph & g, int level = 1 );

        /// \brief enable tracing of everything in given context (off=0)
        ///
        /// To be used in a safe environment only (no steps in flight)
        /// \param c     the context to be traced
        /// \param level trace level
        template< class Derived >
        static void trace_all( ::CnC::context< Derived > & c, int level = 1 )
        { c.set_tracing( level ); }

        /// \brief initalize timer
        /// \param cycle if true, use cycle counter only
        /// Cycle counters might overflow: TSC results are incorrect if the measured time-interval
        /// is larger than a full turn-around.
        static void init_timer( bool cycle = false )
        { Internal::chronometer::init( cycle ); }

        /// \brief save collected time log to a specified file
        /// \param name  the file to write the time log, pass "-" for printing to stdout
        static void finalize_timer( const char * name )
        { Internal::chronometer::save_log( name ? name : "-" ); }

        /// \brief enable timing of a given step
        ///
        /// To be used in a safe environment only (no steps in flight)
        /// \param sc  the step-collection to be timed
        template< typename UserStep, typename Tuner >
        static void time( step_collection< UserStep, Tuner > & sc )
        { sc.set_timing(); }

        /// \brief enable collection scheduler statistics per context
        ///
        /// Statistics will be print upon destructino of a context.
        /// \param c     the context to be examined
        template< class Derived >
        static void collect_scheduler_statistics( ::CnC::context< Derived > & c )
        { c.init_scheduler_statistics(); }
    };

} // namespace cnc

#endif // _CnC_DEBUG_H_
