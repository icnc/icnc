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

#ifndef _CnC_STRIDED_RANGE_H_
#define _CnC_STRIDED_RANGE_H_

namespace CnC {
    namespace Internal {

        /// Represents a half-open range of values with a stride.
        /// e.g. 1:2:10 expands to 1,3,5,7,9
        ///      0:4:8 expands to 0,4 (excluding 8)
        /// This range is compabitble with CnC range concepts.
        /// FIXME: do we want this to be exposed in the API?
        template< class Index, typename Increment = Index >
        class strided_range
        {
        public:
            class const_iterator
            {
            public:
                inline const_iterator( const Index & i, const Increment & s, const Index & e ) : m_val( i ), m_end( e ), m_stride( s ) {}
                inline bool operator==( const const_iterator & i ) const { return m_val == i.m_val; } // || ( m_val > m_end && i.m_val == m_end ) || ( i.m_val > m_end && m_val == m_end ); }
                inline bool operator!=( const const_iterator & i ) const { return m_val != i.m_val; } // && ( ( m_val == m_end && i.m_val < m_end ) || ( i.m_val == m_end && m_val < m_end ) ); }
                inline const Index & operator*() const { return m_val; }
                inline const Index * operator->() const { return &operator*(); }
                inline const_iterator & operator++() { m_val += m_stride; if( m_val > m_end ) m_val = m_end; return *this; }
                inline const_iterator operator++( int ) { const_iterator _tmp = *this; ++(*this); return _tmp; }
                inline operator Index() const { return m_val; }
            private:
                Index m_val;
                Index m_end;
                Increment m_stride;
            };

            typedef Index value_type;
            typedef Increment incr_type;

            strided_range( strided_range & r, tbb::split )
                : m_step( r.m_step )
            {
                value_type _m = r.m_start + ( r.m_last - r.m_start ) / 2;
                value_type _tmp = r.m_start + m_step;
                while( _tmp < _m ) _tmp += m_step;
                _m = _tmp;
                m_start = _m;
                m_last = r.m_last;
                r.m_last = _m;
                init_size();
                CNC_ASSERT( ! empty() );
                r.init_size();
                CNC_ASSERT( ! r.empty() );
            }

            strided_range( value_type start, value_type last, incr_type step )
                : m_start( start ), m_last( last ), m_step( step ){ init_size(); }

            strided_range( value_type start, value_type last )
                : m_start( start ), m_last( last ), m_step( 1 ){ init_size(); }

            strided_range( value_type value )
                : m_start( value ), m_last( value + 1 ), m_step( 1 ), m_size( 1 ) {}

            strided_range() : m_start( 0 ), m_last( 0 ), m_step( 0 ), m_size( 0 ) {}

            bool is_divisible() const { return m_start + m_step < m_last; }
            const_iterator begin() const { return const_iterator( m_start, m_step, m_last ); }
            const_iterator end() const { return const_iterator( m_last, m_step, m_last ); }
            int size() const { return m_size; }
            bool empty() const { return m_size <= 0; }

            operator size_t() const { return m_last + ( m_start << 10 ); }

#ifdef _DIST_CNC_
            void serialize( serializer & ser )
            {
                ser & m_start & m_last & m_step;
            }
#endif

        private:
            int init_size()
            {
                m_size = 0;
                value_type _v = m_start;
                while( _v < m_last ) {
                    ++m_size;
                    _v += m_step;
                } 
                return m_size;
            }

            value_type m_start;
            value_type m_last;
            incr_type  m_step;
            int        m_size;
        };

    } // namespace internal
} // namespace CnC

#endif // _CnC_STRIDED_RANGE_H_
