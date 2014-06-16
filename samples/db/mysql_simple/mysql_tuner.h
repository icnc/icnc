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

#ifndef _SQL_TUNER_INCLUDED_
#define _SQL_TUNER_INCLUDED_

#include "mysql_item_table.h"
#include <cnc/default_tuner.h>

/// \brief The tuner base for hashmap-based item-tuners with
///        additional storage in SQL
///
/// It defines sql_item_table as the item_table when deriving
/// from CnC::item_tuner (in the template argument).
///
/// A SQL-specific tuning-capability decides per item instance if it
/// should be stored in the DB or not. It will always be stored in 
/// the CnC hashmap. See toDB(tag) below.
///
/// The sql_item_table itself uses a 3-level adaptor hierachy
/// to convert between CnC and SQL (sql_table, sql_adaptor and statement_adaptor).
/// This might sound complex, but it actually makes adjustments to your
/// own types and layout simpler.
///
/// See sql_table, sql_adaptor and statement_adaptor on ways
/// to customize to your data types and table layouts etc.
///
/// The internal hash-map uses cnc_tag_hash_compare. If your
/// tag-type is not supported by default, you need to provide a
/// template specialization for it.
struct sql_tuner : public CnC::item_tuner< sql_item_table >
{
    template< typename T, typename I, typename A >
    void init_table( sql_item_table< T, I, A > & sitbl ) const
    {
        { CnC::Internal::Speaker oss( std::cout ); oss << "initing tuner at " << m_config.port;}
        sitbl.init( m_config );
    }
    sql_tuner() {}
    sql_tuner( const sql_config & config ) : m_config( config ) {}
    // Should given instance get stored in the DB?
    // Currently we just check for the availabilty of the put-SQL statment.
    // If it exists, all items will be stored in the DB.
    // We can easily change this to decide individually per instance
    // by using the tag argument in a conditional statement.
    template< typename T >
    bool toDB( const T & ) const
    {
        return m_config.put_stmt.size() > 0;
    }
    const sql_config m_config;
};

#endif // _SQL_TUNER_INCLUDED_
