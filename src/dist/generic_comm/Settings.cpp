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
  see Settings.h
*/

#include "pal_config.h" // _CRT_SECURE_NO_WARNINGS (on Windows)
#include "Settings.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cstdlib> // atol

#include <tbb/spin_mutex.h>
#include <map>
#include <cstring> // strcmp

namespace CnC {
namespace Internal
{
    template< class Key, class Val, class Cmp /* = std::less< Key > */ >
    class LockedMap
    {
    public:
        typedef std::map< Key, Val, Cmp > Map;

        Val * find( const Key & key ) const {
            tbb::spin_mutex::scoped_lock lock( m_mutex );
            typename Map::const_iterator i = m_map.find( key );
            if( i == m_map.end() ) return NULL;
            return const_cast< Val * >( &(i->second) );
        }
        Val& operator[]( const Key & key ) {
            tbb::spin_mutex::scoped_lock lock( m_mutex );
            return m_map[key];
        }
        size_t erase( const Key & key ) {
            tbb::spin_mutex::scoped_lock lock( m_mutex );
            return m_map.erase( key );
        }
        size_t size() {
            tbb::spin_mutex::scoped_lock lock( m_mutex );
            return m_map.size();
        }
        template< typename Op >
        void foreach( Op & op ) {
            tbb::spin_mutex::scoped_lock lock( m_mutex );
            typename Map::iterator i = m_map.begin();
            while( i != m_map.end() ) {
                typename Map::iterator j = i++;
                op( j->first, j->second, m_map );
            }
        }
        
    protected:
        Map m_map;
        mutable tbb::spin_mutex m_mutex;
    };

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    LockedMap< std::string, std::string > * Settings::the_map = NULL;
    namespace {
        Settings settings;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    void Settings::init( const char * file )
    {
        // we assume that the_map is inited *before* settings
        the_map = new LockedMap< std::string, std::string >;
        if ( file ) {
            std::ifstream fileStream( file );
            if ( ! fileStream ) {
                std::cerr << "Could not open config file " << file << ". Ignored.\n";
            } else {
                // Syntax:
                // - allow leading and trailing spaces
                // - allow spaces before and after the separator sign '='
                // - allow comment indicator '#' for cutting off the rest of the line
                // - allow empty lines
                const int maxLineLen = 2048; // should suffice
                char line[maxLineLen];
                int lineCounter = 0;
                while ( fileStream.getline( line, maxLineLen ) ) 
                {
                    ++lineCounter;
                     // Skip leading spaces:
                    char* pc = line;
                    while ( *pc && isspace( *pc ) ) ++pc;
                     // Skip comment lines, allow empty lines:
                    if ( *pc == '#' || *pc == '\0' ) continue;
                     // Read key:
                    char* pKey = pc;
                    while ( *pc && ! isspace( *pc ) ) ++pc;
                    if ( *pc != '\0' ) {
                        *pc = '\0'; // make trailing zero for key
                        ++pc;
                    }
                     // Skip spaces:
                    while ( *pc && isspace( *pc ) ) ++pc;
                     // This char must be the separator:
                    if ( *pc != '=' ) {
                        std::cerr << "syntax error in config file" << file 
                                  << ": skipping line " << lineCounter << '\n' << std::flush;
                        continue;
                    }
                     // Skip separator and subsequent spaces:
                    ++pc;
                    while ( *pc && isspace( *pc ) ) ++pc;
                     // The rest of the line is the value.
                    char* pValue = pc;
                     // Check whether there is a comment sign:
                    while ( *pc ) {
                        if ( *pc == '#' ) *pc = '\0'; // cut off comment part
                        ++pc;
                    }
                     // Skip trailing spaces.
                    pc = pValue + strlen( pValue );
                    while ( pc > pValue && isspace( *(pc - 1) ) ) --pc;
                    *pc = '\0'; // make trailing zero for value
                     // Insert (pKey, pValue) into the map:
                    (*the_map)[pKey] = pValue;
                }
            }
        }
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    Settings::~Settings()
    {
        delete the_map;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    const char * Settings::get_string( const char * option, const char * default_value )
    {
        if ( the_map == NULL ) {
            init( getenv( "CNC_CONFIG" ) );
        }
        std::string env_var( option );
        const char * val = getenv( env_var.c_str() );
        if ( val ) {
            return val;
        }
        std::string * tmp_val = the_map->find( option );
        return tmp_val ? tmp_val->c_str() : default_value;
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    int Settings::get_int( const char * option, int default_value )
    {
        if ( the_map == NULL ) {
            init( getenv( "CNC_CONFIG" ) );
        }
        std::string env_var( option );
        const char * val = getenv( env_var.c_str() );
        if ( val ) {
            return atol( val );
        }
        std::string * tmp_val = the_map->find( option );
        return tmp_val ? atol( tmp_val->c_str() ) : default_value;
    }
    
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    double Settings::get_double( const char * option, double default_value )
    {
        if ( the_map == NULL ) {
            init( getenv( "CNC_CONFIG" ) );
        }
        std::string env_var( option );
        const char * val = getenv( env_var.c_str() );
        if ( val ) {
            return atof( val );
        }
        std::string * tmp_val = the_map->find( option );
        return tmp_val ? atof( tmp_val->c_str() ) : default_value;
    }

} // namespace CnC
} // namespace Internal
