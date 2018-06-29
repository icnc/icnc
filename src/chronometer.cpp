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
  see chronometer.h
*/

#define _CRT_SECURE_NO_DEPRECATE

#include <fstream>

#include <cnc/internal/chronometer.h>
#include <cnc/internal/tls.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/concurrent_queue.h>

namespace CnC {
    namespace Internal {

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        class cr_init
        {
        public:
            cr_init()
                : m_chrons(),
                  m_tls()
            {
            }
            ~cr_init()
            {
                chronometer * _tmp;
                while( m_chrons.try_pop( _tmp ) ) {
                    delete _tmp;
                }
            }
            void reg( chronometer * c )
            {
                m_chrons.push( c );
            }
            void dump_log( std::ostream & os )
            {
                for( tbb::concurrent_queue< chronometer * >::iterator i = m_chrons.unsafe_begin(); i != m_chrons.unsafe_end(); ++ i ) {
                    (*i)->dump_log( os );
                }
            }
            chronometer * get_chronometer() const
            {
                return m_tls.get();
            }
            void set_chronometer( chronometer * c ) const
            {
                m_tls.set( c );
            }
        private:
            tbb::concurrent_queue< chronometer * > m_chrons;
            TLS_static< chronometer * > m_tls;
        };

        static cr_init s_cinit;

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        unsigned long GetThreadId();

        unsigned long GetThreadId()
        {
      // RRN: Well, this seems to be against the rules.  We're
      // assuming pthread_t is something that can reasonably be
      // cast to an unsigned long.
#if defined( _WIN32 ) || defined( _WIN64 )
            return static_cast< unsigned long >( GetCurrentThreadId() );
#else
            // RRN: GCC will not allow a static_cast here.  It's not safe enough to eschew dynamic checking.
            //return static_cast< unsigned long >( pthread_self() );
            return (unsigned long)( pthread_self() );
        // Greg suggested this:
        //            return reinterpret_cast< unsigned long >( pthread_self() );
#endif // _WIN32||_WIN64
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool chronometer::s_useTBB = false;

        void chronometer::init( bool RDTSC_only )
        {
            s_useTBB = !RDTSC_only; 
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        chronometer::chronometer()
            : m_log( 2048 ), m_threadId( GetThreadId() ), m_curr( 0 )
        {
            s_cinit.reg( this );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void chronometer::add_record( const char * name,
                                      const int id,
                                      const uint64_t sc,
                                      const uint64_t cc,
                                      const uint64_t gcc,
                                      const uint64_t pcc,
                                      const double sec,
                                      const double gsec,
                                      const double psec,
                                      const StepReturnValue_t rt )
        {
            chronometer * _c = s_cinit.get_chronometer();
            if( _c == NULL ) {
                _c = new chronometer();
                s_cinit.set_chronometer( _c );
            }
            if( static_cast< unsigned int >( _c->m_curr ) >= _c->m_log.size() ) _c->m_log.resize( 2 * _c->m_curr );
            Record_t & _r = _c->m_log[_c->m_curr++];
            _r.m_name = name;
            _r.m_startCycle = sc;
            _r.m_cycleCount = cc;
            _r.m_getCycles = gcc;
            _r.m_putCycles = pcc;
            _r.m_seconds = sec;
            _r.m_getSeconds = gsec;
            _r.m_putSeconds = psec;
            _r.m_stepId = id;
            _r.m_type = rt;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void chronometer::format_record( std::ostream & os, Record_t & r ) const
        {
            static char const * ExecutionStatusNames[] = { "completed", "requeued", "failed", "error" };
            os << "cycle\t" << r.m_startCycle
               << "\tthread\t" << m_threadId
               << "\tstep\t" << r.m_name
               << "\tid\t" << r.m_stepId
               << "\tstatus\t" << ExecutionStatusNames[static_cast< unsigned int >( r.m_type )]
               //<< "\t" << r.si->timing_name()
               //<< "\t" << r.si->tag()
               << "\tcycles\t" << r.m_cycleCount
               << "\tget-cycles\t" << r.m_getCycles
               << "\tput-cycles\t" << r.m_putCycles;
               if( s_useTBB ) {
                   os << "\ttime[ms]\t" << ( r.m_seconds * 1000.0 )
                      << "\tget-time[ms]\t" << ( r.m_getSeconds * 1000.0 )
                      << "\tput-time[ms]\t" << ( r.m_putSeconds * 1000.0 );
#ifndef __MIC__ 
                   // assembler complaints on MIC, no idea why
                   os << "\timplied-GHZ\t" << ( r.m_cycleCount / r.m_seconds / 1.0e9 );
#endif
               }
               os << std::endl;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void chronometer::dump_log( std::ostream & os )
        {
            for( int i = 0; i < m_curr; ++i ) {
                format_record( os, m_log[i] );
            }
            m_log.clear();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void chronometer::save_log( const char * filename_ )
        {
            std::string filename(filename_);
            if( filename == "-" ) {
                s_cinit.dump_log( std::cout );
            } else {
                std::ofstream tfile( filename.c_str() );
                if (!tfile) 
                {
                    std::cerr << " Timer cannot open " << filename << std::endl;
                    exit(-1);
                }

                s_cinit.dump_log( tfile );

                tfile.close();
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        chronometer::Record_t::Record_t( const char * name,
                                         const int id,
                                         const uint64_t sc,
                                         const uint64_t cc,
                                         const uint64_t gcc,
                                         const uint64_t pcc,
                                         const double sec,
                                         const double gsec,
                                         const double psec,
                                         const StepReturnValue_t rt )
        : m_name( name ),
          m_startCycle( sc ),
          m_cycleCount( cc ),
          m_getCycles( gcc ),
          m_putCycles( pcc ),
          m_seconds( sec ),
          m_getSeconds( gsec ),
          m_putSeconds( psec ),
          m_stepId( id ),
          m_type( rt )
        {
        }

    } //    namespace Internal
} // namespace CnC
