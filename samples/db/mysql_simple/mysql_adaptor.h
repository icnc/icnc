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

#ifndef _SQL_ADAPTOR_INCLUDED_
#define _SQL_ADAPTOR_INCLUDED_

#include "statement_adaptor.h"
#include <cppconn/resultset.h>

// Mid-level adapator needed to "translate" CnC semantics into SQL semantics:
// hash_all, setForPut, setForGet, get, setForErase
// This implementations uses the statement_adaptor to set/get fundamental data types.
// If you're using your own types, you will need to provide statement_adaptor specializations for them.
template< typename Tag, typename ItemT >
struct sql_adaptor
{
    // Must call hashser(tag,item) for every "intersting" tag/value pair in given resultset.
    // called when pre-filling item-collection when attaching to a SQL table,
    template< typename ItemHasher >
    void hash_all( sql::ResultSet * res, const ItemHasher & hasher ) const
    {
        res->beforeFirst();
        Tag _tag;
        ItemT _item;
        while( res->next() ) {
            statement_adaptor< Tag >::getVal( res, 1, _tag );
            statement_adaptor< ItemT >::getVal( res, 2, _item );
            hasher( _tag, _item );
        }
    }
    
    // Set the tag and value on the prepared SQL statement.
    // Called when putting/writing a new tag/value pair into a SQL table.
    void setForPut( sql::PreparedStatement * pstmt, const Tag & tag, const ItemT & item ) const
    {
        statement_adaptor< Tag >::setVal( pstmt, 1, tag );
        statement_adaptor< ItemT >::setVal( pstmt, 2, item );
    }

    // Set the tag on the prepared SQL statement.
    // Called when getting/reading a new tag/value pair from a SQL table.
    void setForGet( sql::PreparedStatement * pstmt, const Tag & tag ) const
    {
        statement_adaptor< Tag >::setVal( pstmt, 1, tag);
    }
    
    // Get the value from a result set.
    // Called when reading a single tag/value pair from SQL table.
    // The resultset might have more than one value, but it can use only one of them.
    // We just use the last one. Change it if that's not what you want.
    void get( sql::ResultSet * res, ItemT & item ) const
    {
        res->afterLast();
        res->previous();
        getVal( res, 1, item );
    }

    // Set the tag on the prepared SQL statement.
    // Called when erasing a tag/value pair from a SQL table.
    void setForErase( sql::PreparedStatement * pstmt, const Tag & tag ) const
    {
        statement_adaptor< Tag >::setVal( pstmt, 1, tag );
    }
};

#endif // _SQL_ADAPTOR_INCLUDED_
