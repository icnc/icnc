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
#ifndef blackscholes_H_ALREADY_INCLUDED
#define blackscholes_H_ALREADY_INCLUDED

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <vector>
#include <memory>

typedef std::shared_ptr< std::vector< OptionData > > option_vector_type;
typedef std::shared_ptr< std::vector< fptype > > price_vector_type;

CNC_BITWISE_SERIALIZABLE( OptionData );

// Forward declaration of the context class (also known as graph)
struct blackscholes_context;

// The step classes
struct Compute
{
    int execute( const int &, blackscholes_context & ) const;
};

struct bs_tuner : public CnC::step_tuner<>, public CnC::vector_tuner
{
    int compute_on( const int tag ) const {
        return tag % numProcs();
    }
    int compute_on( const int tag, blackscholes_context & ) const {
        return compute_on( tag );
    }
    int consumed_by( const int tag ) const {
        return tag;
    }
    int consumed_on( const int tag ) const {
        return compute_on( consumed_by( tag ) );
    }
    int getcount( const int tag ) {
        return 1;
    }
};

// The context class
struct blackscholes_context : public CnC::context< blackscholes_context >
{
    // Step collections
    CnC::step_collection< Compute, bs_tuner > compute;

    // Item collections
    CnC::item_collection< int, option_vector_type, bs_tuner > opt_data;
    CnC::item_collection< int, price_vector_type, bs_tuner >  prices;

    // Tag collections
    CnC::tag_collection< int > tags;

    int m_vs;
    // The context class constructor
    blackscholes_context( int vs = 0)
        : CnC::context< blackscholes_context >(),
	  // Initialize each step collection
          compute( *this ),
          // Initialize each item collection
          opt_data( *this ),
          prices( *this ),
          // Initialize each tag collection
          tags( *this ),
          m_vs( vs )
    {
        // Prescriptive relations
        tags.prescribes( compute, *this );
        compute.produces( prices );
        opt_data.set_max( m_vs );
        prices.set_max( m_vs );
    }
#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_vs;
        if( ser.is_unpacking() ) {
            opt_data.set_max( m_vs );
            prices.set_max( m_vs );
        }
            
    }
#endif
};

#endif // blackscholes_H_ALREADY_INCLUDED
