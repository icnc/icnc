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

#ifndef _CnC_ITEM_COLLECTION_BASE_H_
#define _CnC_ITEM_COLLECTION_BASE_H_

#include <tbb/task.h>
#include <cnc/internal/item_properties.h>
#include <cnc/internal/item_collection_i.h>
#include <cnc/internal/scheduler_i.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/data_not_ready.h>
#include <cnc/internal/scalable_object.h>
#include <cnc/internal/scalable_vector.h>
#include <cnc/internal/cnc_stddef.h>
#include <cnc/internal/suspend_group.h>
#include <cnc/internal/tbbcompat.h>
#include <cnc/internal/service_task.h>
#include <cnc/serializer.h>
#include <cnc/default_tuner.h>

namespace CnC {
    namespace Internal {

#ifndef CNC_ENABLE_GC
        // if 0, no distributed GC happens
        // if > 0
        //   sending getcounts is delayed until that many getcounts have been decremented (locally)
        //   sending item erase request is delayed until half as many items have been erased (locally)
# define CNC_ENABLE_GC 100
#endif

        class context_base;
        template< typename Tuner > const Tuner & get_default_tuner();

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        namespace IC {
            static const char REQUEST       = 0; /// request item
            static const char DELIVER       = 1; /// deliver item
            static const char DELIVER_TO_OWN= 2; /// deliver item and give ownership
            static const char ERASE         = 3; /// items that can be erased from table
            static const char GET_COUNTS    = 4; /// send number of local get-counts for alien items
            static const char PROBE         = 5; /// request item for immediate response
            static const char UNAVAIL       = 6; /// item unavailable
            static const char GATHER_REQ    = 7; /// request gather items at one process
            static const char GATHER_RES    = 8; /// response to gather request
            static const char RESET         = 9; /// request reset
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// The actual core of a item-collection; putting data, getting data
        /// and all the complicated synchronization between gets and puts.
        ///
        /// When we look for an item with get(tag), if there is
        /// no item associated with the tag, we will queue the StepInstance
        /// on a local queue until we see the corresponding Put( tag, item ).
        /// This queue is local to the item.
        ///
        /// This is implemented by having get throw an exception when
        /// it does not find at <tag, value> pair in the ItemCollection.
        /// The exception is caught in the execute method of step instance 
        /// a failed get calls wait_for_put
        /// to see if the stepInstance should be queued on a local queue.
        /// This is done by creating a suspend group through the scheduler.
        ///
        /// If we are calling get from the environment, we cannot replay
        /// if the data is not ready.  We block by passing NULL as step instance
        /// as member of the suspend group.
        /// 
        /// Optionally, getting unavailable items can return unavailability without throwing
        /// an exception. This is needed fo for tuner::depends and unsafe_get getting. All gets
        /// finally go through wait_for_put.
        ///
        /// In the queues, we map the tag to a pair that 
        /// contains a item-pointer and item_properties (get-count, suspend group, owner and subscriber list).
        ///
        /// After inserted or modified, the pair can have 2 states:
        /// 1. item is set          -> item is present and suspend group/queue must be NULL;
        /// 2. suspendgroup is set  -> some thread is waiting, item is not present (item must be NULL)
        ///
        /// Through accessors we guarantee that the any change of status is protected.
        ///
        /// The environment as a sepcial kind of stepinstance is identified by NULL.
        ///
        /// We store pointers to items. Allocating individual items is not optimal, but it helps
        /// us with identifying whether an item was and but and it good for freeing item store.
        /// It also avoids the requirement for a default constructor for the item-type.
        ///
        /// Using the Tuner the actual item store (table_type) can be replaced, it must implement the same
        /// interface as hash_item_table. We have an extra layer which maps an integer to an
        /// table_type (item_collection_base -> item_collection_base).
        ///
        /// The tuner defines the item-storage class/type.
        ///
        /// Before an item is inserted into our item-store, we check if we actually need it
        /// on the local process. We evaluate what tuner::consumed_on returns for the
        /// given tag/item. If we know where the item gets consumed, we only send it where
        /// its needed (and it doesn't get sotred, if we know we don't need it locally).
        /// In dist-mode we assign a owner to each item-instance: the first entry in the
        /// consumed on list or the local process. The owner is responsible for garbage collection.
        ///
        /// Garbage collection is supported based on tuner::get_count. If provided,
        /// the item updates the current CnC::Internal::get_list when get is called.
        /// tagged_step_instance takes care of actually decrementing all get-counts
        /// in that list once the current step has finished successfully. 
        /// \see CnC::Internal::get_list
        ///
        /// Distributed GC is somewhat more complex. After CNC_ENABLE_GC many get-counts
        /// have been decremented locally, the process request the number of decrements
        /// for the items it owns. A received decrement-count is added to local count and
        /// if the combined count equals the get-count, it erases the item locally and
        /// communicates the erase-request to remote processes. To avoid too many small
        /// messages, not only the decrement request is for a set of items, also the 
        /// erase requests are queued and send as a set.
        ///
        /// Distributed GC is done asynchronously in the background.
        /// The get-count request is currently a bcast, in theory, if compute_on is provided,
        /// we could only send it to the processes which consume the item. It is unclear if
        /// the additional bookkeping would actually outweigh the gain in message size.
        /// The same is true for the erase requests.
        /// Sending decrement counts is of course not a bcast, just a message to the owner.
        template< class T, class item_type, class Tuner >
        class item_collection_base : public item_collection_i
        {
        private:
            enum { UNKNOWN_PID = 0x80000000 };
            typedef tbb::spin_mutex my_mutex_type;
            typedef item_collection_base< T, item_type, Tuner > my_type;
        public:
            typedef typename Tuner::template table_type< T, item_type, my_type > table_type;
            typedef typename table_type::const_iterator const_iterator;
            item_collection_base( context_base & g, const std::string & name );
            item_collection_base( context_base & g, const std::string & name, const Tuner & tnr );
            ~item_collection_base();
            void set_max( size_t mx );
            void put(  const T & user_tag, const item_type & item );
            void put_or_delete( const T & user_tag, item_type * item, int amOwner = UNKNOWN_PID, bool fromRemote = false );
            void get( const T & user_tag, item_type & item );
            bool unsafe_get( const T & user_tag, item_type & item );
            const item_type * delay_step( const T & user_tag );
            void decrement_ref_count( const tag_base *tptr );
            void decrement_ref_count( const T & tag, int cnt = 1, bool send = true );
            void decrement_ref_count( typename table_type::accessor & aw, const T & tag, int cnt = 1, bool send = true );
            size_t size() const;
            const_iterator begin() const;
            const_iterator end() const;
            virtual void unsafe_reset( bool );
            virtual void cleanup();
            bool empty() const;
            /// send get-counts to owners (distCnC)
            void send_getcounts( bool enter_safe );

            item_type * create( const item_type & org ) const;
            void uncreate( item_type * item ) const;

            typedef struct _callback { virtual void on_put( const T &, const item_type & ) = 0; } callback_type;
            void on_put( callback_type * cb );

            const Tuner & tuner() const { return m_tuner; }

         private:
             typedef tbb::scalable_allocator< item_type > item_allocator_type;

             /// if owner <0 it will be interpreted as unknown onwer/producer
             typename table_type::item_and_gc_type delay_step( const T & user_tag, typename table_type::accessor & a, step_instance_base * s = NULL );
             void update_get_list( step_instance_base * si, const T & user_tag );

            void do_reset();
            /// in a distributed context, request item from clones
            inline void put_request( const T & tag, bool probe = false, int rcpnt = UNKNOWN_PID );
            /// in a distributed context, other clones might request an item
            inline void get_request( const T & tag, int recpnt, bool probe = false );
            /// in a distributed context send item to given consumer (unless its myPid())
            /// \return true if item needs to be stored locally, false otherwise
            bool deliver_to_consumers(  const T & user_tag, item_type * item, int & owner, int _consumer );
            template< typename VEC >
            bool deliver_to_consumers(  const T & user_tag, item_type * item, int & owner, const VEC & _consumers );
            /// in a distributed context, deliver requested item after put
            inline void deliver( const T & tag, const item_type * item, int recpnt, const char dtype, int owner = UNKNOWN_PID );
            /// in a distributed context, deliver requested item after put
            template< class RecpList >
            inline bool deliver( const T & tag, const item_type * item, const RecpList &, const char dtype, int owner = UNKNOWN_PID );
            /// if env is getting item when graph eval is done, we must probe remote procs for item if not locally available, accessor will be released.
            void probe( const T & tag, typename table_type::accessor & a );
            /// in a distributed context, messages (requesting ro delivering items) from clones arrive here
            virtual void recv_msg( serializer * );
            /// send gather request to all other processes; they will send all their items to requesting process
            void gather();
            /// send gather response to requesting root
            void gather( int root );
            /// sync get-counts of not-owned items; not thread-safe!
            inline void send_erased( bool enter_safe );
            /// add tag to serializer for sending erased info
            inline void add_erased( const T & tag, bool send = true );
            /// erase alien (not owned) tag (and item)  from table
            void erase_alien( const T & tag );

            /// implements all the logic around unsuccessfull gets. Accessor a must point to the entry and hold the lock.
            typename table_type::item_and_gc_type wait_for_put( const T &, typename table_type::accessor & a, step_instance_base * s, bool do_throw = true );
            /// block until the corresponding put occurs.
            /// will release the given lock/accessor
            void block_for_put( const T & user_tag, typename table_type::accessor & a );

        protected:
            void erase( typename table_type::accessor & a );
            table_type tagItemTable;
        private:
            const Tuner & m_tuner;
            mutable item_allocator_type m_allocator;
            typedef std::vector< callback_type * > callback_vec;
            callback_vec m_onPuts;

#ifdef _DIST_CNC_
            class get_count_collector
            {
            public:
                get_count_collector( const distributable * ic );
                ~get_count_collector();
                template< typename Tag > void collect(  const Tag & tag, item_properties & ip, const distributable_context & ctxt );
                void send_getcounts( bool enter_safe, const distributable_context & ctxt );
            private:
                serializer          ** m_sers;
                const distributable  * m_ic;
                serializer::reserved * m_reserved;
                int                  * m_counts;
            };
            
            get_count_collector m_gcColl;
            my_mutex_type m_serMutex;
            my_mutex_type m_eraseMutex;
            my_mutex_type m_cleanUpMutex;
            serializer   * m_erasedSer;
            serializer::reserved m_erasedRes;
            tbb::concurrent_bounded_queue< int > m_gathering;
            tbb::concurrent_bounded_queue< int > m_gCounting;
            int            m_numErased;
            int            m_numAlienItems;
            int            m_numgc;
            friend struct gatherer;
            struct wait_for_gc_to_complete;
            friend struct wait_for_gc_to_complete;
#endif
        }; // class item_collection_base

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static inline bool _UNUSED_ consumers_unspecified( int c )
        {
            return c == CONSUMER_UNKNOWN;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename VEC >
        static inline bool _UNUSED_ consumers_unspecified( const VEC & c )
        {
            return c.size() == 0 || c[0] == CONSUMER_UNKNOWN;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static bool _UNUSED_ am_consumer( int c, int mpid )
        {
            // ALL_OTHERS means: if I produce it -> I will not get it
            //                   if someone else produces it -> I will get it
            // for LOCAL its the other way round
            // -> cannot make a 100% safe check here
            return mpid == c || CONSUMER_ALL == c || CONSUMER_ALL_OTHERS == c || CONSUMER_LOCAL == c ? true : false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename VEC >
        static bool _UNUSED_ am_consumer( const VEC & c, int mpid )
        {
            for( typename VEC::const_iterator i = c.begin(); i != c.end(); ++i ) {
                if( am_consumer( *i, mpid ) ) return true;
            }
            return false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        static bool _UNUSED_ am_only_consumer( int c, int mpid )
        {
            // ALL_OTHERS means: if I produce it -> I will not get it
            //                   if someone else produces it -> I will get it
            // for LOCAL its the other way round
            // -> cannot make a 100% safe check here
            return mpid == c || CONSUMER_LOCAL == c ? true : false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< typename VEC >
        static bool _UNUSED_ am_only_consumer( const VEC & c, int mpid )
        {
            if( c.size() != 1 ) return false;
            return am_only_consumer( c[0], mpid );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        item_collection_base< T, item_type, Tuner >::item_collection_base( context_base & g, const std::string & name, const Tuner & tnr )
            : item_collection_i( g, name ),
              tagItemTable( this ),
              m_tuner( tnr ),
              m_allocator(),
              m_onPuts()
#ifdef _DIST_CNC_
#ifndef  __GNUC__
#pragma warning(suppress : 4355)
#endif
	    , m_gcColl( this ),
              m_serMutex(),
              m_eraseMutex(),
              m_cleanUpMutex(),
              m_erasedSer( NULL ),
              m_erasedRes(),
              m_gathering(),
              m_gCounting(),
              m_numErased( 0 ),
              m_numAlienItems( 0 ),
              m_numgc( 0 )
#endif
        {
            m_tuner.init_table( tagItemTable );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        item_collection_base< T, item_type, Tuner >::item_collection_base( context_base & g, const std::string & name )
            : item_collection_i( g, name ),
              tagItemTable( this ),
              m_tuner( get_default_tuner< Tuner >() ),
              m_allocator(),
              m_onPuts()
#ifdef _DIST_CNC_
#ifndef  __GNUC__
#pragma warning(suppress : 4355)
#endif
            , m_gcColl( this ),
              m_serMutex(),
              m_eraseMutex(),
              m_cleanUpMutex(),
              m_erasedSer( NULL ),
              m_erasedRes(),
              m_gathering(),
              m_gCounting(),
              m_numErased( 0 ),
              m_numAlienItems( 0 ),
              m_numgc( 0 )
#endif
        {
            m_tuner.init_table( tagItemTable );
        }
    
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
        template< class T, class item_type, class Tuner >
        item_collection_base< T, item_type, Tuner >::~item_collection_base()
        {
#ifdef _DIST_CNC_
            my_mutex_type::scoped_lock _lock( m_cleanUpMutex );
#endif
            //            if( distributor::myPid() == 0 ) m_context.wait();
            // free the item_type 
            do_reset();
            for( typename callback_vec::iterator i = m_onPuts.begin(); i != m_onPuts.end(); ++i ) {
                delete (*i);
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::set_max( size_t mx )
        {
            this->tagItemTable.resize( mx + 1 );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::get( const T & user_tag, item_type & item )
        {
            step_instance_base * _stepInstance = m_context.current_step_instance();
            chronometer::get_timer _timer( _stepInstance );

            if( _stepInstance ) {
                // we are calling from a step

                // if item is found, return it.
                typename table_type::accessor a;
                typename table_type::item_and_gc_type _tmpitem = tagItemTable.get_item_or_accessor( user_tag, a );
                do {
                    if( _tmpitem.first ) {
                        item = *_tmpitem.first;
                        if( _tmpitem.second ) this->update_get_list( _stepInstance, user_tag );
                        if ( trace_level() > 0 ) {
                            Speaker oss;
                            oss << "Get item " << name() <<  "[";
                            cnc_format( oss, user_tag ) << "] -> ";
                            cnc_format( oss, item );
                        }
                        return;
                    } else {
                        // no data in the table
                        // if we are a step throw exception for replay
                        
                        if ( trace_level() > 0 ) {
                            Speaker oss;
                            oss << "Get item " << name() <<  "[";
                            cnc_format( oss, user_tag ) << "] -> not ready";
                        }
                        
                        _tmpitem = this->wait_for_put( user_tag, a, _stepInstance ); // will throw( data_not_ready( this, new typed_tag< T >( user_tag ) ) );
                        if( _tmpitem.first ) continue;
                        CNC_ABORT( "Unexpected code path taken." );
                    }
                } while( _tmpitem.first );

            } else {
                const int NUM_TRIALS = 1000;
                // If we are calling from the environment, we cannot replay
                bool probed = false;
                int _trials = 0;
                do {
                    typename table_type::accessor a;
                    typename table_type::item_and_gc_type _tmpitem = tagItemTable.get_item_and_accessor( user_tag, a );
                    if( _tmpitem.first ) {
                        // item is available: GC and return it
                        item = *_tmpitem.first;
                        // FIXME: GC from env
                        //                        if( _tmpitem.second ) decrement_ref_count( a, user_tag );
                        a.release();
                        if ( trace_level() > 0 ) {
                            Speaker oss;
                            oss << "Get item " << name() <<  "[";
                            cnc_format( oss, user_tag ) << "] -> ";
                            cnc_format( oss, item );
                        }
                        return;
                    } else {
                        // we have send a request to remote procs, item still not there
                        if( probed ) {
#ifdef _DIST_CNC_
                            // wait() might not have been called, so we don't know if remote procs are
                            // done or not. So we need to iterate until it appears
                            // we stop iterating after some time; e.g. guessing we really reached quiescence
#else
                            break;
#endif
                        }
                    }
                    // item not available
                    if ( m_context.scheduler().active() ) {
                        // and we must block, item might still arrive
                        // note if we only have one environment, then there can
                        // be only one Get blocked
                        this->block_for_put( user_tag, a ); // a is released hereafter
                        // ok, next accessor get must succeed!
                    } else {
                        // Graph has already finished it's execution
                        // Blocking on a get will freeze the application forever
                        // in dist CnC, we need to check if item is available remotely
                        if( ! probed ) {
                            this->probe( user_tag, a );
                            // we probed, if next get doesn't succeed, it won't arrive
                            // (we reached quiescence, not 100% guaranteed in dist-mode)
                            probed = true;
                        } else {
                            tbb::this_tbb_thread::sleep(tbb::tick_count::interval_t(0.005));  
                        }
                    } 
                } while( ++_trials < NUM_TRIALS );
                // If not available, simply warn a user and proceed
                Speaker oss;
                oss << "Error: Graph execution finished"
                    << (( _trials < NUM_TRIALS ) ? "" : " (presumingly)")
                    << ", but item is not available " << name() << "[";
                    cnc_format( oss, user_tag ) << "]. Returning value is undefined.";
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< class T, class item_type, class Tuner >
        bool item_collection_base< T, item_type, Tuner >::unsafe_get( const T & user_tag, item_type & item )
        {
            step_instance_base * _si = m_context.current_step_instance();
            chronometer::get_timer _timer( _si );

            typename table_type::accessor a;
            typename table_type::item_and_gc_type _item = this->delay_step( user_tag, a, _si );
            if( _item.first ) {
                if( _item.second ) this->update_get_list( _si, user_tag );
                item = *_item.first;

                if ( trace_level() > 0 ) {
                    Speaker oss;
                    oss << "Unsafe get item " << name() <<  "[";
                    cnc_format( oss, user_tag ) << "] -> ";
                    cnc_format( oss, item );
                }
                
                return true;
            }
            return false;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// When using getcounts, we maintain a "Get list" of items read by a stepInstance.      
        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::update_get_list( step_instance_base * si, const T & user_tag )
        {
            // Add the item collection and tag to the Get list of the step
            if( si != NULL ) {
                si->glist().push( this, new typed_tag< T >( user_tag ) );
            } else {
                CNC_ABORT( "Internal GC error" );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::decrement_ref_count( const tag_base *tptr )
        {
            const typed_tag< T > * tag_ptr = dynamic_cast< const typed_tag< T > * >( tptr );
            this->decrement_ref_count( tag_ptr->Value() );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::erase( typename table_type::accessor & aw )
        {
#ifdef _DIST_CNC_
            if( aw.item() && ! aw.properties()->am_creator() ) {
                serializer _ser;
                _ser.set_mode_cleanup();
                _ser.cleanup( const_cast< item_type & >( *aw.item() ) );
            }
#endif
            uncreate( tagItemTable.erase( aw ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::decrement_ref_count( const T & tag, int cnt, bool send )
        {
            typename table_type::accessor aw;
            
            if( tagItemTable.get_accessor( tag, aw ) ) {
                decrement_ref_count( aw, tag, cnt, send );
#ifdef _DIST_CNC_
            } else if( ! send ) {
                // This must come from a getcount msg -> the item will arrive at some point
                {
                    typename table_type::accessor _aw;
                    typename table_type::item_and_gc_type _tmpitem = tagItemTable.get_item_or_accessor( tag, _aw );
                    if( _tmpitem.first == NULL ) {
                        CNC_ASSERT( _aw.properties()->get_count() == item_properties::UNSET_GET_COUNT );
                        _aw.properties()->set_get_count( 0 );
                    }
                }
                this->decrement_ref_count( tag, cnt, send );
#endif
            } else { 
                // This must come from a getlist decrement -> the item must be there!
                std::ostringstream oss;
                oss << "Error: initial ref count of item [" << name() << ": ";
                cnc_format( oss, tag );
                oss << "] was too low.\n";
                CNC_ABORT( oss.str() );
            }
        }

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::decrement_ref_count( typename table_type::accessor & aw, const T & tag, int cnt, bool send )
        {
            item_properties * _prop = aw.properties();
            CNC_ASSERT( _prop->get_count() != item_properties::NO_GET_COUNT );
            CNC_ASSERT( _prop->get_count() > 0 || ! _prop->am_owner() );
            int _gc = _prop->set_or_increment_get_count( -cnt );
                
            if( trace_level() > 1 ) {
                Speaker oss;
                oss << "Item " << name() <<  "[";
                cnc_format( oss, tag ) << "] now has a get-count of " << _gc;
            }
            if( _gc == 0 ) {
                CNC_ASSERT( _prop->am_owner() );
                this->erase( aw );
                if( CNC_ENABLE_GC ) this->add_erased( tag, send );
            }
#ifdef _DIST_CNC_
            else if( CNC_ENABLE_GC && _gc < 0 && _prop->has_owner() ) {
                my_mutex_type::scoped_lock _lock( m_eraseMutex );
                m_gcColl.collect( tag, *_prop, m_context );
                if( ++m_numgc > CNC_ENABLE_GC ) {
                    m_gcColl.send_getcounts( false, m_context );
                    m_numgc = 0;
                }
            }
#endif
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        item_type * item_collection_base< T, item_type, Tuner >::create( const item_type & org ) const
        {
            item_type * _item = m_allocator.allocate( 1 );
            m_allocator.construct( _item, org );
            return _item;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::uncreate( item_type * item ) const
        {
            if( item ) {
                m_allocator.destroy( item );
                m_allocator.deallocate( item, 1 );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::put( const T & t, const item_type & i )
        {
            put_or_delete( t, create( i ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::put_or_delete( const T & user_tag, item_type * item, int owner, bool fromRemote )
        {
            // depending on the answer of the tuner, the item is send to remote
            // processes and/or inserted into the local collection.

            // If the item was already inserted, the given item is deleted
            // and a warning is issued; no other action will be performed.

            // If the item was not yet inserted, the item_table returns an entry.
            // the entry might be new or it might have been created by a preceeding
            // unsuccessfull get or remote request (distCnC).
            // If there was a unsuccessful get before the put, the entry will contain
            // a suspend-group in its properties.
            // If the item was requested (distCnC) it will have a subscriber list.

            chronometer::put_timer _timer( m_context.current_step_instance() );
            if( distributor::numProcs() <= 1 ) owner = distributor::myPid();
            // First check tuner if we need to send it away
            if( fromRemote || distributor::numProcs() <= 1 || this->deliver_to_consumers( user_tag, item, owner, m_tuner.consumed_on( user_tag ) ) ) {
                // local insertion

                // getcount FIXME on dist case we might want to send the getcount
                int getcount = m_tuner.get_count( user_tag );
                if( getcount == 0 && owner == distributor::myPid() ) {
                    for( typename callback_vec::iterator i = m_onPuts.begin(); i != m_onPuts.end(); ++i ) {
                        (*i)->on_put( user_tag, *item );
                    }
                    uncreate( item );
                    return;
                }
                if( getcount != item_properties::NO_GET_COUNT && owner != distributor::myPid() ) getcount = 0;
                // Now try to insert a new pair in the map
                // This the frequent case.
                //
                // It also simplifies thinking about races, because we always
                // know we have something in this slot in the table.
                typename table_type::accessor aw;
                // we'll hold a lock/accessor for the entry after calling tagItemTable.put_item
                if( tagItemTable.put_item( user_tag, item, getcount, owner, aw ) ) {
                    // There might be queue of step instances, waiting for this Put.
                    // -> schedule all of the step instances in the queue.                
                    item_properties * _prop = aw.properties();
                    if( _prop->m_suspendGroup ) {
                        if( scheduler_i::resume( _prop->m_suspendGroup ) ) {
                            // We need to write to the blocking queue if the environment
                            // is waiting. 
                            m_context.unblock_for_put();
                        }
                        CNC_ASSERT( _prop->m_suspendGroup == NULL );
                    }
#ifdef _DIST_CNC_
                    if( fromRemote ) _prop->unset_creator();
                    if( _prop->m_subscribers ) {
                        if( _prop->am_owner() ) {
                            this->deliver( user_tag, item, *_prop->m_subscribers, IC::DELIVER, distributor::myPid() );
                        }
                        delete _prop->m_subscribers;
                        _prop->m_subscribers = NULL;
                    }
#endif 
                    if( owner == distributor::myPid() ) {
                        for( typename callback_vec::iterator i = m_onPuts.begin(); i != m_onPuts.end(); ++i ) {
                            (*i)->on_put( user_tag, *item );
                        }
                    }
                } else {
#ifdef _DIST_CNC_
                    if( fromRemote ) {
                        serializer _ser;
                        _ser.set_mode_cleanup();
                        _ser.cleanup( *item );
                    }
#endif
                    uncreate( item );
                    // The insert failed. There is already a record in the table.
                    // We still hold the lock, and aw points to the record.
                    item_properties * _prop = aw.properties();

                    // We cannot find data in a fully correct program.
                    // If a step instance did a put before all gets, it might get requeued after a put.
                    // On subsequent executions, the the step instance will put the same items.
                    // The new value should equal the old value. We cannot check without requiring
                    // operator==, so let's issue a warning.
                    // in case of distCnC, we might receive the same item again, so let's supress the warning if
                    // we are not the owner
                    CNC_ASSERT( ! _prop->m_suspendGroup );
#ifdef _DIST_CNC_
                    CNC_ASSERT( ! _prop->m_subscribers );
#endif
                    if( ! fromRemote || _prop->am_owner() ) {
                        Speaker oss( std::cerr );
                        oss << "Warning: multiple assignments to same item " << name() << "[";
                        cnc_format( oss, user_tag ) << "]";
                    }
                }
                
                if ( trace_level() > 0 ) {
                    Speaker oss;
                    oss << "Put item " << name() <<  "[";
                    cnc_format( oss, user_tag ) << "] -> ";
                    cnc_format( oss, *aw.item() );
                    if( ( getcount & item_properties::NO_GET_COUNT ) != item_properties::NO_GET_COUNT ) {
                        oss << " with get-count " << aw.properties()->get_count();
                    }
                    if( fromRemote ) oss << " {from remote}";
                }
            } else {
                uncreate( item );
                return;
            }
        }

 
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        typename item_collection_base< T, item_type, Tuner >::table_type::item_and_gc_type item_collection_base< T, item_type, Tuner >::delay_step( const T & user_tag,
                                                                                                                                                    typename table_type::accessor & a,
                                                                                                                                                    step_instance_base * s )
        {
            // we'll hold a lock if tagItemTable.get_item_or_accessor doesn't find a valid entry
            typename table_type::item_and_gc_type _tmpitem = tagItemTable.get_item_or_accessor( user_tag, a );
            if( _tmpitem.first )  return _tmpitem;
            // we hold a lock/accessor!
            _tmpitem = this->wait_for_put( user_tag, a, s, false );

            if ( trace_level() > 0 ) {
                Speaker oss;
                oss << "Delay item " << name() <<  "[";
                cnc_format( oss, user_tag ) << "] " << ( _tmpitem.first ? "" : "-> not ready" );
            }
                    
            return _tmpitem;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        const item_type * item_collection_base< T, item_type, Tuner >::delay_step( const T & user_tag )
        {
            typename table_type::accessor a;
            return this->delay_step( user_tag, a, NULL ).first;
        }
            
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        typename item_collection_base< T, item_type, Tuner >::table_type::item_and_gc_type item_collection_base< T, item_type, Tuner >::wait_for_put( const T & tag,
                                                                                                                                                      typename table_type::accessor & aw,
                                                                                                                                                      step_instance_base *,
                                                                                                                                                      bool do_throw )
        {
            // A get( <tag> ) is waiting for a put( <tag> ).

            //  - a stepInstance will be replayed when the put( <tag> ) arrives
            //    This stepInstance is waiting for a call to put( <tag> ).

            // An entry exists.  aw points to it.  aw is a write accessor/lock to maintain exclusive access
            item_properties * _prop = aw.properties();

            // maybe someone has put the item in the meanwhile
            // posible if using the vector table! Really?
            if( NULL != aw.item() ) return typename table_type::item_and_gc_type( aw.item(), _prop->get_count() != item_properties::NO_GET_COUNT );

            // prepare this Step Instance and throw exception

            //_stepInstance->decCounter();
            if( _prop->m_suspendGroup == NULL ) {
                if( distributor::numProcs() <= 1 || CnC::Internal::consumers_unspecified( m_tuner.consumed_on( tag ) ) ) {
                    // request item only the first time
                    int _owner = UNKNOWN_PID;
                    if( distributor::numProcs() > 1 ) {
                        _owner = m_tuner.produced_on( tag );
                        if( _owner == CnC::PRODUCER_UNKNOWN ) _owner = UNKNOWN_PID;
                        else if( _owner == CnC::PRODUCER_LOCAL ) _owner = distributor::myPid();
                    }
                    this->put_request( tag, false, _owner );
                } else {
                    CNC_ASSERT( am_consumer( m_tuner.consumed_on( tag ), distributor::myPid() ) );
                }
            }
            _prop->m_suspendGroup = scheduler_i::suspend( _prop->m_suspendGroup );
            // _stepInstance->suspend(); is called within scheduler().suspend
            if( do_throw ) {
                throw( DATA_NOT_READY );
            } else {    
                // this fails if getx and normal gets are mixed: CNC_ASSERT( ! _stepInstance->prepared() );
            }

            return typename table_type::item_and_gc_type( (const item_type *)NULL, false );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::block_for_put( const T & user_tag, typename table_type::accessor & a )
        {
            // A get( <tag> ) is waiting for a put( <tag> ).
            //  - the environment will block here until the put( <tag> ) arrive
            // we block until put<tag> occurs.
            item_properties * _prop = a.properties();
            _prop->m_suspendGroup = scheduler_i::suspend( _prop->m_suspendGroup );
            this->put_request( user_tag );
            // we have to release the lock
            a.release();
            m_context.block_for_put();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        size_t item_collection_base< T, item_type, Tuner >::size() const
        {
            // Return the size of a collection.
            //FIXME: we should implement a optimized version which does not gather items, but just the item counts
            const_cast< item_collection_base< T, item_type, Tuner > * >( this )->gather(); // first make sure we have all items locally available
            return tagItemTable.size();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        typename item_collection_base< T, item_type, Tuner >::const_iterator item_collection_base< T, item_type, Tuner >::begin() const
        {
            // Return an iterator referring to the first element of the Item collection

            const_cast< item_collection_base< T, item_type, Tuner > * >( this )->gather(); // first make sure we have all items locally available
            return tagItemTable.begin();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        typename item_collection_base< T, item_type, Tuner >::const_iterator item_collection_base< T, item_type, Tuner >::end() const
        {
            // Return an iterator referring to the first "past-the-end" element of the Item collection

            typename table_type::const_iterator i = tagItemTable.end();
            return i;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::unsafe_reset( bool dist )
        {
#ifdef _DIST_CNC_
            if( dist ) {
                serializer * _ser = m_context.new_serializer( this );
                (*_ser) & IC::RESET;
                m_context.bcast_msg( _ser ); 
            }
#endif
            do_reset();
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::cleanup()
        {
#ifdef _DIST_CNC_
            my_mutex_type::scoped_lock _lock( m_cleanUpMutex );
#endif
            if( CNC_ENABLE_GC ) this->send_getcounts( true );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        namespace {
            struct dumper
            {
                typedef std::string argument_type;
                void operator()( schedulable * instance, argument_type s ) const
                {
                    Speaker oss( std::cerr );
                    oss << instance << s;
                }
            };

            struct table_cleanup
            {
                template< typename Tag, typename Item >
                void operator()( const Tag & tag, const Item * item, const item_properties * ip, const std::string & name )
                {
                    if( ip->m_suspendGroup ) {
                        std::ostringstream oss;
                        oss << " is waiting for item " << name << "[";
                        cnc_format( oss, tag );
                        oss << "]";
                        dumper _dmpr;
                        ip->m_suspendGroup->for_all( _dmpr, oss.str() );
                    }
#ifdef CNC_USE_ASSERT
                    if( item && ip->am_owner() && ip->get_count() != item_properties::NO_GET_COUNT ) {
                        Speaker oss( std::cerr );
                        oss << "Warning: item " << name << "[";
                        cnc_format( oss, tag ) << "] had a wrong get-count; deletion with get-count " << ip->get_count();
                    }
#endif
#ifdef _DIST_CNC_
                    if( item && ! ip->am_creator() ) {
                        serializer _ser;
                        _ser.set_mode_cleanup();
                        _ser.cleanup( const_cast< Item & >( *item ) );
                    }
#endif
                }
            };
        }

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::do_reset()
        {
            table_cleanup _tcu;
            tagItemTable.for_all( _tcu, name() );
            tagItemTable.clear( *this );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        bool item_collection_base< T, item_type, Tuner >::empty() const
        {
            // Return a bool indicating if the Item collection is empty or not

            const_cast< item_collection_base< T, item_type, Tuner > * >( this )->gather(); // first make sure we have all items locally available
            return tagItemTable.empty();
        }
        //
        // end of support for const_iterator
        //

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::on_put( callback_type * cb )
        {
            // not thread-safe!
            m_onPuts.push_back( cb );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        //
        // DIST_CNC

#ifndef _DIST_CNC_

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::send_getcounts( bool ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::erase_alien( const T & tag ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::add_erased( const T &, bool ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::send_erased( bool ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::gather() {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::gather( int root ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::recv_msg( serializer * ser ){}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::put_request( const T & tag, bool probe, int rcpnt ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::get_request( const T & tag, int recpnt, bool probe ) {}
        template< class T, class item_type, class Tuner > void item_collection_base< T, item_type, Tuner >::deliver( const T & tag, const item_type * item, int recpnt, const char dtype, int owner ) {}

        template< class T, class item_type, class Tuner > inline
        bool item_collection_base< T, item_type, Tuner >::deliver_to_consumers( const T & user_tag, item_type * item, int & owner, int consumer )
        { 
            owner = distributor::myPid();
            return true;
        }

        template< class T, class item_type, class Tuner >
        template< typename VEC > inline
        bool item_collection_base< T, item_type, Tuner >::deliver_to_consumers( const T & user_tag, item_type * item, int & owner, const VEC & consumers )
        {
            owner = distributor::myPid();
            return true;
        }

        template< class T, class item_type, class Tuner >
        template< class RecpList > inline
        bool item_collection_base< T, item_type, Tuner >::deliver( const T & tag, const item_type * item, const RecpList & recpnts, const char dtype, int owner )
        { return true; }

        template< class T, class item_type, class Tuner > inline
        void item_collection_base< T, item_type, Tuner >::probe( const T & tag, typename table_type::accessor & a )
        { }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#else // _DIST_CNC_ is defined
        
        template< class T, class item_type, class Tuner >
        item_collection_base< T, item_type, Tuner >::get_count_collector::get_count_collector( const distributable * ic )
            : m_sers( NULL ),
              m_ic( ic ),
              m_reserved( NULL ),
              m_counts( NULL )
        {
            int _n = distributor::numProcs();
            m_sers = new serializer*[_n];
            memset( m_sers, 0, _n * sizeof( *m_sers ) );
            m_reserved = new serializer::reserved[_n];
            memset( m_reserved, 0, _n * sizeof( *m_reserved ) );
            m_counts = new int[_n];
            memset( m_counts, 0, _n * sizeof( *m_counts ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        item_collection_base< T, item_type, Tuner >::get_count_collector::~get_count_collector()
        {
            delete [] m_counts;
            delete [] m_reserved;
            delete [] m_sers;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        
        template< class T, class item_type, class Tuner >
        template< typename Tag >
        void item_collection_base< T, item_type, Tuner >::get_count_collector::collect( const Tag & tag, item_properties & ip, const distributable_context & ctxt )
        {
            int _myId = distributor::myPid();
            int _owner = ip.owner();
            CNC_ASSERT( _owner != _myId && ip.get_count() < 0 );
            CNC_ASSERT( _owner >= 0 && _owner < distributor::numProcs() );
            serializer *& _ser = m_sers[_owner];
            if( _ser == NULL ) {                    // init serializer
                _ser = ctxt.new_serializer( m_ic );
                (*_ser) & IC::GET_COUNTS & _myId;   // type of msg and sender/owner id
                m_reserved[_owner] = (*_ser).reserve( *m_counts );
            }
            do {
                (*_ser) & tag;                    // marshalling
                ++m_counts[_owner];
            } while( ip.decrement_get_count( -1 ) );
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
                
        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::get_count_collector::send_getcounts( bool enter_safe, const distributable_context & ctxt )
        {
            scalable_vector< int > _others( 0 );
            int _myId = distributor::myPid();
                    
            for( int i = 0; i < distributor::numProcs(); ++i ) {
                serializer * _ser = m_sers[i];
                if( _ser != NULL ) {
                    int _c = m_counts[i];
                    m_sers[i] = NULL;
                    m_counts[i] = 0;
                    (*_ser) & enter_safe;           // tag end of the buffer
                    (*_ser).complete( m_reserved[i], _c );
                    ctxt.send_msg( _ser, i );  // send to owner
                } else if( enter_safe && i != _myId ) {
                    _others.push_back( i );
                }
            }
            if( ! _others.empty() ) {
                serializer * _ser = ctxt.new_serializer( m_ic );
                (*_ser) & IC::GET_COUNTS & _myId & 0 & enter_safe; // "empty" msg
                ctxt.bcast_msg( _ser, &_others[0], _others.size() );  // send to owner
            }
            // ok, we are done!
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        struct item_collection_base< T, item_type, Tuner >::wait_for_gc_to_complete
        {
            wait_for_gc_to_complete( item_collection_base< T, item_type, Tuner > * ic )
                : m_ic( ic )
            {}
            
            void execute( int n )
            {
                int _tmp;
                for( int i = 0; i < n; ++i ) {
                    m_ic->m_gCounting.pop( _tmp );
                    CNC_ASSERT( _tmp == 1 );
                }
                m_ic->send_erased( true );
                for( int i = 0; i < n; ++i ) {
                    m_ic->m_gathering.pop( _tmp );
                    CNC_ASSERT( _tmp == 2 );
                }
            }
        private:
            item_collection_base< T, item_type, Tuner > * m_ic;
        };

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::send_getcounts( bool enter_safe )
        {
            if( ! distributor::active() ) return;

            {
                my_mutex_type::scoped_lock _lock( m_eraseMutex );
                m_gcColl.send_getcounts( enter_safe, m_context );
            }
            if( enter_safe ) {
                // now we have to make sure a (potentially ongoing) wait does not return until the GC has completed
                // we cannot block in this thread without requirering more than 1 worker thread
                // -> spawn a service thread that waits for us
                wait_for_gc_to_complete _w( this );
                new service_task< wait_for_gc_to_complete, int >( m_context.scheduler(), _w, distributor::numProcs() - 1 );
            }
            // ok, we are done!
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::erase_alien( const T & tag )
        {
            typename table_type::accessor aw;
            if( tagItemTable.get_accessor( tag, aw ) ) {
                CNC_ASSERT( ! aw.properties()->am_owner() );
                erase( aw );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner > inline
        void item_collection_base< T, item_type, Tuner >::add_erased( const T & tag, bool send )
        {
            if( distributor::numProcs() > 1 && ! am_only_consumer( m_tuner.consumed_on( tag ), distributor::myPid() ) ) {
                my_mutex_type::scoped_lock _lock( m_serMutex );
                if( m_erasedSer == NULL ) {
                    m_erasedSer = m_context.new_serializer( this );
                    (*m_erasedSer) & IC::ERASE;
                    m_erasedRes = m_erasedSer->reserve( m_numErased );
                }
                (*m_erasedSer) & tag;
                ++m_numErased;
                _lock.release();
                if( send ) this->send_erased( false );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner > inline
        void item_collection_base< T, item_type, Tuner >::send_erased( bool enter_safe )
        {
            my_mutex_type::scoped_lock _lock( m_serMutex );
            if( enter_safe || m_numErased > CNC_ENABLE_GC/2 ) {
                if( m_erasedSer == NULL && enter_safe ) {
                    m_erasedSer = m_context.new_serializer( this );
                    (*m_erasedSer) & IC::ERASE;
                    m_erasedRes = m_erasedSer->reserve( m_numErased );
                    CNC_ASSERT( m_numErased == 0 );
                }
                if( m_erasedSer ) {
                    serializer * _ser = m_erasedSer;
                    int _ne = m_numErased;
                    m_erasedSer = NULL;
                    m_numErased = 0;
                    (*_ser) & enter_safe;
                    _ser->complete( m_erasedRes, _ne );
                    m_context.bcast_msg( _ser );
                }
            }
        }
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::put_request( const T & tag, bool probe, int rcpnt )
        {
            if( ! distributor::active() ) return;
            int _pid = distributor::myPid();
            if( rcpnt == _pid ) return;
            serializer * _ser = m_context.new_serializer( this );
            (*_ser) & (probe ? IC::PROBE : IC::REQUEST) & tag & _pid ;
            if( rcpnt >= 0 && rcpnt < distributor::numProcs() ) {
                CNC_ASSERT( probe == IC::REQUEST );
                m_context.send_msg( _ser, rcpnt );
            } else {
                m_context.bcast_msg( _ser );
            }
            if ( trace_level() > 2 ) {
                Speaker oss;
                oss << "Request item " << name() <<  "[";
                cnc_format( oss, tag ) << "] ";
                if( rcpnt >= 0 ) oss << "with " << rcpnt;
                else oss << "as bcast";
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::get_request( const T & tag, int recpnt, bool probe )
        {
            CNC_ASSERT( distributor::active() );
            typename table_type::accessor a;
            typename table_type::item_and_gc_type _tmpitem = tagItemTable.get_item_or_accessor( tag, a );
            if( _tmpitem.first == NULL ) {
                item_properties * _prop = a.properties();
                if( _prop->m_subscribers == NULL ) {
                    _prop->m_subscribers = new typename item_properties::subscriber_list_type;
                    _prop->m_subscribers->reserve( distributor::numProcs() );
                }
                _prop->m_subscribers->push_back( recpnt );
            } else {
                tagItemTable.get_accessor( tag, a );
            }
            bool _amowner = a.properties()->am_owner();
            a.release();
            if( _amowner && _tmpitem.first != NULL ) {
                this->deliver( tag, _tmpitem.first, recpnt, IC::DELIVER, distributor::myPid() );
            } else if( probe ) {
                this->deliver( tag, NULL, recpnt, IC::UNAVAIL );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        bool item_collection_base< T, item_type, Tuner >::deliver_to_consumers( const T & user_tag, item_type * item, int & owner, int consumer )
        {
            CNC_ASSERT( owner < 0 );
            if( consumer != CONSUMER_UNKNOWN && consumer != distributor::myPid() && consumer != CONSUMER_LOCAL ) {
                if( consumer >= 0 ) {
                    owner = consumer;
                    this->deliver( user_tag, item, consumer, IC::DELIVER_TO_OWN, owner );
                    return false;
                } else if( consumer == CONSUMER_ALL ) {
                    owner = distributor::myPid();
                    this->deliver( user_tag, item, consumer, IC::DELIVER, owner );
                    return true;
                } else if( consumer == CONSUMER_ALL_OTHERS ) {
                    owner = ( distributor::myPid() + 1 ) % distributor::numProcs();
                    this->deliver( user_tag, item, consumer, IC::DELIVER, owner );
                    return false;
                } else {
                    std::cerr << "-> " << consumer << " <-\n";
                    CNC_ABORT( "Invalid recipient for item delivery." );
                }
            }
            owner = distributor::myPid();
            return true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        template< typename VEC >
        bool item_collection_base< T, item_type, Tuner >::deliver_to_consumers( const T & user_tag, item_type * item, int & owner, const VEC & consumers )
        {
            CNC_ASSERT( owner < 0 );
            if( consumers.size() > 0 ) {
                int _first = consumers.front();
                if( consumers.size() == 1 ) {
                    return this->deliver_to_consumers( user_tag, item, owner, _first );
                }
                CNC_ASSERT_MSG( owner != CONSUMER_LOCAL, "CONSUMER_LOCAL not yet supported for consumed_on returning more than one receiver" );
                // the first entry in the vector becomes the owner!
                owner = _first;
                bool _me = this->deliver( user_tag, item, consumers, IC::DELIVER, owner );
                //CNC_ASSERT_MSG( _me == false || consumers[0] == distributor::myPid(), "Local pid must be at position 0 of consumer-vector" );
                return _me;
            }
            owner = distributor::myPid();
            return true;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::deliver( const T & tag, const item_type * item, int recpnt, const char dtype, int owner )
        {
            CNC_ASSERT( distributor::active() );
            CNC_ASSERT( recpnt != CONSUMER_ALL_OTHERS || dtype == IC::DELIVER );
            CNC_ASSERT( recpnt != CONSUMER_ALL || dtype == IC::DELIVER );
            CNC_ASSERT( ( recpnt >= 0 && ( dtype == IC::DELIVER_TO_OWN || dtype == IC::DELIVER || dtype == IC::UNAVAIL ) )
                        || recpnt == CONSUMER_ALL_OTHERS || recpnt == CONSUMER_ALL );
            
            serializer * _ser = m_context.new_serializer( this );
            if( item ) {
                (*_ser) & dtype;
                if( dtype == IC::DELIVER ) {
                    (*_ser) & owner;
                }
                (*_ser) & tag & (*item);
            } else {
                CNC_ASSERT( dtype == IC::UNAVAIL );
                (*_ser) & IC::UNAVAIL;
            }
            if( recpnt >= 0 ) {
                m_context.send_msg( _ser, recpnt ); 
            } else {
                CNC_ASSERT( dtype != IC::UNAVAIL && dtype != IC::DELIVER_TO_OWN );
                m_context.bcast_msg( _ser );
            }
            if ( item != NULL && ( trace_level() > 2 ) ) {
                Speaker oss;
                oss << "Deliver item " << name() <<  "[";
                cnc_format( oss, tag ) << "] to " << recpnt << " -> ";
                cnc_format( oss, (*item) );
            }
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        template< class RecpList >
        bool item_collection_base< T, item_type, Tuner >::deliver( const T & tag, const item_type * item, const RecpList & recpnts, const char dtype, int owner )
        {
            CNC_ASSERT( distributor::active() );
            if( recpnts.size() == 1 ) {
                int _rc = recpnts.front();
                if( _rc == distributor::myPid() ) return true;
                this->deliver( tag, item, _rc, dtype, owner );
                return false;
            }
            bool _self = true;
            serializer * _ser = m_context.new_serializer( this );
            if( item ) {
                (*_ser) & dtype;
                if( dtype == IC::DELIVER ) (*_ser) & owner;
                (*_ser)  & tag & (*item);
            } else {
                (*_ser) & IC::UNAVAIL;
            }
            _self = m_context.bcast_msg( _ser, &recpnts.front(), recpnts.size() ); 
            if ( item != NULL && ( trace_level() > 2 ) ) {
                Speaker oss;
                oss << "Deliver item " << name() <<  "[";
                cnc_format( oss, tag ) << "] bcast -> ";
                cnc_format( oss, (*item) );
            }
            return _self;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::probe( const T & tag, typename table_type::accessor & a )
        {
            if( ! distributor::active() ) return;
            item_properties * _prop = a.properties();
            _prop->m_suspendGroup = scheduler_i::suspend( _prop->m_suspendGroup );
            a.release();
            
            // send request
            this->put_request( tag, true );
            
            // now re-use block_for-put stuff; we have to block for each remote process once
            int _n = distributor::numProcs() - 1; // don't wait for myself
            for( int i = 0; i < _n; ++i ) {
                m_context.block_for_put();
            }
            return;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::gather()
        {
            if( ! distributor::active() ) return;
   
            // send gather request to all processes
            int _myId = distributor::myPid();
            serializer * _ser = m_context.new_serializer( this );
            (*_ser) & IC::GATHER_REQ & _myId;
            CNC_ASSERT( m_gathering.size() == 0 );
            m_context.bcast_msg( _ser ); 
            
            // now we have to block for each remote process once
            int _n = distributor::numProcs() - 1; // don't wait for myself
            int _tmp;
            for( int i = 0; i < _n; ++i ) {
                m_gathering.pop( _tmp );
            }
            CNC_ASSERT( m_gathering.size() == 0 );
            // ok, we are done!
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        namespace {
            struct item_gather
            {
                item_gather( serializer * s ) : m_ser( s ) {}
                template< typename Tag, typename Item >
                void operator()( const Tag & tag, const Item * item, const item_properties * ip, int & n )
                {
                    if( item && ip->am_owner() ) {
                        ++n;
                        (*m_ser) & tag & *item;
                    }
                }
                serializer * m_ser;
            };
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::gather( int root )
        { // FIXME save data movement when repeatedly called. Could remember sent items. or send known item-tags when requesting gather.
            if( ! distributor::active() ) return;

            // first determine number of items that this process owns
            int _n = 0;
            serializer * _ser = m_context.new_serializer( this );
            int _myId = distributor::myPid();
            (*_ser) & IC::GATHER_RES & _myId;
            serializer::reserved _r = _ser->reserve( _n );
            item_gather ig( _ser );
            {
                my_mutex_type::scoped_lock _lock( m_cleanUpMutex );
                tagItemTable.for_all( ig, _n );
            }
            _ser->complete( _r, _n );
            // send gather response to root
            m_context.send_msg( _ser, root ); 
            CNC_ASSERT( m_gathering.size() == 0 );
            // ok, we are done!
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class T > struct is_pointer{ enum { value = 0 }; };
        template< class T > struct is_pointer< T * > { enum { value = 1 }; };

        struct gatherer
        {
            template< typename IC >
            void execute( std::pair< IC *, int > & arg )
            {
                arg.first->gather( arg.second );
            }
        };

        /// see namespace IC above for message type defs
        template< class T, class item_type, class Tuner >
        void item_collection_base< T, item_type, Tuner >::recv_msg( serializer * ser )
        {
            CNC_ASSERT( distributor::active() );
            char _msg;
            (*ser) & _msg;
            int _owner( _msg == IC::DELIVER_TO_OWN ? distributor::myPid() : -1 );
            switch( _msg ) {
                case IC::REQUEST :
                case IC::PROBE :
                {
                    T    _tag;
                    int _recpnt;
                    (*ser) & _tag & _recpnt;
                    this->get_request( _tag, _recpnt, _msg == IC::PROBE );
                    serializer _ser;
                    _ser.set_mode_cleanup();
                    _ser.cleanup( _tag );
                    break;
                }
                case IC::DELIVER:
                    (*ser) & _owner;
                case IC::DELIVER_TO_OWN :
                {
                    T    _tag;
                    item_type * _item = create( item_type() );
                    (*ser) & _tag & (*_item);
                    this->put_or_delete( _tag, _item, _owner, true ); // FIXME getcount
                    serializer _ser;
                    _ser.set_mode_cleanup();
                    _ser.cleanup( _tag );
                    break;
                }
                case IC::UNAVAIL :
                {
                    m_context.unblock_for_put();
                    break; // no cleanup
                }
                case IC::GATHER_REQ :
                {
                    int _p;
                    (*ser) & _p;
                    new service_task< gatherer, std::pair<  item_collection_base< T, item_type, Tuner > *, int > > ( m_context.scheduler(), std::make_pair( this, _p ) );
                    //                    gather( _p );
                    break; // no cleanup
                }
                case IC::GATHER_RES :
                {
                    int _n;
                    T   _tag;
                    serializer _c_ser;
                    _c_ser.set_mode_cleanup();
                    (*ser) & _owner & _n;
                    for( int i = 0; i < _n; ++i ) {
                        //                        item_type * _item = new item_type;
                        item_type * _item = create( item_type() );
                        (*ser) & _tag & (*_item);
                        this->put_or_delete( _tag, _item, _owner, true ); // FIXME getcount
                        _c_ser.cleanup( _tag );
                    }
                    m_gathering.push( 0 ); // signal to blocking thread that we received a response
                    break; // no cleanup
                }
                case IC::RESET :
                {
                    do_reset();
                    break; // no cleanup
                }
                case IC::GET_COUNTS :
                {
                    CNC_ASSERT( CNC_ENABLE_GC );
                    int _pid, _n;
                    (*ser) & _pid & _n;
                    while( _n > 0 ) {
                        T _tag;
                        (*ser) & _tag;
                        this->decrement_ref_count( _tag, 1, false );
                        --_n;
                    }
                    bool _es;
                    (*ser) & _es;
                    if( _es ) {
                        m_gCounting.push( 1 );
                    } else {
                        this->send_erased( false );
                    }
                    break; // no cleanup
                }
                case IC::ERASE :
                {
                    CNC_ASSERT( CNC_ENABLE_GC );
                    int _n;
                    (*ser) & _n;
                    while( _n ) {
                        T _tag;
                        (*ser) & _tag;
                        this->erase_alien( _tag );
                        --_n;
                    }
                    bool _es;
                    (*ser) & _es;
                    if( _es ) {
                        m_gathering.push( 2 );
                    }
                    break;
                }
                default :
                    CNC_ABORT( "Protocol error: unexpected message tag." );
            }
        }

#endif // _DIST_CNC_

        
    } // namespace Internal
} // namespace CnC

#endif // _CnC_ITEM_COLLECTION_BASE_H_
