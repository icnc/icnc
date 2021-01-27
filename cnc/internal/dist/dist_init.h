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

#ifndef _CnC__DIST_CNC__INIT_H_
#define _CnC__DIST_CNC__INIT_H_


#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
// Workaround for overzealous compiler warnings 
#pragma warning (push)
#pragma warning (disable: 4251 4275)
#endif

#include <iostream>
#include <cstdlib> // getenv
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/cnc_api.h>
#include <cnc/internal/dist/factory.h>
#include <cnc/internal/dist/creatable.h>
#include <cnc/internal/dist/creator.h>

namespace CnC {

    namespace Internal {

        communicator_loader_type CNC_API dist_cnc_load_comm( const char * lib, bool use_itac );

        /// Distributed CnC is activated through an initialization object, which
        /// must be instantiated right after entering main. Inside the
        /// instantiation the dist_cnc_init object registers all contexts with a
        /// factory and launches client processes.
        /// 
        /// distCnC requires contexts to have all their collections as members and
        /// to be default-constructible. By this we do not need to serialize the
        /// collections.  When clients initialize the dist_cnc_init object, they
        /// establish the connection to the host and a n2n network. The main
        /// thread then enters a loop to receive messages. Upon program exit, the
        /// main thread closes the communication network and exits. It will never
        /// leave the dist_cnc_init initialization/constructor.
        /// 
        /// When a context is created (on the host), it gets cloned on the remote
        /// clients. Similarly, if it is destructed, remote clients get informed
        /// and will delete the duplicate, too.
        /// 
        /// Each collection is a distributable which registers itself with its
        /// context and gets an unique id. This id is used for communication
        /// through a serializer. The collections request a serializer from its
        /// context, which requests it from the distributor. Each instance stores
        /// the required information in the serializer and hands it over to the
        /// next level. Messages are received asynchronously (each process has a
        /// separate receiver thread). When a message arrives, its serializer is
        /// handed over to the distributor, which passes it to the respective
        /// context, which hands it over to the right distributable (collection or
        /// scheduler).
        /// 
        /// Remote execution of steps can be controlled by the user
        /// program though providing a tuner declared when prescribing steps. The
        /// tuner must implement a suitable method tuner::compute_on. When a tag is
        /// put, the scheduler will prepare execution. During preparation it calls
        /// tuner::compute_on. If a remote execution is requested, the tag_collection
        /// will send the tag to the respective clone (remote process). When a
        /// tag-collection receives a tag (asynchronously), it will perform normal
        /// preparation and scheduling.
        /// 
        /// During step execution items might not be locally available. Rather
        /// than proactively sending all items to all clones or maintaining a
        /// "global" directory, unavailable items are requested through a
        /// broadcast and the step is suspended. Other processes receive the
        /// request and when this item becomes available locally, they will send
        /// it to the processes which requested it. Receiving a requested item
        /// triggers conceptually the same action as a local put of the item.
        /// 
        /// If the producer/owner of an item is known, it can be declared 
        /// in the depends function of the tuner as an additional argument to
        /// depConsumer.depends.
        ///
        /// All communication between collections is symmetric (e.g. all
        /// processes, be it host or client) can request, pass on and receive
        /// items/tags.
        ///
        /// \todo
        /// context::wait invoked from clients is currently not supoported.
        /// Hence, creating contexts on clients currently fails.
        ///
        /// \todo
        /// "Global view" on collections is currently only supported in "safe" state, 
        /// e.g. if the context is currently not evaluated. Also, optimizations such as
        /// as a gather method for computing the global size of collections might be beneficial.
        ///
        /// \todo
        /// Enable mixed-mode and hierarchical communication, e.g. allow mixing socket and KNF
        /// or a cluster of KNF-attached nodes. For this we probably need some kind of message-routing.
        ///
        /// \todo
        /// Optimizations:
        ///   * agglomorate requests for items
        ///   * use optimized partial bcast when flushing
        ///   * optimize bcast (e.g. communicate over a tree)
        struct dist_init
        {
            // typedef void (*subscriber_type)();
            template<typename SubscriberType>
            dist_init( SubscriberType subscriber, long flag = 0, bool dist_env = false )
            {
                const char * dist_cnc_comm = getenv( "DIST_CNC" ); 
                communicator_loader_type loader = Internal::dist_cnc_load_comm( dist_cnc_comm,
#ifdef CNC_WITH_ITAC
                                                                                true
#else
                                                                                false
#endif
                                                                                );
                if( loader ) {
                    Internal::distributor::init( loader, dist_env );
                    subscriber();
                    Internal::distributor::start( flag );
                }
            }

            ~dist_init()
            {
                if( distributor::active() ) {
                    distributor::stop();
                }
            }
            
        };

    } // namespace Internal
} // namespace CnC

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#pragma warning (pop)
#endif // warnings 4251 4275 are back

#endif // _CnC__DIST_CNC__INIT_H_
