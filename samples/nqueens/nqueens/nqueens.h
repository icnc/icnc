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

#pragma once
#ifndef _NQ_H_
#define _NQ_H_

#define MAX_BOARD_SIZE (sizeof(int)*8)

// A board configuration. We use this as the tag.
// Not optimized in any way, like using bits rather than bytes
struct NQueensConfig
{
    int  m_boardSize;
    int  m_nextRow;
    char m_config[MAX_BOARD_SIZE];
    NQueensConfig( int sz = 0 ) : m_boardSize( sz ), m_nextRow( 0 ) { memset( m_config, 0, sizeof( m_config ) ); }
    NQueensConfig( const NQueensConfig& cc, int c )
        : m_boardSize( cc.m_boardSize ), m_nextRow( cc.m_nextRow + 1 ) {
        memcpy( m_config, cc.m_config, sizeof( m_config ) );
        m_config[cc.m_nextRow] = c; 
    }
    NQueensConfig( const NQueensConfig& cc )
        : m_boardSize( cc.m_boardSize ),
          m_nextRow( cc.m_nextRow )
    {
        memcpy( m_config, cc.m_config, sizeof( m_config ) );
    }
    bool operator==(const NQueensConfig& c) const { return m_nextRow == c.m_nextRow && 0 == memcmp( m_config, c.m_config, m_nextRow ); }
};

template <>
struct cnc_hash< NQueensConfig >
{
	size_t operator()(const NQueensConfig &x) const
	{
		return *(reinterpret_cast<const int*>( x.m_config ));
	}
};

std::ostream& cnc_format( std::ostream& output, const NQueensConfig& c)
{
    output << c.m_nextRow << "[";
    for( int i = 0; i < c.m_nextRow; ++ i ) output << ((int)c.m_config[i]) << ", ";
    return output << "]";
}

#endif // _NQ_H_
