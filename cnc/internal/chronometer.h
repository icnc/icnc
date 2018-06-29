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

#ifndef TIMER_HH_ALREADY_INCLUDED
#define TIMER_HH_ALREADY_INCLUDED

#if defined( _WIN32 ) || defined( _WIN64 )
typedef unsigned __int64 uint64_t;
# include <windows.h>
# ifdef _M_X64
#  include <intrin.h>
#  pragma intrinsic( __rdtsc )
# endif  //_M_X64
#else
#define __STDC_LIMIT_MACROS
# include <stdint.h>
#endif // _WIN32||_WIN64

#include <cnc/internal/tbbcompat.h>
#include <tbb/atomic.h>
#include <tbb/tick_count.h>
#include <cnc/internal/schedulable.h>
#include <cnc/internal/cnc_api.h>
#include <cnc/internal/scalable_object.h>
#include <cnc/internal/scalable_vector.h>
#include <iostream>

#ifndef UINT64_MAX
# define UINT64_MAX 0xffffffffffffffffULL
#endif

namespace CnC {
    namespace Internal {

        class step_launcher_i;
        class step_instance_base;
        static inline uint64_t cnc_rdtsc();

        const char CnC_incomplete = 3;

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// Take low-overhead and low-level, fine-grained timings per step-instance.
        /// We record times spent in a step-instance during normal execution and
        /// also record the times spend in getting and putting items.
        /// The information per instance is stored in TLS, no sync with other threads is needed.
        /// When save_log is called, the logs of each thread are combined and saved.
        class CNC_API chronometer : public scalable_object
        {
        public:
            class get_timer : public scalable_object
            {
            public:
                inline get_timer( step_instance_base * _si );
                inline ~get_timer();
            private:
                step_instance_base * m_stepInstance;
            };

            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            class put_timer : public scalable_object
            {
            public:
                inline put_timer( step_instance_base * _si );
                inline ~put_timer();
            private:
                step_instance_base * m_stepInstance;
            };

            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            class step_timer : public scalable_object
            {
            public:
                inline void start();
                inline void start_get();
                inline void stop_get() ;
                inline void start_put() ;
                inline void stop_put();
                inline void log( const char * name, const int id, const StepReturnValue_t rt ) const;
            private:
                uint64_t                    m_startCycle;
                uint64_t                    m_tmpStartCycle;
                uint64_t                    m_getCycles;
                uint64_t                    m_putCycles;
                tbb::tick_count             m_startTick;
                tbb::tick_count             m_tmpStartTick;
                tbb::tick_count::interval_t m_getTicks;
                tbb::tick_count::interval_t m_putTicks;
            };

            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            struct Record_t
            {
                const char *        m_name;
                uint64_t            m_startCycle;
                uint64_t            m_cycleCount;
                uint64_t            m_getCycles;
                uint64_t            m_putCycles;
                double              m_seconds;
                double              m_getSeconds;
                double              m_putSeconds;
                int                 m_stepId;
                StepReturnValue_t   m_type;

                Record_t( const char * name = NULL, const int id = -1,
                          const uint64_t sc = 0, const uint64_t cc = 0, const uint64_t gcc = 0, const uint64_t pcc = 0,
                          const double sec = 0.0, const double gsec = 0.0, const double psec = 0.0,
                          const StepReturnValue_t rt = CnC_incomplete );
            };

            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            chronometer();
            static void init( bool RDTSC_only );

            static void add_record( const char * name, const int id,
                                    const uint64_t sc, const uint64_t cc, const uint64_t gcc, const uint64_t pcc,
                                    const double sec, const double gsec, const double psec,
                                    const StepReturnValue_t rt );
            static void save_log( const char * filename );
            void dump_log( std::ostream & );

        private:
            void format_record( std::ostream &, Record_t &r ) const;

            scalable_vector< Record_t > m_log;
            unsigned long           m_threadId;
            int                     m_curr;
            static bool             s_useTBB;
            friend class step_timer;
        };

    } //    namespace Internal
} // namspace CnC

#include <cnc/internal/step_instance_base.h>

namespace CnC {
    namespace Internal {

