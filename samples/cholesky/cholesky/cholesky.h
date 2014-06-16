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
#ifndef cholesky_H_ALREADY_INCLUDED
#define cholesky_H_ALREADY_INCLUDED

#ifdef _DIST_
# include <cnc/dist_cnc.h>
#else
# include <cnc/cnc.h>
#endif
#include <cnc/debug.h>
#include <memory>

// Forward declaration of the context class (also known as graph)
struct cholesky_context;

// The step classes

struct S1_compute
{
    int execute( const int & t, cholesky_context & c ) const;
};

struct S2_compute
{
    int execute( const pair & t, cholesky_context & c ) const;
};

struct S3_compute
{
    int execute( const triple & t, cholesky_context & c ) const;
};

///////////////

static void mark( int p, bool * d )
{
    if( d[p] == false ) {
        d[p] = true;
    }
}

struct cholesky_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
    cholesky_tuner( cholesky_context & c, int p = 0, int n = 0, dist_type dt = BLOCKED_ROWS )
        : m_context( c ), m_p( p ), m_n( n ),
          m_div( (((p*p) / 2) + 1) / numProcs() ),
          //          m_div( p ? (((p*(p+1))/2)+((2*numProcs())/2))/(2*numProcs()) : 0 ),
          m_dt( dt )
    {
        if( myPid() == 0 ) {
            switch( dt ) {
            default:
            case BLOCKED_ROWS :
                std::cerr << "Distributing BLOCKED_ROWS\n";
                break;
            case ROW_CYCLIC :
                std::cerr << "Distributing ROW_CYCLIC\n";
                break;
            case COLUMN_CYCLIC :
                std::cerr << "Distributing COLUMN_CYCLIC\n";
                break;
            case BLOCKED_CYCLIC :
                std::cerr << "Distributing BLOCKED_CYCLICS\n";
                break;
            }
        }
    }

    inline static int compute_on( const dist_type dt, const int i, const int j, const int n, const int s )
    {
        switch( dt ) {
        default:
        case BLOCKED_ROWS :
            return ( ((j*j)/2 + 1 + i ) / s ) % numProcs();
            break;
        case ROW_CYCLIC :
            return j % numProcs();
            break;
        case COLUMN_CYCLIC :
            return i % numProcs();
            break;
        case BLOCKED_CYCLIC :
            return ( (i/2) * n + (j/2) ) % numProcs();
            break;
        }
    }

    // step-bits
    int compute_on( const int tag, cholesky_context & /*arg*/ ) const
    {
        return compute_on( m_dt, tag, tag, m_n, m_div );
    }
    
    int compute_on( const pair & tag, cholesky_context & /*arg*/ ) const
    {
        return compute_on( m_dt, tag.first, tag.second, m_p, m_div );
    }
    
    int compute_on( const triple & tag, cholesky_context & /*arg*/ ) const
    {
        return compute_on( m_dt, tag[2], tag[1], m_p, m_div );
    }
    
    // item-bits
    typedef triple tag_type;

    int get_count( const tag_type & tag ) const
    {
        int _k = tag[0], _i = tag[2];
        if( _k == _i+1 ) return CnC::NO_GETCOUNT; // that's our result
        return ( _k > 0 && _k > _i ) ? ( m_p - _k ) : 1;
    }

    // First we determine which *steps* are going to consume this item.
    // Then we use compute_on to determine the *processes* to which the item needs to go.
    // Mostly the two steps are formulated in one line.
    // We avoid duplicate entries by using a helper mechanism "mark"
    std::vector< int > consumed_on( const tag_type & tag  ) const
    {
        int _k = tag[0], _j = tag[1], _i = tag[2];

        if( _i == _j ) { // on diagonal
            if( _i == _k ) return std::vector< int >( 1, compute_on( _k, m_context ) ); // S1 only
            if( _k == m_p ) return std::vector< int >( 1, 0 ); // the end
        }
        if( _i == _k ) return std::vector< int >( 1, compute_on( pair( _k, _j ), m_context ) ); // S2 only

        bool * _d;
        _d = new bool[numProcs()];
        memset( _d, 0, numProcs() * sizeof( *_d ) );

        if( _i == _k-1 ) {
            if( _i == _j ) { // on diagonal on S2
                for( int j = _k; j < m_p; ++j ) {
                    mark( compute_on( pair( _k - 1, j ), m_context ), _d );
                }
            } else { // S3
                for( int j = _j; j < m_p; ++j ) {
                    for( int i = _k; i <= _j; ++i ) {
                        mark( compute_on( triple( _k-1, j, i ), m_context ), _d );
                    }
                }
            }
        }
        mark( compute_on( triple( _k, _j, _i ), m_context ), _d );
        std::vector< int > _v;
        _v.reserve( numProcs()/2 );
        if( _d[myPid()] ) _v.push_back( myPid() );
        for( int i = 0; i < numProcs(); ++i ) {
            if( _d[i] && i != myPid() ) _v.push_back( i );
        }
        delete [] _d;
        return _v;
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & m_p & m_div & m_n & m_dt;
    }
#endif
private:
    cholesky_context & m_context;
    int m_p;
    int m_n;
    int m_div;
    dist_type m_dt;
};

// The context class
struct cholesky_context : public CnC::context< cholesky_context >
{
    // tuners
    cholesky_tuner tuner;

    // Step Collections
    CnC::step_collection< S1_compute, cholesky_tuner > sc_s1_compute;
    CnC::step_collection< S2_compute, cholesky_tuner > sc_s2_compute;
    CnC::step_collection< S3_compute, cholesky_tuner > sc_s3_compute;

    // Item collections
    CnC::item_collection< triple, tile_const_ptr_type, cholesky_tuner > Lkji;
    int p,b;

    // Tag collections
    CnC::tag_collection< int > control_S1;
    CnC::tag_collection< pair > control_S2;
    CnC::tag_collection< triple > control_S3;

    // The context class constructor
    cholesky_context( int _b = 0, int _p = 0, int _n = 0, dist_type dt = BLOCKED_ROWS )
        : CnC::context< cholesky_context >(),
          // init tuners
          tuner( *this, _p, _n, dt ),
          // init step colls
          sc_s1_compute( *this, tuner, "Cholesky" ),
          sc_s2_compute( *this, tuner, "Trisolve" ),
          sc_s3_compute( *this, tuner, "Update" ),
          // Initialize each item collection
          Lkji( *this, "Lkji", tuner ),
          p( _p ),
          b( _b ),
          // Initialize each tag collection
          control_S1( *this, "S1" ),
          control_S2( *this, "S2" ),
          control_S3( *this, "S3" )
    {
        // Prescriptive relations
        control_S1.prescribes( sc_s1_compute, *this );
        control_S2.prescribes( sc_s2_compute, *this );
        control_S3.prescribes( sc_s3_compute, *this );

        // producer/consumer relations
        sc_s1_compute.consumes( Lkji );
        sc_s1_compute.produces( Lkji );
        sc_s2_compute.consumes( Lkji );
        sc_s2_compute.produces( Lkji );
        sc_s3_compute.consumes( Lkji );
        sc_s3_compute.produces( Lkji );
#if 0
        CnC::debug::trace_all( *this );
#endif
    }

#ifdef _DIST_
    void serialize( CnC::serializer & ser )
    {
        ser & p & b & tuner;
    }
#endif
};

#endif // cholesky_H_ALREADY_INCLUDED
