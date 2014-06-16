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
  hashing and comparing tags and items.
*/

#ifndef _TAG_HASH_COMPARE_HH_ALREADY_INCLUDED
#define _TAG_HASH_COMPARE_HH_ALREADY_INCLUDED

#include <string>
#include <vector>
#include <cnc/internal/cnc_stddef.h>
#include <cstring>
#include <tbb/concurrent_hash_map.h>

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/// \brief Provides hash operators for hashing
/// 
/// Specializations for custom data types must implement
/// hash and equal to work with preserving tag-collections (CnC::preserve_tuner)
/// and/or (default) CnC::item_collection using hashmaps.
///
/// Standard data types are supported out-of-the-box as
/// well as std::vector and std:pair thereof, char*, std::string
/// and pointers (which you better avoid if ever possible, because it
/// pointers as tags are not portable, e.g. to distributed memory).
/// \return a unique integer for the given tag
template< typename T >
struct cnc_hash : public tbb::tbb_hash< T >
{ 
};

/// \brief Provides equality operators for hashing
/// 
/// Specializations for custom data types must implement
/// hash and equal to work with preserving tag-collections (CnC::preserve_tuner)
/// and/or (default) CnC::item_collection using hashmaps.
///
/// Standard data types are supported out-of-the-box as
/// well as std::vector and std:pair thereof, char*, std::string
/// and pointers (which you better avoid if ever possible, because it
/// pointers as tags are not portable, e.g. to distributed memory).
/// \return true if a equals b, false otherwise
template< typename T >
struct cnc_equal : public std::equal_to< T >
{
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// \brief hash for pointer types, but you better avoid tags being pointers
template< class T >
struct cnc_hash< T* >
{
    size_t operator()( const T * x ) const
    { 
        return reinterpret_cast< size_t >( x ) * 2654435761;
    }
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// \brief hash for const char *
template<>
struct cnc_hash< char * >
{
    size_t operator()( const char * x ) const
    {
        // suggested by Paul Larson of Microsoft Research
        size_t _n = 0;
        while ( *x ) {
            _n = _n * 101  +  *x;
            ++x;
        }
        return _n;
    }
};

// \brief equality for const char *
template<>
struct cnc_equal< char * >
{
    bool operator()( const char * x, const char * y ) const
    {
        return ( strcmp( x, y ) == 0 );
    }
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

/// \brief hash for std::string
template<>
struct cnc_hash< std::string >
{
    size_t operator()( const std::string & x ) const
    {
        // suggested by Paul Larson of Microsoft Research
        size_t _n = 0;
        for( std::string::const_iterator i = x.begin(); i != x.end(); ++ i ) {
            _n = _n * 101  +  *i;
        }
        return _n;
    }
};

/// \brief equality for std::string
template<>
struct cnc_equal< std::string >
{
    bool operator()( const std::string & a, const std::string & b ) const
    {
        return ( a.compare( b ) == 0 );
    }
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// \brief hash/equality for std::vector
template< class T, class Allocator >
struct cnc_hash< std::vector< T, Allocator > >
{
public:
    size_t operator()( const std::vector< T, Allocator > & x ) const
    {
        size_t _n = x.size();
        CNC_ASSERT( _n > 0 );
        cnc_hash< T > _hasher;
        switch( _n ) {
            case 0 : return 0;
            case 1 : return _hasher.operator()( x[0] );
            case 2 : return ( _hasher.operator()( x[0] )
                              + ( _hasher.operator()( x[1] ) << 10 ) );;
            case 3 : return ( _hasher.operator()( x[0] )
                               + ( _hasher.operator()( x[1] ) << 9 )
                               + ( _hasher.operator()( x[2] ) << 18 ) );
            case 4 : return ( _hasher.operator()( x[0] )
                               + ( _hasher.operator()( x[3] ) << 8 )
                               + ( _hasher.operator()( x[1] ) << 16 )
                               + ( _hasher.operator()( x[2] ) << 24 ) );
            default : {
                size_t _n = 0;
                for( typename std::vector< T, Allocator >::const_iterator i = x.begin(); i != x.end(); ++ i ) {
                    _n = _n * 3  + _hasher.operator()( *i );
                }
                return _n;
            }
        }
    }
};

// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


/// \brief Provides hash and equality operators for hashing as used by item_collections
///
/// It is recommended to specilialize cnc_hash and/or cnc_equal rather than cnc_tag_hash_compare
template< class T >
struct cnc_tag_hash_compare : public cnc_hash< T >, public cnc_equal< T >
{
    /// \return a unique integer for the given tag
    size_t hash( const T & x ) const
    { 
        return cnc_hash< T >::operator()( x );
    }
    /// \return true if a equals b, false otherwise
    bool equal( const T & x, const T & y ) const
    {
        return cnc_equal< T >::operator()( x, y );
    }
};

#endif // _TAG_HASH_COMPARE_HH_ALREADY_INCLUDED
