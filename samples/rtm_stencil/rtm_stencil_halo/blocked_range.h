//********************************************************************************
// Copyright (c) 2010-2014 Intel Corporation. All rights reserved.              **
//                                                                              **
// Redistribution and use in source and binary forms, with or without           **
// modification, are permitted provided that the following conditions are met:  **
//   * Redistributions of source code must retain the above copyright notice,   **
//     this list of conditions and the following disclaimer.                    **
//   * Redistributions in binary form must reproduce the above copyright        **
//     notice, this list of conditions and the following disclaimer in the      **
//     documentation and/or other materials provided with the distribution.     **
//   * Neither the name of Intel Corporation nor the names of its contributors  **
//     may be used to endorse or promote products derived from this software    **
//     without specific prior written permission.                               **
//                                                                              **
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************

#ifndef _CnC_BLOCKED_RANGE_H_
#define _CnC_BLOCKED_RANGE_H_

#include <array>
#include <cnc/internal/cnc_stddef.h>
#include <tbb/tbb_stddef.h>

template< int DIM, class T = int >
class blocked_range
{
public:
    typedef std::array< T, DIM > value_type;
    blocked_range( int gs = 1 );
    blocked_range( const blocked_range< DIM, T > & o );
    blocked_range( blocked_range< DIM, T > & o, tbb::split );
    int size() const;
    void set_dim( int dim, T s, T e );
    void set_grain( int dim, int gs );
    class const_iterator;
    const_iterator begin() const;
    const const_iterator end() const;
    bool is_divisible() const;
    operator size_t() const;
    size_t get_index( const value_type ) const;
#ifdef _DIST_
    void serialize( CnC::serializer & ser );
#endif

private:
    int calc_size() const;
    value_type m_start;
    value_type m_end;
    std::array< T, DIM+1 > m_grain;
    int        m_size;
    blocked_range< DIM, T > * m_root;
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
class blocked_range< DIM, T >::const_iterator
{
    friend class blocked_range< DIM, T >;

public:
    const_iterator( const blocked_range< DIM, T > * br = NULL )
        : m_range( br ),
          m_pos()
    {
        if( m_range ) {
            m_pos = m_range->m_start;
        }
    }

    const const_iterator & operator=( const const_iterator & o )
    {
        m_range = o.m_range;
        if( m_range ) {
            m_pos = m_range->m_start;
        }
    }

    bool operator==( const const_iterator & o ) const
    {
        return m_range == o.m_range && m_pos == o.m_pos;
    }

    bool operator!=( const const_iterator & o ) const
    {
        return m_range != o.m_range || m_pos != o.m_pos;
    }

    const value_type & operator*() const
    {
        return m_pos;
    }

    const value_type * operator->() const
    {
        return &operator*();
    }

    const_iterator & operator++()
    {
        CNC_ASSERT_MSG( m_pos[0] < m_range->m_end[0], "Cannot advance const_iterator beyond end of blocked_range" );
        for( int i = DIM - 1; i >= 0; --i ) {
            if( m_pos[i] < m_range->m_end[i] - 1 ) {
                ++ m_pos[i];
                return *this;
            } else m_pos[i] = m_range->m_start[i];
        }
        m_pos = m_range->m_end;
        return *this;
    }

    const_iterator operator++( int )
    {
        const_iterator _tmp = *this;
        ++(*this);
        return _tmp;
    }

