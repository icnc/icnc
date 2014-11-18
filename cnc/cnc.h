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
  Main include file for using CnC.
  Provides all CnC classes except debug things.
  See CnC::context for a very brief description on how to write a CnC program.
*/

#ifndef _CnC_H_ALREADY_INCLUDED_
#define _CnC_H_ALREADY_INCLUDED_

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
// Workaround for overzealous compiler warnings 
# pragma warning (push)
# pragma warning (disable: 4251 4275 4290)
#endif

#include <cnc/internal/tag_collection_base.h>
#include <cnc/internal/item_collection_base.h>
#include <cnc/internal/context_base.h>
#include <cnc/internal/no_range.h>

/// \brief CnC API
namespace CnC {

    typedef int error_type;

    // forward declarations
    template< class T > class context;
    struct debug;
    template< typename Tag, typename Tuner > class tag_collection;
    template< typename Tag, typename Item, typename Tuner > class item_collection;

    /// Steps return CNC_Success if execution was successful
    const int CNC_Success = 0;
    /// Steps return CNC_Failure if execution failed
    const int CNC_Failure = 1;

    /// \brief A step collection is logical set of step instances.
    ///
    /// A step-collection must be prescribed by a tag-collection and it
    /// can be part of consumer/producer relationships with item-collections.
    /// Additionally, it can be the controller in control-dependencies (e.g. produce tags).
    template< typename UserStep, typename Tuner = step_tuner<> >
    class step_collection : public Internal::traceable
    {
    public:
        /// the type of the step as provided by the user
        typedef UserStep step_type;
        /// the type of the tuner as provided by the user
        typedef Tuner tuner_type;
        
        /// \brief constructor which registers collection with given context
        ///
        /// \param ctxt the context this collection belongs to
        /// \param name an optional name, used for debug output and tracing
        /// \param userStep an optional user step argument, a copy will be created through copy-construction
        /// \param tnr an optional tuner object which must persist throughout the lifetime of the step-collection
        ///            by default a default-constructed tuner will be used.
        template< typename Derived >
        step_collection( context< Derived > & ctxt, const std::string & name, const step_type & userStep, const tuner_type & tnr );
        template< typename Derived >
        step_collection( context< Derived > & ctxt );
        template< typename Derived >
        step_collection( context< Derived > & ctxt, const std::string & name );
        template< typename Derived >
        step_collection( context< Derived > & ctxt, const tuner_type & tnr, const std::string & name = std::string() );
        template< typename Derived >
        step_collection( context< Derived > & ctxt, const std::string & name, const step_type & userStep );
        
        ~step_collection();

        /// Declare this step-collecation as consumer of given item-collection
        template< typename DataTag, typename Item, typename ITuner >
        void consumes( CnC::item_collection< DataTag, Item, ITuner > & );

        /// Declare this step-collecation as producer for given item-collection
        template< typename DataTag, typename Item, typename ITuner >
        void produces( CnC::item_collection< DataTag, Item, ITuner > & );

        /// Declare this step-collecation as controller of given tag-collection
        template< typename ControlTag, typename TTuner >
        void controls( CnC::tag_collection< ControlTag, TTuner > & );

    private:
        const step_type     m_userStep;
        const tuner_type   & m_tuner;
        Internal::distributable_context & m_context;
        template< class Tag, class Step, class Arg, class TTuner, class STuner > friend class Internal::step_launcher;
    };

    /// \brief A tag collection is a set of tags of the same type. It is
    /// used to prescribe steps.  By default, tags are not stored.
    ///
    /// Tag must provide copy and default constructors and the assigment
    /// operator.
    ///
    /// If Tag is not convertable into size_t, a suitable hash_compare
    /// class must be provided which satisifies the requirements for
    /// tbb::concurrent_hash_map.  The default cnc_tag_hash_compare works
    /// for types that can be converted to size_t and have an
    /// operator==. You can provide specialized templates for
    /// cnc_hash and/or cnc_equal or cnc_tag_hash_compare
    /// or specify and implement your own compatible class.
    template< typename Tag, typename Tuner = tag_tuner<> >
    class /*CNC_API*/ tag_collection
    {
    public:
        /// the tag type
        typedef Tag tag_type;

