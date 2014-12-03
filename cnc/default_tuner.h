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
  CnC tuner interface(s).
*/

#ifndef CNC_DEFAULT_TUNER_H_ALREADY_INCLUDED
#define CNC_DEFAULT_TUNER_H_ALREADY_INCLUDED

#include <cnc/internal/cnc_api.h>
#include <cnc/default_partitioner.h>
#include <cnc/internal/step_delayer.h>
#include <cnc/internal/cnc_tag_hash_compare.h>
#include <cnc/internal/no_range.h>
#include <cnc/internal/no_tag_table.h>
#include <cnc/internal/hash_tag_table.h>
#include <cnc/internal/item_properties.h>
#include <cnc/internal/item_properties.h>
#include <cnc/internal/dist/distributor.h>
#include <cnc/internal/tbbcompat.h>
#include <tbb/atomic.h>
#include <tbb/concurrent_unordered_set.h>
//#include <tbb/concurrent_hash_map.h>

namespace CnC {

    template< class T > class context;
    namespace Internal {
        template< class Tag, class Range, class StepColl, class RangeStepI, class TIR, bool deps > struct range_step;
    }

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    // Key-words tuning
    enum {
        COMPUTE_ON_LOCAL       = -2,  ///< let tuner::compute_on return COMPUTE_ON_LOCAL if the step should be executed locally
        COMPUTE_ON_ROUND_ROBIN = -3,  ///< let tuner::compute_on return COMPUTE_ON_ROUND_ROBIN to let the scheduler distribute it in a round-robin fashion
        COMPUTE_ON_ALL         = -4,  ///< let tuner::compute_on return COMPUTE_ON_ALL if the step should be executed on all processes, as well as locally
        COMPUTE_ON_ALL_OTHERS  = -5,  ///< let tuner::compute_on return COMPUTE_ON_ALL_OTHERS if the step should be executed on all processes, but not locally    
        PRODUCER_UNKNOWN    = -6,  ///< producer process of dependent item is unknown
        PRODUCER_LOCAL      = -7,  ///< producer process of dependent item is local process
        CONSUMER_UNKNOWN    = -8,  ///< consumer process of given item is unkown
        CONSUMER_LOCAL      = -9,  ///< consumer process of given item is the local process
        CONSUMER_ALL        = -10, ///< all processes consume given item
        CONSUMER_ALL_OTHERS = -11, ///< all processes but this consume given item
        NO_GETCOUNT         = Internal::item_properties::NO_GET_COUNT,   ///< no get-count specified
        AFFINITY_HERE       = Internal::scheduler_i::AFFINITY_HERE       ///< default affinity to current thread
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// Functionality that might be needed to implement all kinds of tuners.
    /// Always try to use higher level tuners which derive from this.
    /// Always use virtual inheritance (see higher level tuners).
    class tuner_base
    {
    public:
        /// returns id/rank of calling process
        /// defaults to 0 if running on one process only.
        inline static int myPid()
        {
            return Internal::distributor::myPid();
        }
        /// return total number of processes participating in this programm execution
        /// defaults to 1 if running on one process only.
        inline static int numProcs()
        {
            return Internal::distributor::numProcs();
        }
        /// returns number of threads used by scheduler in given context
        template< typename Ctxt >
        inline static int numThreads( const Ctxt & ctxt )
        {
            return ctxt.numThreads();
        }
    };


    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \brief Default (NOP) implementations of the step_tuner interface.
    ///
    /// Also defines the interface a user-provided tuner must satisfy.
    /// Derive your tuner from this (to avoid implementing the entire interface).
    ///
    /// It is recommended that your tuner does not implement the methods as templates.
    /// Instead, you should use the actual types that it expects.
    ///
    /// \#include <cnc/default_tuner.h>
    template< bool check_deps = true >
    struct /*CNC_API*/ step_tuner : public virtual tuner_base
    {
        /// \brief Allows definition of priorities to individual steps (which are identified by the tag).
        /// \return the default implementation always return 1.
        /// \param tag the tag which identifies the step to be executed
        /// \param arg the argument as passed to context< Derived >::prescribed (usually the context)
        /// \see also CNCROOT/samples/floyd_warshall
        template< typename Tag, typename Arg >
        int priority( const Tag & tag, Arg & arg ) const
        {
            return 1;
        }

