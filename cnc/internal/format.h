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

#ifndef FORMAT_HH_ALREADY_INCLUDED
#define FORMAT_HH_ALREADY_INCLUDED

#include <string>
#include <vector>
#include <ostream>

/// cnc_format is used to print tag and item objects in error messages
/// and tracing output. Standard types are supported out-of-the-box.
/// For custom data types you must implement your own cnc_format specialization.
template< typename T >
inline std::ostream & cnc_format( std::ostream & o, const T & t )
{
    return o << "UnformattedType at " << &t;
}

template< typename T >
inline std::ostream & cnc_format( std::ostream & o, const T * t )
{
    return o << std::hex << t;
}

template<>
inline std::ostream & cnc_format< int >( std::ostream & o, const int & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< unsigned int >( std::ostream & o, const unsigned int & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< long >( std::ostream & o, const long & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< unsigned long >( std::ostream & o, const unsigned long & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< long long >( std::ostream & o, const long long & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< unsigned long long >( std::ostream & o, const unsigned long long & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< short >( std::ostream & o, const short & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< unsigned short >( std::ostream & o, const unsigned short & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< char * >( std::ostream & o, char * const & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< char >( std::ostream & o, const char * t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< unsigned char >( std::ostream & o, const unsigned char & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< double >( std::ostream & o, const double & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< long double >( std::ostream & o, const long double & t )
{
    return o << t;
}

template<>
inline std::ostream & cnc_format< float >( std::ostream & o, const float & t )
{
    return o << t;
}


//namespace std {

inline std::ostream & cnc_format( std::ostream & o, const std::string & t )
    {
        return o << t;
    }

    template< class A, class B >
    inline std::ostream & cnc_format( std::ostream& os, const std::pair< A, B > & p )
    {
        os << "(";
        cnc_format( os, p.first );
        os << ",";
        cnc_format( os, p.second );
        return os << ")";
    }

    // now some help with vectors
    template< class T, class Allocator >
    inline std::ostream & cnc_format( std::ostream& os, const std::vector< T, Allocator > & x )
    {
        os << "[";
        for( typename std::vector< T, Allocator >::const_iterator i = x.begin(); i != x.end(); ++ i ) {
            cnc_format( os, *i );
            os << ",";
        }
         return os << "]";
    }
//}

#include <tbb/blocked_range.h>

    template< class T >
    inline std::ostream & cnc_format( std::ostream & os, const tbb::blocked_range< T > & x )
    {
        os << "[";
        if( ! x.empty() ) os << x.begin() << ", " << x.end();
        return os << "[";
    }

#endif // FORMAT_HH_ALREADY_INCLUDED
