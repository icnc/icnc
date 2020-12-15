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

#ifndef _CnC_PINNING_H_
#define _CnC_PINNING_H_

#ifdef __APPLE__
// Not supported on Mac
namespace CnC {
  namespace Internal {

    static void pin_thread( int tid, int cid, int htstride )
    {}

    class pinning_observer
    {
    public:
      pinning_observer( int hts ) {}
    };
  }
}

#else // __APPLE__

#ifndef WIN32
#include <cstring>
#include <pthread.h>
#include <sched.h>
#endif
#include <cnc/internal/tbbcompat.h>
#include <tbb/task_scheduler_observer.h>
#include <atomic>

namespace CnC {
	namespace Internal {

		inline static void pin_thread( int tid, int cid, int htstride );
		
        /// pins TBB thraeds to cores
        static std::atomic< int > s_id;
        class pinning_observer: public tbb::task_scheduler_observer
        {
        public:
            pinning_observer( int hts )
				: m_hts( hts )
            {
                observe(true); // activate the observer
            }
            /*override*/ void on_scheduler_entry( bool worker ) {
                int _i = s_id.fetch_add(1);
                pin_thread( _i, -1, m_hts );
                // set_thread_affinity( tbb::task_arena::current_slot(), m_mask ); 
            }
            /*override*/ bool on_scheduler_leaving( ) {
                // Return false to trap the thread in the arena
                return /*bool*/ false; //!is_more_work_available_soon();
            }
            /*override*/ void on_scheduler_exit( bool worker ) { }
		private:
			const int m_hts;
        };

// return position in bit-set for a given number of cores and thread-stride
#define _T2C( _x, _np, _hs ) (((_x)*((_np)+1)/((_np)/(_hs)))%(_np))

#ifdef _WIN32

		namespace {
			template< typename T >
			T find_nth_bit( T x, int n ) {
				int f = 0;
				for( int i=0; i< sizeof( T ); ++ i ) {
					if( (x >> i) & 1 ) {
						if( f == n ) return (1 << i);
						++f;
					}
				}
				return 0;
			}
		}

		/// pin current thread (with id tid) to processor with id cid
		/// if cid < 0, determine cid with strided round-robin distribution
		inline static void pin_thread( int tid, int cid, int htstride )
		{
			DWORD64 _procmask = 0;
			const char * _msg = NULL;
			SYSTEM_INFO sysinfo;
			GetSystemInfo( &sysinfo );
			if( cid < 0 ) {
#ifdef _WIN64
				DWORD64 _dwProcessAffinity, _dwSystemAffinity;
#else
				DWORD _dwProcessAffinity, _dwSystemAffinity;
#endif
				if( GetProcessAffinityMask( GetCurrentProcess(), &_dwProcessAffinity, &_dwSystemAffinity ) != 0 ) {
                    DWORD64 _np = __popcnt( _dwProcessAffinity );
					if( _np > 0 ) {
						CNC_ASSERT_MSG( _np > 0, "No available processors found to pin thread to" );
						int _n = _T2C( tid, _np, htstride );
						_procmask = find_nth_bit( _dwProcessAffinity, _n );
						CNC_ASSERT( __popcnt( _procmask ) == 1 );
					} else _msg = "Warning: GetProcessAffinityMask returned empty processor mask\n";
				} else _msg = "Warning: GetProcessAffinityMask failed\n";
			} else {
				_procmask = (1 << cid);
			}
			if( _msg == NULL ) {
				if( SetThreadAffinityMask( GetCurrentThread(), _procmask) != 0 ) {
					unsigned long _tmp;
					_BitScanForward( &_tmp, _procmask );
#ifndef NDEBUG
                    Speaker oss( std::cerr );
					oss << "Bind thread " << tid << " to processor " << _tmp;
#endif
					return;
				} else {
					_msg = "Warning: SetThreadAffinityMask failed";
				}
			}
			if( _msg ) {
                Speaker oss( std::cerr );
                oss << _msg;
			}
		}

#else // -> LINUX

#define CNC_MX_CPU 256

        // grr older, but not even too oldeLinux don't define this...
#ifndef CPU_COUNT
        inline static int cpu_count( cpu_set_t * s )
        {
            int _c = 0;
            for( int i = 0; i < CNC_MX_CPU; ++i ) {
                if( CPU_ISSET( i, s ) ) ++_c;
            }
            return _c;
        }
# define CPU_COUNT( _s ) cpu_count( _s )
#endif

		/// pin current thread (with id tid) to processor with id cid
		/// if cid < 0, determine cid with strided round-robin distribution
		inline static void pin_thread( int tid, int cid, int htstride )
		{
            cpu_set_t _procmask; CPU_ZERO( &_procmask );
			const char * _msg = NULL;

			if( cid < 0 ) {
                cpu_set_t _dwProcessAffinity; CPU_ZERO( &_dwProcessAffinity );
                if( sched_getaffinity( 0, sizeof( _dwProcessAffinity ), &_dwProcessAffinity ) == 0 ) {
                    int _np = CPU_COUNT( &_dwProcessAffinity );
                    if( _np > 0 ) {
                        CNC_ASSERT_MSG( _np > 0, "No available processors found to pin thread to" );
                        //#if __MIC__ || __MIC2__

                        int _n = _T2C( tid, _np, htstride );
                        int _tmp = 0;
						while( _n >= 0 && _tmp <= CNC_MX_CPU ) { //sizeof( _dwProcessAffinity ) ) {
                            if( CPU_ISSET( _tmp, &_dwProcessAffinity ) ) --_n;
                            ++_tmp;
                        }
                        --_tmp;
                        CPU_SET( _tmp, &_procmask );
						CNC_ASSERT( CPU_COUNT( &_procmask ) == 1 );
                    } else _msg = "Warning: sched_getaffinity returned empty processor mask\n";
                } else _msg = "Warning: sched_getaffinity failed";
			} else {
				CPU_SET( cid, &_procmask );
			}
			if( _msg == NULL ) {
                if( pthread_setaffinity_np( pthread_self(), sizeof( _procmask ), &_procmask ) == 0 ) {
                    int _tmp = 0;
                    while( ! CPU_ISSET( _tmp, &_procmask ) ) { ++ _tmp; }
                    Speaker oss( std::cerr );
                    oss << "Bind thread " << tid << " to processor " << _tmp;
				} else {
					_msg = "Warning: pthread_setaffinity_np failed\n";
				}
            }
			if( _msg ) {
                Speaker oss( std::cerr );
				oss << _msg;
			}
        }

#endif // WIN32

	} //namespace internal
} // namespace CnC

#endif // __APPLE__
#endif // _CnC_PINNING_H_
