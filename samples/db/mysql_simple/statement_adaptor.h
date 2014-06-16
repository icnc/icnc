//********************************************************************************
// Copyright (c) 2014-2014 Intel Corporation. All rights reserved.              **
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
// This software is provided by the copyright holders and contributors "as is"  **
// and any express or implied warranties, including, but not limited to, the    **
// implied warranties of merchantability and fitness for a particular purpose   **
// are disclaimed. In no event shall the copyright owner or contributors be     **
// liable for any direct, indirect, incidental, special, exemplary, or          **
// consequential damages (including, but not limited to, procurement of         **
// substitute goods or services; loss of use, data, or profits; or business     **
// interruption) however caused and on any theory of liability, whether in      **
// contract, strict liability, or tort (including negligence or otherwise)      **
// arising in any way out of the use of this software, even if advised of       **
// the possibility of such damage.                                              **
//********************************************************************************

#ifndef _SQL_STATEMENT_ADAPTOR_INCLUDED_
#define _SQL_STATEMENT_ADAPTOR_INCLUDED_

#include <cppconn/prepared_statement.h>

// The lowest level adaptor to get/set values from/into SQL prepared statements.
// You might need to write your a specialization for your own datatype.
template< typename T >
struct statement_adaptor
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const T & val )
    {
        assert( "Unsupported type for putting values into SQL DB" == NULL );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, T & val )
    {
        assert( "Unsupported type for getting values into SQL DB" == NULL );
    }
};

template<>
struct statement_adaptor< bool >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const bool & val )
    {
        pstmt->setBoolean( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, bool & val )
    {
        val = rset->getBoolean( idx );
    }
};

template<>
struct statement_adaptor< std::string >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const std::string & val )
    {
        pstmt->setString( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, std::string & val )
    {
        val = rset->getString( idx );
    }
};

template<>
struct statement_adaptor< sql::SQLString >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const sql::SQLString & val )
    {
        pstmt->setString( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, sql::SQLString & val )
    {
        val = rset->getString( idx );
    }
};

template<>
struct statement_adaptor< double >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const double & val )
    {
        pstmt->setDouble( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, double & val )
    {
        val = rset->getDouble( idx );
    }
};

template<>
struct statement_adaptor< int32_t >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const int32_t & val )
    {
        pstmt->setInt( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, int32_t & val )
    {
        val = rset->getInt( idx );
    }
};

template<>
struct statement_adaptor< uint32_t >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const uint32_t & val )
    {
        pstmt->setUInt( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, uint32_t & val )
    {
        val = rset->getUInt( idx );
    }
};

template<>
struct statement_adaptor< int64_t >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const int64_t & val )
    {
        pstmt->setInt64( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, int64_t & val )
    {
        val = rset->getInt64( idx );
    }
};

template<>
struct statement_adaptor< uint64_t >
{
    static void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const uint64_t & val )
    {
        pstmt->setUInt64( idx, val );
    }
    static void getVal( sql::ResultSet * rset, unsigned int idx, int64_t & val )
    {
        val = rset->getInt64( idx );
    }
};

// ***********************************************************************************************
// ***********************************************************************************************


// template<>
// void setVal( sql::PreparedStatement * pstmt, unsigned int idx, const std::istream * val )
// {
// 	pstmt->setBlob(idx, val);
// }

// template<>
// void getVal( sql::ResultSet * rset, unsigned int idx, const std::istream * val )
// {
// 	rset->getBlob(idx, val);
// }

#endif // _SQL_STATEMENT_ADAPTOR_INCLUDED_