        /// \brief Allows declaration of data dependencies (to items) of
        /// given step (identified by the tag).
        ///
        /// When a step-instance is prescribed through a corresponding
        /// tag_collection::put, this method will be called.  You can
        /// declare dependencies to items by calling
        /// dC.depends( item_collection, dependent_item_tag )
        /// for every item the step is going to 'get' in its execute
        /// method. The actual step execution will be delayed until
        /// all dependencies can be satisfied.  The default
        /// implementation does nothing (NOP). Your implementation
        /// must accept dC by reference (T&).
        /// \param tag the tag which identifies the step to be executed.
        /// \param arg the argument as passed to context< Derived >::prescribed
        ///            (usually the context)
        /// \param dC  opaque object (must be by reference!) providing method depends
        ///            to declare item dependencies
        template< typename Tag, typename Arg, typename T >
        void depends( const Tag & tag, Arg & arg, T & dC ) const
        {
        }

        /// \brief Returns whether the step should be pre-scheduled 
        ///
        /// Pre-scheduling provides an alternative method for detecting
        /// data dependencies, in particular if it is combined with
        /// item_collection::unsafe_get and context::flush_gets() in the
        /// step-code.
        ///
        /// The step instance will be run immediately when prescribed
        /// by a tag_collection::put. All items that are not yet
        /// available when accessed by the blocking
        /// item_collection::get() and/or non-blocking
        /// item_collection::unsafe_get() methods will automatically
        /// be treated as dependent items. The pre-run will end at the
        /// first unsuccessful blocking get or at
        /// context::flush_gets() (if any items got through
        /// item_collection::unsafe_get() were unavailable). Execution
        /// stops by throwing an exception, similar to un unsuccessful
        /// item_collection::get(). The step execution will be delayed
        /// until all detected dependencies can be satisfied.
        /// \note If all dependent items are available in the pre-scheduling
        ///       execution the step gets fully executed. This can lead to
        ///       very deep call-stacks if items are always available and
        ///       new control is produced in steps.
        bool preschedule() const
        {
            return false;
        }
                  
        /// \brief Tell the scheduler the preferred thread for executing given step
        ///
        /// Not all schedulers might actually evaluate this call (see \ref scheduler);
        /// it involves a virtual function call whenever a step is (re-)scheduled.
        /// This feature is most useful in combination with 
        /// the CNC_PIN_THREADS environment variable (\ref priorpin).
        /// \return thread id or AFFINITY_HERE (default)
        template< typename Tag, typename Arg >
        int affinity( const Tag & /*tag*/, Arg & /*arg*/ ) const
        {
            return AFFINITY_HERE;
        }

        /// \brief tell the scheduler on which process to run the step
        /// (or range of steps) (distCnC)
        ///
        /// return process id where the step will be executed, or
        /// COMPUTE_ON_ROUND_ROBIN, or COMPUTE_ON_LOCAL, or
        /// COMPUTE_ON_ALL, or COMPUTE_ON_ALL_OTHERS
        template< typename Tag, typename Arg >
        int compute_on( const Tag & /*tag*/, Arg & /*arg*/ ) const
        {
            return COMPUTE_ON_ROUND_ROBIN;
        }

        /// \brief true if steps launched through ranges consume items
        /// or need global locking, false otherwise.
        ///
        /// Avoiding checks for dependencies and global locks saves
        /// overhead and will perform better (e.g. for parallel_for).
        /// Safe execution (with checks) is the default (check_deps
        /// template argument).
        static const bool check_deps_in_ranges = check_deps;

        /// \brief check for cancelation of given step
        ///
        /// \return true if step was canceled, false otherwise (default)
        /// \note Must be thread-safe.
        /// Runtime will try to not execute the step, but it might still get executed.
        /// Best effort - but no guarantees.
        /// Canceling steps makes all determinism guarantees void.
        ///
        /// For distributed memory your implementation might require to sync its state
        /// across processes. Currently there is no API exposed to do that conveniently.
        /// However, an example implementatino CnC::cancel_tuner is provided which
        /// works on distributed memory.
        /// \see also CNCROOT/samples/floyd_warshall
        template< typename Tag, typename Arg >
        int was_canceled( const Tag & /*tag*/, Arg & /*arg*/ ) const
        {
            return false;
        }