        /// \brief constructor which registers collection with given context
        ///
        /// \param ctxt the context this collection belongs to
        /// \param name an optional name, used for debug output and tracing
        /// \param tnr an optional tuner object which must persist throughout the lifetime of the tag-collection
        ///            by default a default-constructed tuner will be used.
        template< class Derived >
        tag_collection( context< Derived > & ctxt, const std::string & name, const Tuner & tnr );
        template< class Derived >
        tag_collection( context< Derived > & ctxt, const std::string & name = std::string() );
        template< class Derived >
        tag_collection( context< Derived > & ctxt, const Tuner & tnr );

        ~tag_collection();

        /// \brief Declare the prescription relationship between the tag collection
        /// and a step collection.
        ///
        /// \param s class representing step collection. s is required to
        /// provide the following const method, where Arg a is the optional
        /// parameter described below.
        ///  
        /// \code int execute( const Tag & tag, Arg & a ) const; \endcode
        ///
        /// A copy of s will be created by calling its copy constructor.
        ///
        /// \param arg This argument will be the parameter passed to
        ///            Step::execute and the tuner methods.  The object must exist as
        ///            long as instances of the given step might be executed. Usually
        ///            arg will be the containing context.
        ///
        /// \return 0 if succeeded, error code otherwise
        template< typename UserStep, typename STuner, typename Arg >
        error_type prescribes( const step_collection< UserStep, STuner > & s, Arg & arg );

        /// \brief prescribe the associated step.  If we are preserving tags for this collection, make a copy of the tag and store it in the collection.
        /// \param  t the tag to be put
        void put( const Tag & t );

        /// \brief prescribe an entire range of tags
        ///
        /// \param r  A range, which is potentially splittible through a partitioner.
        ///           Following the TBB range/splittable concept, extended by STL container requirements, 
        ///           a range R must provide the following interface:
        ///              - R::R( const R& ) : Copy constructor.
        ///              - int size() : return number of elements (tags) in range
        ///              - const_iterator : forward iterator (operator++, operator tag_type() const)
        ///                                 to make it work with tbb::blocked_range, the cast operator is used instead of operator*().
        ///              - const_iterator begin() const : first member of range
        ///              - const_iterator end() const : Exclusive upper bound on range
        ///            Using it with the default_partitioner also requires
        ///              - R::R( R& r, tbb::split ) : Split r into two subranges.
        ///              - bool R::is_divisible() const : true if range can be partitioned into two subranges.
        void put_range( const typename Tuner::range_type & r );
        void put_range( const Internal::no_range & ) const;

        /// const forward iterator as in STL
        typedef typename Tuner::tag_table_type::const_iterator const_iterator;

        /// \brief returns begin() as in STL containers
        /// \note iteration through collections is not thread safe;
        /// use it only between calls to CnC::context::wait() and putting tags
        const_iterator begin() const;

        /// \brief returns end() as in STL containers
        /// \note iteration through collections is not thread safe;
        /// use it only between calls to CnC::context::wait() and putting tags
        const_iterator end() const;

        /// \brief removes all of the tag instances from the collection
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        void unsafe_reset();

        /// returns number of elements in collection
        size_t size();

        /// returns true if size()==0, false otherwise
        bool empty();

        /// callback type for tag-collections
        /// \see register_callback
        typedef typename Internal::tag_collection_base< Tag, Tuner >::callback_type callback_type;

        /// Call this to register a on-put-callback for the tag-collection.
        /// When registered, callback.on_put( tag ) gets called when a tag was put successfully.
        /// e.g. it will not get called a second time on a second put if memoization is enabled.
        /// The call is blocking, e.g. the triggering/calling "put" will not return to its caller
        /// until the callback has terminated.
        /// The provided object will be deleted when the collection is deleted.
        /// \see graph for more details, in particular about thread-safety issues
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        /// \note not needed for regular CnC
        void on_put( callback_type * cb );

