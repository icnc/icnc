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

#ifndef _CNC_COMM_SETTINGS_H_
#define _CNC_COMM_SETTINGS_H_

#include <string>
#include <functional> // std::less

namespace CnC {
namespace Internal {

    template< class Key, class Val, class Cmp = std::less< Key > > class LockedMap;
    
    /// A class to store settings/configuration as a map string->value.
    /// The settings/options are read from a config file, provided at init time.
    class Settings
    {
    public:
        /// looks up given option and sets given pointer to its value (string)
        static const char * get_string( const char * option, const char * default_value );
        /// looks up given option and sets given var to its integer value
        static int get_int( const char * option, int default_value );
        /// looks up given option and sets given var to its double value
        static double get_double( const char * option, double default_value );
        
        ~Settings();
    private:
        /// reads settings in given file
        static void init( const char * file = NULL );
        static LockedMap< std::string, std::string > * the_map;
    };
    
} // namespace CnC
} // namespace Internal

#endif //_CNC_COMM_SETTINGS_H_