        /// \brief check if given step-instance needs to be executed sequentially
        /// \return false by default, if true, step-instance gets queued for execution
        ///         in a sequential phase (after all workers are quienscent)
        template< typename Tag, typename Arg >
        bool sequentialize( const Tag & /*tag*/, Arg & /*arg*/ ) const
        {
            return false;
        }
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \brief Default (NOP) implementations of the pfor_tuner interface.
    ///
    /// Also defines the interface a user-provided step-tuner must satisfy.
    /// Derive your tuner from this (to avoid implementing the entire interface).
    ///
    /// It is recommended that your tuner does not implement the methods as templates.
    /// Instead, you should use the actual types that it expects.
    ///
    /// \#include <cnc/default_tuner.h>
    template< bool check_deps = true, typename Partitioner = default_partitioner<>  >
    struct /*CNC_API*/ pfor_tuner : public virtual tuner_base
    {
        template< typename Tag, typename Arg >
        int priority( const Tag & tag, Arg & arg ) const
        {
            return 1;
        }

        template< typename Tag, typename Arg, typename T >
        void depends( const Tag & tag, Arg & arg, T & dC ) const
        {
        }

        bool preschedule() const
        {
            return false;
        }

        template< typename Tag, typename Arg >
        int affinity( const Tag & /*tag*/, Arg & /*arg*/ ) const
        {
            return AFFINITY_HERE;
        }

        static const bool check_deps_in_ranges = check_deps;

        typedef Partitioner partitioner_type;

        partitioner_type partitioner() const
        {
            return typename pfor_tuner< check_deps, Partitioner >::partitioner_type();
        }
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    namespace CT {
#ifdef _DIST_CNC_
        static const char SINGLE  = 0; // single tag
        static const char ALL     = 1; // cancel all
        static const char RESET   = 2; // reset
#endif
    }

    /// \brief Step tuner with convenient cancelation capabilities
    ///
    /// Allows cancelation of individual step-instances by their tags as well as
    /// canceling all instances at once. All cancelation requests are "active" until
    /// unsafe_reset() is called (or the tuner is destructed).
    ///
    /// To use it, you need a cancel_tuner object in your context which you pass
    /// to the constructor of the respective step_collection.
    ///
    /// It works on distributed memory but might perform poorly if used frequently.
    ///
    /// \param Tag tag-type
    /// \param check_deps if false, avoid some mechanics to handle unavailable items
    /// \param Hasher hash-functor for Tag, defaults to tbb::tbb_hash< Tag >
    /// \param Equality equality operator for Tag, defaults to std::equal_to< Tag >
    /// \note It is assumed that cancelation per instance happens relatively rarely. Hence
    ///       no automatic garbage collection of the tags is provided. If you cancel individual
    ///       step-instances frequently, it is recommended to prune the internal data structure
    ///       from time to time in a safe state through unsafe_reset().
    /// \see also CNCROOT/samples/floyd_warshall
    template< typename Tag, bool check_deps = true,
              typename Hasher = cnc_hash< Tag >, typename Equality = cnc_equal< Tag > >
    class cancel_tuner : public step_tuner< check_deps >, public Internal::distributable
    {
    public:
        template< typename C >
        cancel_tuner( C & ctxt )
            : Internal::distributable( "cancel_tuner" ), m_context( ctxt ), m_canceledTags(), m_cancelAll()
        {
            m_context.subscribe( this );
            m_cancelAll = false;
        }

        ~cancel_tuner()
        {
            m_context.unsubscribe( this );
        }

        /// \brief cancel given step (identified by tag)
        void cancel( const Tag & t, bool from_msg = false  )
        {
            if( ! m_cancelAll ) {
#ifdef _DIST_CNC_
                if( !from_msg && Internal::distributor::numProcs() > 1 ) {
                    serializer * _ser = m_context.new_serializer( this );
                    (*_ser) & CT::SINGLE & t;
                    m_context.bcast_msg( _ser ); 
                }
#endif
                m_canceledTags.insert( t );
            }
        }