    private:
        Internal::tag_collection_base< Tag, Tuner > m_tagCollection;
        //template< class T > friend class context;
        friend struct ::CnC::debug;
    };

    /// \brief An item collection is a mapping from tags to items.
    ///
    /// Tag and Item must provide copy and default constructors and the
    /// assigment operator.
    ///
    /// Th last template argument is an optional tuner. The tuner provides 
    /// tuning hints, such as the type of the data store or information about
    /// its use in distributed environments. Most importantly it tells the runtime
    /// and compiler which type of data store it should use. By default that's
    /// a hash-map (hashmap_tuner). For non default-supported tag types (e.g. those that are
    /// not convertable into size_t) a suitable cnc_hash template
    /// specialization must be provided. If in addition your type does not support std::equal_to
    /// you also need to specialize cnc_equal.
    /// For the vector-based data store (vector_tuner) that's not
    /// necessary, but the tag-type must then be convertible to and from size_t.
    /// \see CnC::item_tuner for more information.
    ///
    /// The CnC runtime will make a copy of your item when it is 'put' into
    /// the item_collection.  The CnC runtime will delete the copied item
    /// copy once the get-count reaches 0 (or, if no get-count was
    /// provided, once the collection is destroyed).  If the item-type is a
    /// pointer type, the runtime will not delete the memory the item
    /// points to. If you store pointeres, you have to care for the appropriate 
    /// garbage collection, e.g. you might consider using smart pointers.
    template< typename Tag, typename Item, typename Tuner = hashmap_tuner >
    class /*CNC_API*/ item_collection
    {
        typedef Internal::item_collection_base< Tag, Item, Tuner > base_coll_type;
    public:
        /// the tag type
        typedef Tag tag_type;
        /// the data/item type
        typedef Item data_type;

        /// const forward iterator as in STL
        class const_iterator;

        /// \brief constructor which registers collection with given context
        ///
        /// \param ctxt the context this collection belongs to
        /// \param name an optional name, used for debug output and tracing
        /// \param tnr a tuner object which must persist throughout the lifetime of the step-collection
        ///            by default a default-constructed tuner will be used.
        template< class Derived >
        item_collection( context< Derived > & ctxt, const std::string & name, const Tuner & tnr );
        template< class Derived >
        item_collection( context< Derived > & ctxt, const std::string & name = std::string() );
        template< class Derived >
        item_collection( context< Derived > & ctxt, const Tuner & tnr );

        ~item_collection();

        /// \brief Declares the maxium tag value.
        ///
        /// Must be called prior to accessing the collection if the data store is a vector.
        /// Useful only for dense tag-spaces.
        /// \param mx the largest tag-value ever used for this collection
        void set_max( size_t mx );

        /// \brief make copies of the item and the tag and store them in the collection.
        /// \param tag        the tag identifying the item
        /// \param item       the item to be copied and stored
        void put( const Tag & tag, const Item & item );

        /// \brief get an item
        /// \param  tag the tag identifying the item
        /// \param  item reference to item to store result in
        /// \throw DataNotReady throws exception if data not yet available.
        void get( const Tag & tag, Item & item ) const;

        /// \brief try to get an item and store it in given object (non-blocking)
        ///        
        /// \attention This method is unsafe: you can create non-deterministic results if you decide to 
        ///            to perform semantically relevant actions if an item is unavailable (returns false)
        ///
        /// If the item is unavailable, it does not change item.
        /// Make sure you call flush_gets() after last call to this method (of any item collection) within a step.
        /// In any case, you must check the return value before accessing the item.
        /// \param  tag the tag identifying the item
        /// \param  item reference to item to store result in
        /// \return true if item is available
        /// \throw DataNotReady might throw exception if data not available (yet)
        bool unsafe_get( const Tag & tag, Item & item ) const;

