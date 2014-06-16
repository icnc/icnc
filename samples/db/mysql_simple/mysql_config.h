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

#ifndef _SQL_CONFIG_INCLUDED_
#define _SQL_CONFIG_INCLUDED_

#include <string>

// A DB configuration
struct sql_config
{
    sql_config() {}
    // \param port_    port to attach to mysql
    //                 (e.g. "tcp://127.0.0.1:3306")
    // \param user_    mysql user
    //                 (e.g. "cnc")
    // \param pw_      password for mysql user
    //                 (e.g. "secret")
    // \param db_      schema/database to be used
    //                 (e.g. "items")
    // \param drop_    SQL statement to drop the table, pass "" to keep table
    //                 (e.g. "DROP  TABLE IF EXISTS items;")
    // \param create_  SQL statement to create the table, pass "" to use existing table
    //                 (e.g. "CREATE TABLE If NOT EXISTS items (id INT, value INT);")
    // \param read_    SQL statement to read entire table and pre-fill item-collection, pass "" to ignore existing content
    //                 (e.g. "SELECT id, value FROM items;")
    // \param clear_   SQL statement to clear entire table upon reset, pass "" to avoid clearance
    //                 (e.g. "DELETE * FROM items;")
    // \param get_     prepared SQL statement to select a value with a given tag
    //                 (e.g. "SELECT value FROM items WHERE id=?;")
    // \param put_     prepared SQL statement to insert given tag and value
    //                 (e.g. "INSERT INTO items( id, value ) VALUES ( ?, ? );")
    // \param erase_   prepared SQL statement to erase value for given tag
    //                 (e.g. "DELETE FROM items WHERE id=?;")
    sql_config( const char * port_, const char * user_, const char * pw_, const char * db_,
                const char * drop_, const char * create_, const char * read_, const char * clear_,
                const char * get_, const char * put_, const char * erase_ )
        : port( port_ ), user( user_ ), pw( pw_ ), db( db_ ),
          drop_stmt( drop_ ), create_stmt( create_ ), read_stmt( read_ ), clear_stmt( create_ ),
          get_stmt( get_ ), put_stmt( put_ ), erase_stmt( erase_ )
    {
    }

    std::string port, user, pw, db, drop_stmt, create_stmt, read_stmt, clear_stmt, get_stmt, put_stmt, erase_stmt;
};

#endif // _SQL_CONFIG_INCLUDED_
 
