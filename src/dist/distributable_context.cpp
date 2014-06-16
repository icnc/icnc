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
  see distributable_context.h
*/

#include <cnc/serializer.h>
#include <cnc/internal/dist/distributable_context.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/dist/distributable.h>
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/statistics.h>
#include <tbb/aligned_space.h>
#include <cnc/internal/tbbcompat.h>

namespace CnC {
    namespace Internal {

        namespace {
            static const char READY   = 1;
            static const char DATA    = 2;
            static const char CLEANUP = 3;

        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        distributable_context::distributable_context( const std::string & name )
            : creatable(), distributable( name ),
              m_mutex(),
              m_statistics( NULL ),
              m_distributables(),
              //              m_wasDistributed( false ),
              m_distributionEnabled( false ),
              m_distributionReady( false )
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        distributable_context::~distributable_context()
        {
            delete m_statistics;
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::init_dist_ready() const
        {
            // nothing to be done
            // we need to send the data, but that needs to be lazy
            // because we have no other reliable hook
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        void distributable_context::fini_dist_ready()
        {
            CNC_ASSERT( ! dist_ready() );
            if( ! distributor::distributed_env() ) {
                if( distributor::myPid() == 0 ) {
                    int n = distributor::numProcs();
                    int _tmp;
                    for( int i = 1; i < n; ++i ) m_barrier.pop( _tmp );
                    m_distributionReady = true;
                } else {
                    m_distributionReady = true;
                    serializer * _ser = new_serializer( this );
                    (*_ser) & READY;
                    send_msg( _ser, 0 );
                }
            } else m_distributionReady = true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        
        void distributable_context::recv_msg( serializer * serlzr )
        {
            int _did;
            (*serlzr) & _did;
            if( _did >= 0 ) {
                distributable * _dist = NULL;
                mutex_type::scoped_lock _lock( m_mutex );
                CNC_ASSERT( _did >= 0 && _did < (int)m_distributables.size() );
                _dist = m_distributables[_did];
                _lock.release();
                if( _dist ) _dist->recv_msg( serlzr );
            } else { // own message handling
                char _msg;
                (*serlzr) & _msg;
                switch( _msg ) {
                case CLEANUP :
                    spawn_cleanup();
                    break;
                case READY :
                    CNC_ASSERT( distributor::myPid() == 0 );
                    m_barrier.push(1);
                    break;
                default:
                    CNC_ASSERT_MSG( false, "Context received unexpected message tag" );
                }
            }
            LOG_STATS( stats(), msg_recvd() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        serializer * distributable_context::new_serializer( const distributable * distbl ) const
        {
            // an undocumented feature suggested by Arch to make the mutex-init therad-safe
            static tbb::aligned_space< mutex_type, 1 > s_ser_mutex;
            {
                mutex_type::scoped_lock _lock( *s_ser_mutex.begin() );
                if( ! subscribed() ) {
                    CNC_ASSERT( distributor::myPid() == 0 );
                    distributor::distribute( const_cast< distributable_context * >( this ) );
                }
                CNC_ASSERT( dist_ready() );
            }
            serializer * _serlzr = distributor::new_serializer( this );
            int _did = distbl != this ? distbl->gid() : -1;
            CNC_ASSERT( _did < (int)m_distributables.size() );
            (*_serlzr) & _did;
            return _serlzr;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::send_msg( serializer * serlzr, int rcver ) const
        {
            distributor::send_msg( serlzr, rcver );
            LOG_STATS( stats(), msg_sent() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        bool distributable_context::bcast_msg( serializer * serlzr, const int * rv, int nrvs ) const
        {
            return distributor::bcast_msg( serlzr, rv, nrvs );
            LOG_STATS( stats(), msg_sent( nrvs ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::bcast_msg( serializer * serlzr ) const
        {
            distributor::bcast_msg( serlzr );
            LOG_STATS( stats(), bcast_sent() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int distributable_context::subscribe( distributable * distbl )
        {
            // we might have unsubscribed before, let's fill empty slots first
            int _gid = 0;
            mutex_type::scoped_lock _lock( m_mutex );
            for( distributable_container::iterator i = m_distributables.begin(); i != m_distributables.end(); ++i, ++_gid ) {
                if( *i == NULL ) {
                    *i = distbl;
                    distbl->set_gid( _gid );
                    return _gid;
                }
            }
            m_distributables.push_back( distbl );
            _gid = m_distributables.size()-1;
            distbl->set_gid( _gid );
            return _gid;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::unsubscribe( distributable * distbl )
        {
            mutex_type::scoped_lock _lock( m_mutex );
            // FIXME unsubscription must be synchronized with remote clones
            //       we might want to move to a hash_map
            for( distributable_container::iterator i = m_distributables.begin(); i != m_distributables.end(); ++i ) {
                if( (*i) == distbl ) {
                    (*i) = NULL;
                    return;
                }
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::set_tracing( int level )
        {
            traceable::set_tracing( level);
            mutex_type::scoped_lock _lock( m_mutex );
            for( distributable_container::iterator i = m_distributables.begin(); i != m_distributables.end(); ++i ) {
                if( *i != NULL ) {
                    (*i)->set_tracing( level );
                }
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::serialize( serializer & )
        {
            // nothing to be done, user might overload in his/her context
        }
            
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::reset_distributables()
        {
            bool _dist = distributor::numProcs() > 1 && subscribed();
            mutex_type::scoped_lock _lock( m_mutex );
            for( distributable_container::iterator i = m_distributables.begin(); i != m_distributables.end(); ++i ) {
                if( *i ) (*i)->unsafe_reset( _dist );
            }
        }
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::cleanup_distributables( bool bcast )
        {
            if( bcast && distributor::numProcs() > 1 && subscribed() ) {
                serializer * _ser = new_serializer( this );
                (*_ser) & CLEANUP;
                bcast_msg( _ser );
            }
            bool done = false;
            int i = 0;
            do {
                mutex_type::scoped_lock _lock( m_mutex );
                if( static_cast< unsigned int >( i ) < m_distributables.size() ) {
                    distributable * _dstbl = m_distributables[i];
                    done = ( static_cast< unsigned int >( ++i ) >= m_distributables.size() );
                    _lock.release();
                    if( _dstbl ) _dstbl->cleanup();
                } else done = true;
            } while( ! done );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::init_scheduler_statistics() 
        {
            if( m_statistics == NULL ) {
                m_statistics = new statistics;
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributable_context::print_scheduler_statistics()
        {
            LOG_STATS( m_statistics, print_statistics( std::cout ) );
        }

        
    } // namespace Internal
} // namespace CnC