    // operator blocked_range< DIM, T >() const
    // {
    //     blocked_range< DIM, T > _tmp;
    //     for( int i = 0; i < DIM; ++i ) {
    //         _tmp.set_dim( i, m_pos[i], m_pos[i]+1 );
    //     }
    //     return _tmp;
    // }

private:
    const blocked_range< DIM, T > * m_range;
    value_type                      m_pos;
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
template< int DIM, class T >
blocked_range< DIM, T >::blocked_range( int gs )
    : m_start(),
      m_end(),
      m_grain(),
      m_size( 0 ),
      m_root( this )
{
    //    int _tmp = (int) pow( gs, 1.0/DIM );
    for( int i = 0; i < DIM; ++i ) {
        m_grain[i] = 1; //_tmp;
    }
    m_grain[DIM] = gs;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
blocked_range< DIM, T >::blocked_range( const blocked_range< DIM, T > & o )
    : m_start( o.m_start ),
      m_end( o.m_end ),
      m_grain( o.m_grain ),
      m_size( o.m_size ),
      m_root( o.m_root )
{
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
template< int DIM, class T >
blocked_range< DIM, T >::blocked_range( blocked_range< DIM, T > & o, tbb::split )
    : m_start( o.m_start ),
      m_end( o.m_end ),
      m_grain( o.m_grain ),
      m_size( 0 ),
      m_root( o.m_root )
{
    CNC_ASSERT( o.size() > m_grain[DIM] );

    T _mx_cut = 0;
    int _mx_i = -1;

    for( int i = 0; i < DIM; ++i ) {
        T _tmp_d = m_end[i] - m_start[i];
        // Can we do a split here?
        if( m_grain[i] < _tmp_d ) {
            // cut in half and align with min grain along this dimension
            T _tmp_cut = ( ( ( ( _tmp_d + 1 ) / 2 ) + m_grain[i] - 1 ) / m_grain[i] ) * m_grain[i];
            // we use this dimension if the resulting cut is larger
            if( ( _tmp_cut > _mx_cut )  ) {
                _mx_cut = _tmp_cut;
                _mx_i   = i;
            }
        }
    }

    // T _mx_d = 0;
    // float _mx_bal = 99999;
    // int _mx_i = -1;
    // // find l
    // for( int i = 0; i < DIM; ++i ) {
    //     T _tmp_d = m_end[i] - m_start[i];
    //     // Can we do a split here?
    //     if( m_grain[i] < _tmp_d ) {
    //         // cut in half and align with min grain along this dimension
    //         T _tmp_cut = ( ( ( ( _tmp_d + 1 ) / 2 ) + m_grain[i] - 1 ) / m_grain[i] ) * m_grain[i];
    //         // calc balance, _tmp_cut is never smaller than the other side of the cut
    //         CNC_ASSERT( _tmp_cut >= ( _tmp_d - _tmp_cut ) && _tmp_cut <= _tmp_d );
    //         float _tmp_bal = ((float)_tmp_cut) / ((float)( _tmp_d - _tmp_cut ));
    //         //            std::cout << "\t dim " << i << ": " << _tmp_cut << " " << _tmp_bal << std::endl;
    //         // we use this dimension if the resulting partitioning has a better balance
    //         if( ( _tmp_d > 3 * _mx_d ) || _tmp_bal < _mx_bal || ( _tmp_bal == _mx_bal && _tmp_cut > _mx_cut )  ) {
    //             _mx_cut = _tmp_cut;
    //             _mx_d   = _tmp_d;
    //             _mx_bal = _tmp_bal;
    //             _mx_i   = i;
    //         }
    //     }
    // }
    CNC_ASSERT_MSG( _mx_cut > 0, "requested partition size cannot be achieved with given granularity-limits on individual dimensions." );
    //    std::cout << "Cutting dim " << _mx_i << ": " << _mx_cut << " " << std::endl;
    // now split at best dimension
    m_start[_mx_i] += _mx_cut;
    o.m_end[_mx_i] = m_start[_mx_i];
    m_size = calc_size();
    o.m_size = o.calc_size();
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
blocked_range< DIM, T >::operator size_t() const
{
    return get_index( m_start );
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
size_t blocked_range< DIM, T >::get_index( const typename blocked_range< DIM, T >::value_type tup ) const
{
    size_t _idx( tup[DIM-1] );
    size_t _f = 1;
    for( int i = DIM-2; i >= 0; --i ) {
        _f *= ( m_root->m_end[i] - m_root->m_start[i] );
        _idx += ( tup[i] - m_root->m_start[i] ) * _f;
    }
    return _idx;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
int blocked_range< DIM, T >::calc_size() const
{
    int _sz( m_end[0] - m_start[0] );
    for( int i = 1; i < DIM; ++i ) {
        _sz *= m_end[i] - m_start[i];
    }
    return _sz;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
int blocked_range< DIM, T >::size() const
{
    CNC_ASSERT( m_size == calc_size() );
    return m_size;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
void blocked_range< DIM, T >::set_dim( int dim, T s, T e )
{
    CNC_ASSERT( dim < DIM );
    m_start[dim] = s;
    m_end[dim]   = e;
    m_size = calc_size();
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
void blocked_range< DIM, T >::set_grain( int dim, int gs )
{
    CNC_ASSERT( dim < DIM );
    m_grain[dim] = gs;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
typename blocked_range< DIM, T >::const_iterator blocked_range< DIM, T >::begin() const
{
    return const_iterator( this );
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
const typename blocked_range< DIM, T >::const_iterator blocked_range< DIM, T >::end() const
{
    const_iterator _e( this );
    for( int i = 0; i < DIM; ++i ) {
        _e.m_pos[i] = m_end[i];
    }
    return _e;
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

template< int DIM, class T >
bool blocked_range< DIM, T >::is_divisible() const
{
    if( size() <= m_grain[DIM] ) return false;
    for( int i = 0; i < DIM; ++i ) {
        if( m_end[i] - m_start[i] > m_grain[i] ) return true;
    }
    return false;        
}

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#ifdef _DIST_
// namespace std {
//     // template< class T, size_t N >
//     // inline CnC::bitwise_serializable serializer_category( const std::array< T, N > * ) {
//     //     return CnC::bitwise_serializable();
//     // }
//     template< class T, size_t N >
//     inline CnC::bitwise_serializable serializer_category( const std::tr1::array< T, N > * ) {
//         return CnC::bitwise_serializable();
//     }
// }
// namespace CnC {
//     template< class T, size_t N >
//     inline void serialize( CnC::serializer & ser, std::array< T, N > & a ) {
//         T * _tmp = a.data();
//         ser & CnC::chunk< T, CnC::no_alloc >( _tmp, N );
//     }
// }

template< int DIM, class T >
void blocked_range< DIM, T >::serialize( CnC::serializer & ser )
{
    ser & m_start & m_end & m_grain &  m_size;
}
#endif

#endif // _CnC_BLOCKED_RANGE_H_
