//********************************************************************************
// Copyright (c) 2007-2014 Intel Corporation. All rights reserved.              **
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

#ifndef _SQL_ITEM_TABLE_INCLUDED_
#define _SQL_ITEM_TABLE_INCLUDED_

#include <stdlib.h>
#include <iostream>
#include <string>
#include <cassert>

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/statement.h>

#include <tbb/enumerable_thread_specific.h>

#include "mysql_config.h"
#include "mysql_adaptor.h"

#define thread_local __thread

template< typename Tag, typename ItemT, typename Coll > class sql_item_table;

// The higher-level features needed to attach a CnC item-collection to a DB:
// init, put, get, erase and clear
// For some data-types and/or DB layouts you might need to change this.
// In many cases it will be sufficient to work with the mid-level Adaptor (see "sql_adaoptor.h")
// or even the low-level statement_adaptor ("statement_adaptor.h") if it's just about a custom data type.
template< typename Adaptor >
class sql_table
{
public:
    sql_table( const Adaptor & a )
        : m_connection( []()->sql::Connection*{return NULL;} ),
          m_adaptor( a )
    {}

    template< typename ItemHasher >
    void init( const sql_config & config, ItemHasher hasher  )
    {
        { CnC::Internal::Speaker oss( std::cout ); oss << "initing table at " << config.port;}
        m_config = config;
        
        try {
            connect();
            
            sql::Statement * _stmt = m_connection.local()->createStatement();
            // drop table?
            if( m_config.drop_stmt.size() ) {
                { CnC::Internal::Speaker oss( std::cout ); oss << "dropping table";}
                _stmt->execute( m_config.drop_stmt );
            }
            // create table?
            if( m_config.create_stmt.size() ) {
                { CnC::Internal::Speaker oss( std::cout ); oss << "creating table";}
                _stmt->execute( m_config.create_stmt );
            }
            // should we read in?
            if( m_config.read_stmt.size() ) {
                { CnC::Internal::Speaker oss( std::cout ); oss << "reading table";}
                sql::ResultSet * _res = _stmt->executeQuery( m_config.read_stmt );
                m_adaptor.hash_all( _res, hasher );
            }
            delete _stmt;
        } catch( sql::SQLException & e ) {
            std::cerr << "Exception caught when initializing SQL table.\n";
        }
    }

    // a item-collection::put was called, store it in DB?
    template< typename Tag, typename ItemT >
    bool put( const Tag & tag, const ItemT & item )
    {
        if( m_config.put_stmt.size() ) {
            try {
                if( m_connection.local() == NULL ) connect();
                sql::PreparedStatement * _pstmt = m_connection.local()->prepareStatement( m_config.put_stmt );
                m_adaptor.setForPut( _pstmt, tag, item );
                _pstmt->executeUpdate();
                delete _pstmt;
            } catch( sql::SQLException & e ) {
                return false;
            }
        }
        return true;
    }
    template< typename Tag, typename ItemT >
    
    // a item-collection::get was called, get it from DB?
    bool get( const Tag & tag, ItemT & item )
    {
        if( m_config.get_stmt.size() ) {
            try {
                if( m_connection.local() == NULL ) connect();
                sql::PreparedStatement * _pstmt = m_connection.local()->prepareStatement( m_config.get_stmt );
                m_adaptor.setForGet( _pstmt, tag );
                sql::ResultSet * _res = _pstmt->executeQuery();
                m_adaptor.get( _pstmt, item );
                delete _res;
                delete _pstmt;
            } catch( sql::SQLException & e ) {
                return true;
            }
        }
        return false;
    }

    // a item-collection::erase was called, erase it from DB?
    template< typename Tag >
    void erase( const Tag & tag )
    {
        if( m_config.erase_stmt.size() ) {
            try {
                if( m_connection.local() == NULL ) connect();
                sql::PreparedStatement * _pstmt = m_connection.local()->prepareStatement( m_config.erase_stmt );
                m_adaptor.setForErase( _pstmt, tag );
                _pstmt->executeUpdate();
                delete _pstmt;
            } catch( sql::SQLException & e ) {
                std::cerr << "Exception caught when erasing item from SQL table.\n";
            }
        }
    }