        /// \brief returns begin() as in STL containers
        /// \note iteration through collections is not thread safe;
        /// use it only between calls to CnC::context::wait() and putting tags
        const_iterator begin() const;

        /// \brief returns end() as in STL containers
        /// \note iteration through collections is not thread safe;
        /// use it only between calls to CnC::context::wait() and putting tags
        const_iterator end() const;

        /// \brief removes all of the item instances from the collection
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        void unsafe_reset();

        /// returns number of elements in collection
        size_t size();

        /// returns true if size()==0, false otherwise
        bool empty();
        
        /// callback type for item-collections.
        /// \see register_callback
        typedef typename base_coll_type::callback_type callback_type;

        /// Call this to register a on-put-callback for the item-collection.
        /// When registered, callback.on_put( tag, item ) gets called when an item was put successfully.
        /// The call is blocking, e.g. the triggering/calling "put" will not return to its caller
        /// until the callback has terminated.
        ///
        /// The provided object will be deleted when the collection is deleted.
        ///
        /// In distCnC, the callback will be executed on the (first) process returned
        /// by tuner::consumed_on (which defaults to execution on the process which puts the item).
        ///
        /// \see graph for more details, in particular about thread-safety issues
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        /// \note not needed for regular CnC
        void on_put( callback_type * cb );

    private:
        base_coll_type m_itemCollection;
        friend struct ::CnC::debug;
        friend class Internal::step_delayer;
        friend class const_iterator;
    };


    /// \brief Base class for defining and using CnC (sub-)graphs.
    ///
    /// Derive from this class to define your own graph. It might
    /// cooperate with other CnC graphs in the same context.  A graph
    /// can either be a normal CnC graph or anything else that uses
    /// CnC collections as input and output.
    ///
    /// It is recommended to always use CnC types (e.g. collections)
    /// to implement a graph. It facilitates implementation because
    /// the CnC semantics allow automatically managing various tasks,
    /// like distribution and termination detection.
    ///
    /// The fundamental communication channel between graphs and their
    /// input/output collections are the normal get and put calls.
    ///
    /// The following paragraphs explain specifics needed to write
    /// graphs which do not adhere to core CnC semantics. None of 
    /// this is needed for graphs that internally are "normal", fully
    /// CnC compliant constructs.
    ///
    /// Besides get calls, tag- and item-collections also provide a
    /// callback mechanism.  Such callbacks can be registered for any
    /// (input) collection individually (item_collection::on_put and
    /// tag_collection::on_put). The registered callback will then be
    /// called for every incoming item/tag after it was succesfully
    /// put. The callback might (or might not) be called
    /// asynchronously.
    ///
    /// \note multiple calls to the same or different callbacks might
    ///        be issued simultaneously.  Hence your callbacks must
    ///        either have no side effects (other than through CnC) or
    ///        take care of adequate protection/synchronization
    ///
    /// \note If your graph executes anything outside of CnC steps or
    ///       outside callbacks (e.g. if spawns threads or alike), then
    ///       termination/quiescence handling needs to be addressed
    ///       explicitly (see graph::enter_quiescence and
    ///       graph::leave_quiescence). Graphs are born in
    ///       quiescent state!
    ///
    /// If you're using non-CnC data structures you will probably need
    /// to explicitly take of distributed memory. A clone of the graph
    /// gets instantiated on each process. You can send essages between
    /// siblings through serializers which get instantiated through
    /// graph::new_serializer. After marshalling the desired data
    /// (CnC::serializer), the messages can be sent to individual
    /// processes (graph::send_msg) or they may be broadcasted
    /// (graph::bcast_msg).  Messages can only be send from one
    /// instance of a graph to its siblings on other processes.
    /// Receiving messages is done by overwriting the graph::recv_msg
    /// callback.
    ///
    /// Callbacks are executed on the first process of the list of
    /// consumers/executing processes as provided by the collection's
    /// tuner. If no consumer/executing process is provided /e.g. the default)
    /// the callback is executed on the process where the item/tag is
    /// produced.
    class graph : public Internal::distributable
    {
    public:
        /// \brief A graph requires a context and a name for its initialization.
        ///
        /// Do all your initialization/start-up stuff in the
        /// constructor of your derived class.  This implies that your
        /// constructor needs to accept and wire all input-and
        /// output-collections.  The constructor must also call the
        /// respective consumes/produces/prescribes calls.  If your
        /// graph operates/computes outside the steps and callbacks
        /// you need to explicitly handle/manage quiescence (see
        /// graph::enter_quiescence, graph::leave_quiescence).
        template< typename Ctxt >
        graph( CnC::context< Ctxt > & ctxt, const std::string & name = "" );

