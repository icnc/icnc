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


/* 
 Attach a database to an item-table.

 We create a very simple graph with one step and one item-collection.
 It doesn't do anything sensible, just a demo.  Of course you can
 write your own application which uses as many DB-attached
 item_collections as you want. Each item-coll keeps its connection and
 config separately.

 We use our sql_tuner to attach to the DB. It uses our wrapper
 sql_item_table.  The sql_config allows configuring the DB (port,
 table, layout etc) as well as how we actually store (or not) the data
 in there (by providing SQL statements).  E.g. we might pre-fill the
 collection at start-up with what's already in the table; or we might
 want to delete everything when attaching to it.

 The program uses mysql and assumes a user "cnc" with pw "cnc" and a
 DB "test".  You can easily adjust with the SQL config below.  Setup
 your mysql accordingly.
*/

#include <cnc/cnc.h>
#include "mysql_tuner.h"

struct my_context;
struct my_sqlstep
{
    int execute( int, my_context & ctxt ) const;
};

struct my_context : public CnC::context< my_context >
{
    // we use the sql_tuner to attach the item-col to a DB
    sql_tuner m_tuner;
    CnC::tag_collection< int > m_tags;
    CnC::item_collection< int, int, sql_tuner > m_items;
    CnC::step_collection< my_sqlstep > m_steps;
    my_context()
        // the argument to the tuner is a config for the DB
        : m_tuner( {"tcp://127.0.0.1:3306",                                  // port
                    "cnc", "cnc",                                            // user, password
                    "test",                                                  // DB/schema,
                    "DROP TABLE IF EXISTS items;",                           // drop statement, set to "" to keep table
                    "CREATE TABLE If NOT EXISTS items (id INT, value INT);", // table creation statement, set to "" to not create table
                    "SELECT id, value FROM items;",                          // read full table statement, set to "" to not read existing entries
                    "DELETE * FROM items;",                                  // clear statements
                    "SELECT value FROM items WHERE id=?;",                   // get statement
                    "INSERT INTO items( id, value ) VALUES ( ?, ? );",       // put statement, set to "" for not putting things into DB
                    "DELETE FROM items WHERE id=?;",                         // erase statement, set to "" to keep things in table upon collection.reset()
                    }),
          m_tags( *this, "tags" ),
          m_items( *this, "items", m_tuner ),
          m_steps( *this, "steps" )
    {
        m_tags.prescribes( m_steps, *this );
        m_steps.consumes( m_items );
        m_steps.produces( m_items );
    }
};

// our step simple gets a piece of data and puts another one
int my_sqlstep::execute( int t, my_context & ctxt ) const
{
    int r;
    ctxt.m_items.get( t-1, r );
    ctxt.m_items.put( t, t+r );
    return 0;
}

// we simply create the graph, put the control tags and the initial data.
// "mysql -u cnc -p test -pcnc -e "select * from items;" shows what has been stored to the table
int main(void)
{
    my_context _ctxt;
    _ctxt.m_items.put( -1, -1 );
    for( int i = 0; i<100; ++i ) {
        _ctxt.m_tags.put( i );
    }
    _ctxt.wait();
    for( int i = 0; i<10; ++i ) {
        int _ir = -1;
        _ctxt.m_items.get( i, _ir );
        std::cout << _ir << " for " << i << std::endl;
    }

    return 0;
}