    // item_collection::reset was called, clear DB table?
    void clear()
    {
        if( m_config.clear_stmt.size() ) {
            try {
                if( m_connection.local() == NULL ) connect();
                sql::Statement * _stmt = m_connection.local()->createStatement();
                _stmt->execute( m_config.clear_stmt );
                delete _stmt;
            } catch( sql::SQLException & e ) {
                std::cerr << "Exception caught when clearing SQL table.\n";
            }
        }
    }
    
private:
    // return a thread-local connection
    void connect()
    {
        try {
            sql::Driver * _driver = get_driver_instance();
            // connect to the db server
            sql::Connection *& _conn = m_connection.local();
            _conn = _driver->connect( m_config.port, m_config.user, m_config.pw );
            // use the desired database
            _conn->setSchema( m_config.db );
        } catch( sql::SQLException & e ) {
            std::cerr << "Exception caught when connecting to DB server.\n";
        }
    }
    sql_config m_config;
    typedef tbb::enumerable_thread_specific< sql::Connection * > connection_type;
    connection_type m_connection;
    const Adaptor m_adaptor;
};

#include <cnc/internal/hash_item_table.h>
#include <tbb/tbb_thread.h>

/// Stores items in a hash_table and in a SQL table.
/// Implements the required interface to be used by item_collection_base.
/// Derives from hash_item_table.
/// We never get from the DB table, because we always store in the hashmap as well.
/// Getting from the DB makes sense only if we support not storing items in the hash-map.
///
/// We use our sql_adaptor which is pretty generic already.
/// Adjusting to items/values which represent sets of values, you might want
/// to write your own and/or extend it.
template< typename Tag, typename ItemT, typename Coll >
class sql_item_table : public CnC::Internal::hash_item_table< Tag, ItemT, Coll >
{
    typedef CnC::Internal::hash_item_table< Tag, ItemT, Coll > parent;
public:
    sql_item_table( const Coll * c, int )
        : m_coll( c ),
          m_sqlTable( sql_adaptor< Tag, ItemT >() )
    {}

    void init( const sql_config & config )
    {
        { CnC::Internal::Speaker oss( std::cout ); oss << "initing item table at " << config.port;}
        m_sqlTable.init( config, [=]( const Tag & t, const ItemT & i ) { this->hash_item( t, i ); } );
    }

    /// Puts an item into table if not present and acquires the accessor/lock for it.
    /// \return true if inserted, false if already there
    bool put_item( const Tag & t, const ItemT * i, const int getcount, const int owner, typename parent::accessor & a )
    {
        if( parent::put_item( t, i, getcount, owner, a ) ) {
            if( m_coll->tuner().toDB( t ) ) {
                while( ! m_sqlTable.put( t, *i ) ) tbb::this_tbb_thread::yield();
            }
            return true;
        }
        return false;
    }

    /// erase an entry identified by accessor.
    ItemT * erase( typename parent::accessor & a )
    {
        m_sqlTable.erase( a.tag() );
        return parent::erase( a );
    }

    /// delete all entries in table
    /// to delete, call da.unceate()
    template< class DA >
    void clear( const DA & da )
    {
        //m_sqlTable.clear();
        return parent::clear( da );
    }

    /// Put item into hash-table but do not store in DB.
    /// Used to initialize hash-table from data that exists in the SQL
    /// when the sql_item_table is instantiated.
    void hash_item( const Tag & t, const ItemT & i,
                    const int getcount = CnC::Internal::item_properties::NO_GET_COUNT,
                    const int owner = CnC::Internal::item_properties::UNSET_GET_COUNT)
    {
        { CnC::Internal::Speaker oss( std::cout ); oss << "hashing [" << t << "] => " << i;}
        typename parent::accessor a;
        parent::put_item( t, m_coll->create( i ), getcount, owner, a );
    }

private:
    const Coll * m_coll;
    sql_table< sql_adaptor< Tag, ItemT > > m_sqlTable;
};

#endif // _SQL_ITEM_TABLE_INCLUDED_