        virtual ~graph();

        /// \brief Tell the runtime that the graph reached (temporary) quiescence.
        /// \note Entering quiescence means that it will not return to activity unless
        /// new data arrives through a callback (to be registered with graph::register_callback).
        /// \note Explicit quiescence handling is not needed if the graph operates/computes
        /// only within the callbacks.
        /// \note graphs are born in quiescent state. A call
        ///       to enter_quiescence must be paired with exactly one preceeding call to leave_quiescence
        void enter_quiescence() const;

        /// \brief Tell the runtime that the graph leaves quiescence and goes back to activity.
        ///  Must be called only within a on_put callback (to be
        /// registered through item_collection::on_put,
        /// tag_collection::on_put) or within a CnC step.  Leaving
        /// quiescence outside a callback/step leads to undefined
        /// behaviour (like dead-locks or worse).
        /// \note Explicit quiescence handling is not needed if the
        /// graph operates/computes only within the callbacks.
        /// \note graphs are born in quiescent state.
        void leave_quiescence() const;

        /// cleanup, e.g. collect garbage
        virtual void cleanup();

        /// reset instance, e.g. remove entries from collection
        /// \param dist if false, the current context is actually not distributed -> don't sync with remote siblings
        virtual void unsafe_reset( bool dist );

        /// \brief Overwrite this if you need to exchange messages on distributed memory.
        ///  Each graph must implement its own "protocol" Sending
        /// message is done through graph::new_serializer followed by
        /// graph::send_msg / graph::bcast_msg.
        /// \note not needed for regular CnC
        virtual void recv_msg( serializer * );

        /// \brief Get a serializer
        /// Caller must pass it to send/bcast or delete it.
        /// Receiving messages is done in recv_msg callback.
        /// \note not needed for regular CnC
        serializer * new_serializer() const;

        /// \brief send a message (in serializer) to given recipient
        /// Receiving messages is done in recv_msg callback.
        /// \note not needed for regular CnC
        void send_msg( serializer * ser, int rcver ) const;

        /// \brief broadcast message (in serializer) to all (other) ranks/processes
        /// Receiving messages is done in recv_msg callback.
        /// \note not needed for regular CnC
        void bcast_msg( serializer * ser ) const;

        /// \brief broadcast message (in serializer) to given recipients
        /// Receiving messages is done in recv_msg callback.
        /// \note not needed for regular CnC
        bool bcast_msg( serializer *ser , const int * rcvers, int nrecvrs ) const;

        /// \brief Flush a potentially hidden graph.
        /// Usually is a nop.
        /// Our reduction uses this to finalize all pending reductions.
        /// (e.g. when the number of reduced items per reduction cannot be determined).
        /// \note To be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        virtual void flush() {};

    private:
        Internal::context_base & m_context;
    };

    namespace Internal {
        class distributor;
        template< class T > class creator;
        template< class Index, class Functor, class Tuner, typename Increment > class pfor_context;
    }

