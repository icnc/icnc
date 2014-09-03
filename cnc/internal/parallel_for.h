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

#ifndef PARALLEL_FOR_HH_ALREADY_INCLUDED
#define PARALLEL_FOR_HH_ALREADY_INCLUDED

namespace CnC {
    namespace Internal {
        
        /// This is a "dummy" context used only to execute parallel_fors.
        /// The scheduler interface does not have a way to define groups of steps to wait for,
        /// So we need a dedicated scheduler to wait on in order to avoid dead-locks.
        /// A scheduler needs a context but parallel_for does not have one -> create a temporary context.
        /// The actual implementation of parallel for does not involve a tag-collection. Steps are 
        /// directly created from a local step_collection.
        template< class Index, class Functor, class Tuner, typename Increment = Index >
        class pfor_context : public context< pfor_context< Index, Functor, Tuner, Increment > >
        {
        public:
            pfor_context( const Functor & f );
            pfor_context( const Functor & f, const Tuner & t );
#ifdef WIN32
	    pfor_context(); // undefined, must not occur; only to make the compiler happy
#endif
            ~pfor_context();
            const Functor & parallel_for( Index first, Index last, Increment step, const Functor & f );

            // from creatable
            /// parallel_for cannot distribute work, we have no handle on the functor (lambda) which is
            /// uniquely identifyable on different processes. Future implementations might make this possible.
            virtual int factory_id() { CNC_ABORT( "pfor_context::factoryId should never get called." ); return 0; }

        private:
            typedef pfor_context< Index, Functor, Tuner, Increment > context_type;
            typedef Internal::strided_range< Index, Increment >      range_type;
            class functor_step;
            struct tuner_type;
            typedef step_collection< functor_step, tuner_type > step_coll_type;
            typedef Internal::step_launcher< Index, functor_step, context_type, tuner_type, tuner_type > step_launcher_type;

            const Tuner        & m_tuner;
            step_coll_type       m_stepColl;
            step_launcher_type   m_stepLauncher;
        };

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Index, class Functor, class Tuner, typename Increment >
        class pfor_context< Index, Functor, Tuner, Increment >::functor_step
        {
        public:
            functor_step( const Functor & f )
                : m_func( f )
            {}
            
            int execute( const Index & t, context_type & /*ctxt*/ ) const
            {
                m_func( t );
                return CNC_Success;
            }
            
        private:
            const Functor & m_func;
        };


        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        /// a tuner that never distributes work
        template< class Index, class Functor, class Tuner, typename Increment >
        struct pfor_context< Index, Functor, Tuner, Increment >::tuner_type : public Tuner
        {
            tuner_type(){}
            tuner_type( const Tuner & t ) : Tuner( t ) {}
            // no compute_on
            template< class Tag, class Arg >
            int compute_on( const Tag & /*tag*/, Arg & /*arg*/ ) const
            {
                return COMPUTE_ON_LOCAL;
            }
            // no cancelation
            template< typename Tag, typename Arg >
            int was_canceled( const Tag & /*tag*/, Arg & /*arg*/ ) const
            {
                return false;
            }
            // no sequential execution
            template< typename Tag, typename Arg >
            bool sequentialize( const Tag & /*tag*/, Arg & /*arg*/ ) const
            {
                return false;
            }

            typedef typename pfor_context< Index, Functor, Tuner, Increment >::range_type range_type;
            typedef Internal::no_tag_table tag_table_type;
        };
        
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifndef  __GNUC__
# pragma warning (push)
# pragma warning (disable: 4355)
#endif
        template< class Index, class Functor, class Tuner, typename Increment >
        pfor_context< Index, Functor, Tuner, Increment >::pfor_context( const Functor & f )
            : context< pfor_context< Index, Functor, Tuner, Increment > >( true ),
              m_tuner( get_default_tuner< Tuner >() ),
              m_stepColl( *this, "pfor", f, m_tuner ),
              m_stepLauncher( *this, *this, m_stepColl, m_tuner, this->scheduler() )
        {
            // henn and egg, we need a scheduler to create the step collection,
            // and we need a context to create the scheduler.
            // -> this must be a pointer and explicit allocation with the scheduler of the parent
            //            m_stepLauncher = new step_launcher_type( *this, *this, m_stepColl, this->scheduler() );
        }

        template< class Index, class Functor, class Tuner, typename Increment >
        pfor_context< Index, Functor, Tuner, Increment >::pfor_context( const Functor & f, const Tuner & t )
            : context< pfor_context< Index, Functor, Tuner, Increment > >( true ),
              m_tuner( t ),
              m_stepColl( *this, "pfor", f, m_tuner ),
              m_stepLauncher( *this, *this, m_stepColl, m_tuner, this->scheduler() )
        {
            // henn and egg, we need a scheduler to create the step collection,
            // and we need a context to create the scheduler.
            // -> this must be a pointer and explicit allocation with the scheduler of the parent
            //            m_stepLauncher = new step_launcher_type( *this, *this, m_stepColl, this->scheduler() );
        }
#ifndef  __GNUC__
# pragma warning (pop)
#endif

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Index, class Functor, class Tuner, typename Increment >
        pfor_context< Index, Functor, Tuner, Increment >::~pfor_context()
        {
            //            delete m_stepLauncher;
        }

        // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

        template< class Index, class Functor, class Tuner, typename Increment >
        const Functor & pfor_context< Index, Functor, Tuner, Increment >::parallel_for( Index first, Index last, Increment step, const Functor & f )
        {
            range_type   _range( first, last, step );
            //            int          _sz = _range.size();
            
            //            tagged_step_instance< range_type > * _si = 
            (void) m_stepLauncher.create_range_step_instance( _range, *this );
            context_base::wait();
            
            return f;
        }
    }
    
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    template< class Index, class Functor, typename Increment >
    void parallel_for( Index first, Index last, Increment step, const Functor & f )
    {
        Internal::pfor_context< Index, Functor, pfor_tuner<>, Increment > _ctxt( f );
        _ctxt.parallel_for( first, last, step, f );
    }

    template< class Index, class Functor, class Tuner, typename Increment >
    void parallel_for( Index first, Index last, Increment step, const Functor & f, const Tuner & tuner )
    {
        Internal::pfor_context< Index, Functor, Tuner, Increment > _ctxt( f, tuner );
	_ctxt.parallel_for( first, last, step, f );
    }

} // namspace CnC

#endif // PARALLEL_FOR_HH_ALREADY_INCLUDED
