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
  see distributor.h
*/

#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/dist/distributable_context.h>
#include <cnc/internal/dist/communicator.h>
#include <cnc/internal/dist/factory.h>
#include <cnc/serializer.h>

#include <iostream>

namespace CnC {
    namespace Internal {

        distributor * distributor::theDistributor = NULL;
        communicator * distributor::m_communicator = NULL;

        distributor::distributor()
            : m_distContexts(),
              m_nextGId(),
              m_state( distributor::DIST_OFF ),
              m_sync(),
              m_flushCount( 0 ),
              m_nMsgsRecvd(),
              m_distEnv( false )
        {
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::init( communicator_loader_type loader, bool dist_env )
        {
            theDistributor = new distributor();
            theDistributor->m_state = DIST_INITING;
            theDistributor->m_distEnv = dist_env;
            loader( *theDistributor, dist_env );
            CNC_ASSERT( theDistributor->m_communicator );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        void distributor::fini()
        {
            delete theDistributor;
            theDistributor = NULL;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// start distributed system
        void distributor::start( long flag )
        {
            CNC_ASSERT( m_communicator );
            theDistributor->m_nextGId = 0;
            theDistributor->m_nMsgsRecvd = 0;
            theDistributor->m_state = DIST_ON;
            theDistributor->m_communicator->init( 0, flag );
        } // FIXME more than one communicator

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// stop distributed system
        void distributor::stop()
        {
            if( active() && theDistributor->m_communicator ) {
                theDistributor->m_communicator->fini();
                theDistributor->m_communicator = NULL;
            }
            theDistributor->m_state = DIST_SUSPENDED;
        }   // FIXME more than one communicator

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static const char DIS_CTXT = 0;
        static const char UN_CTXT  = 1;
        static const char PING = 3;
        static const char PONG = 4;

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        int distributor::distribute( distributable_context * dctxt )
        {
            if( !active() || theDistributor->m_communicator == NULL || !dctxt->distributed() || ( remote() && !theDistributor->distributed_env() ) ) {
                // in a hierarchical network, we might have to do something here
                return dctxt->gid();
            }
            int _gid = theDistributor->m_nextGId++;
            my_map::accessor _accr;
            theDistributor->m_distContexts[0].insert( _accr, _gid );
            _accr->second = dctxt;
            dctxt->set_gid( _gid );
            _accr.release();
            if( theDistributor->distributed_env() ) {
                dctxt->fini_dist_ready();
                return _gid;
            }
            // the host needs to actually send context to remote processes
            serializer * _serlzr = new_serializer( NULL );
            int _tid = dctxt->factory_id();
            (*_serlzr) & DIS_CTXT & _tid & _gid & (*dctxt);
            bcast_msg( _serlzr );

            dctxt->fini_dist_ready();

            return _gid;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// mark given context as deleted
        /// currently limited to be called on the host only
        /// clones on other processes will be deleted
        void distributor::undistribute( distributable_context * dctxt )
        {
            if( !active() || theDistributor->m_communicator == NULL || !dctxt->distributed() || remote() || theDistributor->distributed_env() ) {
                // in a hierarchical network, we might have to do something here
                return;
            }

            // the host needs to actually inform remote processes
            serializer * _serlzr = new_serializer( NULL );
            int _tid = dctxt->factory_id();
            int _gid = dctxt->gid();
            (*_serlzr) & UN_CTXT & _tid & _gid;
            bcast_msg( _serlzr );

            theDistributor->m_distContexts[0].erase( dctxt->gid() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// distributable_contexts send their messages through the global distributor
        void distributor::send_msg( serializer * serlzr, int rcver )
        {
            BufferAccess::finalizePack( *serlzr );
            // FIXME support for more than one communicator
            theDistributor->m_communicator->send_msg( serlzr, rcver );
            //            ++theDistributor->m_nMsgsRecvd;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// distributable_contexts send their messages through the global distributor
        void distributor::bcast_msg( serializer * serlzr )
        {
            BufferAccess::finalizePack( *serlzr );
            // FIXME support for more than one communicator
            theDistributor->m_communicator->bcast_msg( serlzr );
            //            ++theDistributor->m_nMsgsRecvd;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// distributable_contexts send their messages through the global distributor
        bool distributor::bcast_msg( serializer * serlzr, const int * rcvers, int nrecvrs )
        {
            BufferAccess::finalizePack( *serlzr );
            // FIXME support for more than one communicator
            bool _res = theDistributor->m_communicator->bcast_msg( serlzr, rcvers, nrecvrs );
            //            ++theDistributor->m_nMsgsRecvd;
            return _res;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// any communicator calls this for incoming messages
        void distributor::recv_msg( serializer * serlzr , int pid )
        {
            BufferAccess::initUnpack( *serlzr );
            int _dctxtId;
            (*serlzr) & _dctxtId;
            if( _dctxtId == SELF ) {
                char _action;
                (*serlzr) & _action;
                switch( _action )
                {
                    case PING : { // now send pong to all other clients but not the host
                        CNC_ASSERT( myPid() ); // must not be root!
                        serializer * _serlzr = new_serializer( NULL );
                        // int _pid = myPid();
                        (*_serlzr) & PONG; // & _pid;
                        int _n = numProcs();
                        if( _n == 2 ) {
                            // let's trick the case-branch: no break for this if-branch
                            theDistributor->m_flushCount = 1;
                        } else {
                            scalable_vector< int > _rcvers;
                            _rcvers.reserve( _n - 2 );
                            for( int i = 1; i < _n; ++i ) { // not to the host yet
                                if( i != myPid() ) {
                                    _rcvers.push_back( i );
                                }
                            }
                            // wait until send-queue is empty
                            if( has_pending_messages() ) {++theDistributor->m_nMsgsRecvd;}
                            // now issue send
                            bcast_msg( _serlzr, &_rcvers.front(), _rcvers.size() );
                            // we expect pong from all other clients; not the host
                            if( theDistributor->m_flushCount == 2 - _n ) {
                                // we had all the pongs before the ping!
                                // let's trick the case-branch: no break for this if-branch
                                theDistributor->m_flushCount = 1;
                            } else {
                                theDistributor->m_flushCount += _n - 2;
                                break;
                            }
                        }
                    }
                    case PONG : {
                        if( myPid() ) {  // reduce expected pong count; if hits 0, send pong back to host
                            if( --theDistributor->m_flushCount == 0 ) {
                                serializer * _serlzr = new_serializer( NULL );
                                // wait until send-queue is empty
                                if( has_pending_messages() ) {++theDistributor->m_nMsgsRecvd;}
                                int _nMsgs = theDistributor->m_nMsgsRecvd.fetch_and_store( 0 );
                                // int _pid = myPid();
                                (*_serlzr) & PONG & _nMsgs; // & _pid;
                                send_msg( _serlzr, 0 );
                            }
                        } else { // push 1 pong on queue
                            int _nMsgs;
                            (*serlzr) & _nMsgs;
                            theDistributor->m_nMsgsRecvd += _nMsgs; // not as clean as it could be, but efficient
                            theDistributor->m_sync.push( 0 );
                        }
                        break;
                    }
                    default : {
                        ++theDistributor->m_nMsgsRecvd;
                        int _typeId;
                        (*serlzr) & _typeId & _dctxtId;
                        // message for distributor? Distribute or undistribute contexts
                        my_map::accessor _accr;
                        bool _inserted = theDistributor->m_distContexts[pid].insert( _accr, _dctxtId );
                        // if the context is allready there, remove it.
                        if( ! _inserted ) {
                            CNC_ASSERT( _action == UN_CTXT );
                            distributable_context * _dctxt = _accr->second;
                            CNC_ASSERT( _accr->second != NULL );
                            delete _dctxt; // on the host, the user is deleting the context
                            theDistributor->m_distContexts[pid].erase( _accr );
                            return;
                        }
                        CNC_ASSERT( _action == DIS_CTXT );
                        creatable * _crtbl = factory::create( _typeId );
                        CNC_ASSERT( dynamic_cast< distributable_context * >( _crtbl ) );
                        distributable_context * _dctxt = static_cast< distributable_context * >( _crtbl );
                        _dctxt->set_gid( _dctxtId );
                        _accr->second = _dctxt;
                        (*serlzr) & (*_dctxt);
                        _dctxt->fini_dist_ready();
                    }
                }
            } else {
                ++theDistributor->m_nMsgsRecvd;
                my_map::const_accessor _accr;
                bool _inTable = theDistributor->m_distContexts[pid].find( _accr, _dctxtId );
                CNC_ASSERT_MSG( _inTable, "Received message for not (yet) existing context\n" );
                distributable_context * _dctxt = _accr->second;
                _accr.release();
                _dctxt->recv_msg( serlzr );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// currently supported only when called on Host
        /// send ping to all clients and wait for n-1 pongs to arrive.
        /// When receiving a ping, client sends pong to all other clients (n-2).
        /// When client received last pong (n-2) it sends its pong to host.
        int distributor::flush()
        {
            // send all clients ping and expect a pong back
            // upon receiving ping, clients sends pong to all processes
            serializer * _serlzr = new_serializer( NULL );
            CNC_ASSERT( myPid() == 0 );
            // int _pid = myPid();
            CNC_ASSERT( theDistributor->m_sync.size() == 0 );
            (*_serlzr) & PING; // & _pid;
            bcast_msg( _serlzr );
            // pongs are sent within recv_msg
            int _n = numProcs() - 1;
            int _tmp;
            // wait for all pongs to arrive
            for( int i = 0; i < _n; ++i ) {
                theDistributor->m_sync.pop( _tmp );
            }
            int _res = theDistributor->m_nMsgsRecvd.fetch_and_store( 0 ); //_ret;
            // std::cerr << "\t." << _res << std::endl;
            return _res;// - (2*_n + 1);
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// distributable_contexts must get serializer through the distributor
        /// when done, serializer must be freed by caller using "delete".
        serializer * distributor::new_serializer( const distributable_context * dctxt )
        {
            //            ++theDistributor->m_nMsgsRecvd;
            serializer * _serlzr = new serializer( false, true );
            _serlzr->set_mode_pack();
            int _dctxtId = dctxt ? dctxt->gid() : SELF;
            (*_serlzr) & _dctxtId;
            return _serlzr;
        }

    } // namespace Internal
} // namespace CnC