    /**
       \brief CnC context bringing together collections (for steps, items and tags).
       
       The user needs to derive his or her own context from the CnC::context.
       The template argument to context is the user's context class itself.
       
       For example,
       \code
          struct my_context : public CnC::context< my_context >
          {
             CnC::step_collection< FindPrimes > steps;
             CnC::tag_collection< int > oddNums;
             CnC::item_collection< int,int > primes;
             my_context() 
                 : CnC::context< my_context >(),
                   steps( this ),
                   oddNums( this ),
                   primes( this )
             {
                 oddNums.prescribes( steps );
             }
          };
       \endcode
       
       Several contexts can be created and executed simultaneously.
       
       Execution starts as soon as a step(-instance) is prescribed through putting a tag.
       All ready steps will be executed even if CnC::context::wait() is never be called.
       
       It is recommended to declare collections as members of a context derived from CnC::context.
       This yields more maintanable code and future versions of CnC may require this convention. 
    **/
    template< class Derived >
    class /*CNC_API*/ context : private Internal::context_base
    {
    public:
        /// default constructor
        context();
        /// destructor
        virtual ~context();

        /// \brief wait until all the steps prescribed by this context have completed execution.
        /// \return 0 if succeeded, error code otherwise
        error_type wait();

        /// \brief used with the preschedule tuner to finalize 'gets' in the pre-execution of a step
        ///
        /// Call this after last call to the non-blocking item_collection::unsafe_get method.
        void flush_gets();

        /// reset all collections of this context
        /// \note not thread-safe, to be called in safe state only
        ///       (between program start or calling context::wait() and putting the first tag or item).
        void unsafe_reset();

        /// (distCnC) overload this if default construction on remote processes is not enough.
        virtual void serialize( serializer & ){}
 
    private:
        void init();
        virtual void unsafe_reset( bool );
        context( bool );
        virtual int factory_id();
        template< class Range, class Coll >
        void divide_and_put( Range & range, int grain, Coll * coll, Internal::scheduler_i * sched );
        friend struct ::CnC::debug;
        friend class ::CnC::Internal::distributor;
		friend class ::CnC::tuner_base;
        friend class ::CnC::graph;
        template< typename Tag, bool check_deps, typename Hasher, typename Equality > friend class ::CnC::cancel_tuner;
        template< class T > friend class ::CnC::Internal::creator;
        template< class Index, class Functor, class Tuner, typename Increment > friend class ::CnC::Internal::pfor_context;
        template< typename Tag, typename Tuner > friend class tag_collection;
        template< typename Step, typename Tuner > friend class step_collection;
        template< typename Tag, typename Item, typename Tuner > friend class item_collection;
    };

    /// \brief Execute f( i ) for every i in {first <= i=first+step*x < last and 0 <= x}.
    ///
    /// For different values of i, function execution might occur in parallel.
    /// Returns functor object ocne all iterations have been executed.
    /// Type Index must support operator+(increment) and operator-(Index), like C++ random access iterators do.
    /// Executes on the local process only. No distribution to other processes supported.
    /// \param first  starting index of parallel iteration
    /// \param last   iteration stops before reaching last
    /// \param incr   increment index by this value in each iteration
    /// \param f      function to be executed
    /// \param tuner  defaults to pfor_tuner<>
    template< class Index, class Functor, class Tuner, typename Increment >
    void parallel_for( Index first, Index last, Increment incr, const Functor & f, const Tuner & tuner );
    template< class Index, class Functor, typename Increment >
    void parallel_for( Index first, Index last, Increment incr, const Functor & f );

} // namespace cnc

#include <cnc/internal/step_collection.h>
#include <cnc/internal/tag_collection.h>
#include <cnc/internal/item_collection.h>
#include <cnc/internal/graph.h>
#include <cnc/internal/context.h>
#include <cnc/internal/parallel_for.h>
#include <cnc/internal/hash_item_table.h>
#include <cnc/internal/vec_item_table.h>

#if defined(_MSC_VER) && !defined(__INTEL_COMPILER)
# pragma warning (pop)
# pragma warning( disable : 4355 )
#endif // warnings 4251 4275 4290 are back, 4355 is hidden

#endif // _CnC_H_ALREADY_INCLUDED_