        /// \brief cancel all steps
        void cancel_all( bool from_msg = false )
        {
#ifdef _DIST_CNC_
            if( !from_msg && Internal::distributor::numProcs() > 1 ) {
                serializer * _ser = m_context.new_serializer( this );
                (*_ser) & CT::ALL;
                m_context.bcast_msg( _ser ); 
            }
#endif
            m_cancelAll = true;
        }

        void unsafe_reset( )
        {
            unsafe_reset( true );
        }

        /// \brief implements/overwrites step_tuner::was_canceled(...)
        template< typename Arg >
        int was_canceled( const Tag & tag, Arg & /*arg*/ ) const
        {
            return m_cancelAll == true || m_canceledTags.count( tag ) > 0;
        }

        // from distributable
        virtual void recv_msg( serializer * ser )
        {
#ifdef _DIST_CNC_
            CNC_ASSERT( Internal::distributor::active() );
            char _msg;
            (*ser) & _msg;
            switch( _msg ) {
            case CT::SINGLE :
                Tag _tag;
                (*ser) & _tag;
                this->cancel( _tag, true );
                break;
            case CT::ALL :
                this->cancel_all( true );
                break;
            case CT::RESET :
                this->unsafe_reset( false );
                break;
            default:
                CNC_ABORT( "Unexpected message received (cancel_tuner)." );
            }
#endif
        }

    private:
        /// \brief reset all current cancel states
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        virtual void unsafe_reset( bool dist )
        {
#ifdef _DIST_CNC_
            if( dist && Internal::distributor::numProcs() > 1 ) {
                serializer * _ser = m_context.new_serializer( this );
                (*_ser) & CT::RESET;
                m_context.bcast_msg( _ser ); 
            }
#endif
            m_canceledTags.clear();
            m_cancelAll = false;
        }

        Internal::distributable_context & m_context;
        tbb::concurrent_unordered_set< Tag, Hasher, Equality > m_canceledTags;
        tbb::atomic< bool > m_cancelAll;
    };


    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \brief Default implementations of the item-tuner interface for item-collections
    ///
    /// Usually you will not need to use this directly for anything else than documentation and interface.
    template< template< typename T, typename I, typename A > class TT >
    struct item_tuner : public virtual tuner_base
    {
        /// \brief Class which manages item creation/uncreation..
        ///
        /// item_allocator::type must provide 
        ///        item_type * create( const item_type & org ) const;
        ///        void uncreate( item_type * item ) const;
        ///
        /// Only C++0x allows template aliasing, need to use an indirection
        template< typename Item >
        struct item_allocator
        {
            //            typedef CnC::Internal::item_manager_default< Item > type;
            typedef tbb::scalable_allocator< Item > type;
        };

        /// \brief Defines the type of the internal data store.
        ///
        /// It forwards the functionality of the template parameter template.
        /// The expected interface of the template is not yet exposed.
        /// Use hashmap_tuner or vector_tuner to derive your own tuner.
        template< typename Tag, typename Item, typename Coll >
        struct table_type : public TT< Tag, Item, Coll >
        {
            table_type( const Coll * c, size_t sz = 0 ) : TT< Tag, Item, Coll >( c, sz ) {}
        };
        
        /// \brief Initialize the internal storage.
        ///
        /// Can be used to configure the storage table.
        /// Called in the collection constructor.
        template< typename Tag, typename Item, typename Coll >
        void init_table( TT< Tag, Item, Coll > & stbl ) const
        {}

        /// \brief Allows specifying the number of gets to the given item.
        ///
        /// After get_count() many 'get()'s the item can be removed from the collection.
        /// By default, the item is not removed until the collection is deleted.
        /// Gets by the environment are ignored, use CnC::NO_GETCOUNT for items 
        /// which are consumed by the environment.
        /// \param tag the tag which identifies the item
        /// \return number of expected gets to this item, or CNC::NO_GETCOUNT
        template< typename Tag >
        int get_count( const Tag & tag ) const
        {
            return NO_GETCOUNT;
        }