        class step_instance_base;

#define TSC_DIFF( _s, _e ) ( ( (_e) > (_s) ) ? ( (_e) - (_s) ) : ( (_e) + ( UINT64_MAX - (_s) ) ) )

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline chronometer::get_timer::get_timer( step_instance_base * _si )
            : m_stepInstance( _si )
        {
            if( _si ) _si->start_get();
        }

        inline chronometer::get_timer::~get_timer()
        {
            if( m_stepInstance ) m_stepInstance->stop_get();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline chronometer::put_timer::put_timer( step_instance_base * _si )
            : m_stepInstance( _si )
        {
            if( _si ) _si->start_put();
        }

        inline chronometer::put_timer::~put_timer()
        {
            if( m_stepInstance ) m_stepInstance->stop_put();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        inline void chronometer::step_timer::start()
        {
            m_startCycle = cnc_rdtsc();
            if( chronometer::s_useTBB ) m_startTick = tbb::tick_count::now();
            m_tmpStartCycle = 0;
            m_getCycles = 0;
            m_putCycles = 0;
//            m_tmpStartTick = 0;
            m_getTicks = tbb::tick_count::interval_t();
            m_putTicks = tbb::tick_count::interval_t();
        }

        inline void chronometer::step_timer::start_get()
        { 
            m_tmpStartCycle = cnc_rdtsc();
            if( chronometer::s_useTBB ) m_tmpStartTick = tbb::tick_count::now(); 
        }

        inline void chronometer::step_timer::stop_get() 
        {
            uint64_t _tmp = cnc_rdtsc();
            m_getCycles += TSC_DIFF( m_tmpStartCycle, _tmp );
            if( chronometer::s_useTBB ) m_getTicks += tbb::tick_count::now() - m_tmpStartTick;
        }

        inline void chronometer::step_timer::start_put() 
        { 
            m_tmpStartCycle = cnc_rdtsc();
            if( chronometer::s_useTBB ) m_tmpStartTick = tbb::tick_count::now(); 
        }

        inline void chronometer::step_timer::stop_put() 
        { 
            uint64_t _tmp = cnc_rdtsc();
            m_putCycles += TSC_DIFF( m_tmpStartCycle, _tmp );
            if( chronometer::s_useTBB ) m_putTicks += tbb::tick_count::now() - m_tmpStartTick;
        }

        inline void chronometer::step_timer::log( const char * name, const int id, const StepReturnValue_t rt ) const
        {
            uint64_t _tmp = cnc_rdtsc();
            tbb::tick_count _now;
            if( chronometer::s_useTBB ) _now = tbb::tick_count::now();
            chronometer::add_record( name,
                                     id,
                                     m_startCycle,
                                     TSC_DIFF( m_startCycle, _tmp ),
                                     m_getCycles,
                                     m_putCycles,
                                     chronometer::s_useTBB ? (_now - m_startTick).seconds() : 0,
                                     chronometer::s_useTBB ? m_getTicks.seconds() : 0,
                                     chronometer::s_useTBB ? m_putTicks.seconds() : 0,
                                     rt );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static inline uint64_t cnc_rdtsc()
        {
            // cpuid serializes pipeline.
#if defined( _WIN32 ) || defined( _WIN64 )
            uint64_t temp;
#ifdef _M_X64
            temp = __rdtsc();
#else
            _asm  cpuid
            _asm  rdtsc
            _asm  lea ecx, temp
            _asm  mov [ecx], eax
            _asm  mov [ecx+4], edx
#endif //_M_X64
            return temp; 
#else // LIN
            uint32_t a, d;
            __asm__ __volatile__ ("xorl %%eax,%%eax\n"
                                  "cpuid"      // serialize
                                  ::: "%rax", "%rbx", "%rcx", "%rdx");
            // 64 bit from 2 x 32 bit 
            __asm__ __volatile__ ("rdtsc" : "=a" (a), "=d" (d));
            return (uint64_t)d << 32 | a;
#endif // _WIN32||_WIN64
        }

    } //    namespace Internal
} // namspace CnC

#endif // TIMER_HH_ALREADY_INCLUDED