        /// \brief Tells the scheduler on which process(es) this item is going to be consumed
        ///
        /// return process id where the item will be consumed (get), or CONSUMER_UNKNOWN (default)
        ///     or std::vector<int>, containing all ids of consuming processes.
        ///        To indicate that the consumer processes are unknown, return an empty vector.
        ///        The vector must contain special values like CONSUMER_LOCAL.
        /// If not CnC::CONSUMER_UKNOWN (or empty vector), this declaration will overwrite what
        /// the step-tuner might declare in depends.
        /// Providing this method leads to the most efficient communication pattern for
        /// to getting the data to where it is needed.
        /// \param tag the tag which identifies the item
        template< typename Tag >
        int consumed_on( const Tag & tag ) const
        {
            return CONSUMER_UNKNOWN;
        }

        /// \brief Tells the scheduler on which process(es) this item is going to be produced
        ///
        /// return process id where the item will be produced. If unknown return CnC::PRODUCER_UNKNOWN.
        /// return PRODUCER_LOCAL if local process is the owner.
        /// Implementing this method reduces the communication cost for locating and sending data.
        /// Implementing item_tuner::consumed_on is more efficient, but might be more complicated.
        /// Will be evaluated only if item_tuner::consumed_on returns CnC::CONSUMER_UNKNOWN
        /// \param tag the tag which identifies the item
        template< typename Tag >
        int produced_on( const Tag & tag ) const
        {
            return PRODUCER_UNKNOWN;
        }
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    namespace Internal {
        template< typename Tag, typename ItemT, typename Coll > class hash_item_table;
        template< typename Tag, typename ItemT, typename Coll > class vec_item_table;
    }

    /// \brief The tuner base for hashmap-based item-tuners.
    ///
    /// The internal hash-map uses cnc_tag_hash_compare. If your
    /// tag-type is not supported by default, you need to provide a
    /// template specialization for it.
    struct hashmap_tuner : public item_tuner< Internal::hash_item_table >
    {
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \brief The tuner base for vector-based item-tuners.
    ///
    /// Your tags must be convertable to and from size_t.  You must
    /// provide the maximum value before accessing the collection
    /// (constructor or set_max).  The runtime will allocate as many
    /// slots. Hence, use this only if your tags-space is dense,
    /// without a large offset and if it is not too large.
    struct vector_tuner : public item_tuner< Internal::vec_item_table >
    {
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /// \brief Default implementations of the tag-tuner interface for tag-collections
    ///
    /// Use this if you are going put ranges. Optional argument is a custom partitioner. 
    /// Ranges don't work with memoization (yet)
    template< typename Range = Internal::no_range, typename Partitioner = default_partitioner<> >
    struct tag_tuner : public virtual tuner_base
    {
        /// A tag tuner must provide the type of the range, default is no range
        typedef Range range_type;
        /// A tag tuner must provide a tag-table type; default is no tag-table
        typedef Internal::no_tag_table tag_table_type;
        /// \brief The type of the partitioner
        typedef Partitioner partitioner_type;

        /// \brief return a partitioner for range-based features, such as parallel_for
        /// \see default_partitioner for the expected signature of partitioners
        /// overwrite partitioner() if it doesn't come with default-constructor or
        /// if the default constructor is insufficient.
        partitioner_type partitioner() const
        {
            return typename tag_tuner< Range, Partitioner >::partitioner_type();
        }

        /// return true if tag memoization is wanted; returns false by default (with no_tag_table)
        bool preserve_tags() const
        {
            return false;
        };
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    /// Use this if your tag-collection should preserve tags (memoization)
    /// \note Memoization doesn't work with ranges (yet)
    template< typename Tag, typename H = cnc_hash< Tag >, typename E = cnc_equal< Tag > >
    struct preserve_tuner : public tag_tuner< Internal::no_range, default_partitioner<> >
    {
        /// A tag tuner must provide a tag-table type; default is no tag-table
        typedef Internal::hash_tag_table< Tag, H, E > tag_table_type;

        bool preserve_tags() const
        {
            return true;
        };
    };

    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    namespace Internal {
        template< typename Tuner >
        const Tuner & get_default_tuner()
        {
            static tbb::atomic< Tuner * > s_tuner;
            if( s_tuner == NULL ) {
                Tuner * _tmp = new Tuner;
                if( s_tuner.compare_and_swap( _tmp, NULL ) != NULL ) delete _tmp;
            }
            return *s_tuner;
        }
    }

} // end namespace CnC

#endif //CNC_DEFAULT_TUNER_H_ALREADY_INCLUDED
