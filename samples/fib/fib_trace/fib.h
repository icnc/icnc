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
#ifndef fib_H_ALREADY_INCLUDED
#define fib_H_ALREADY_INCLUDED

#include <cnc/cnc.h>
#include <cnc/debug.h>

// Forward declaration of the context class (also known as graph)
struct fib_context;

// The step classes

struct fib_step
{
    int execute( const int & t, fib_context & c ) const;
};

// The context class
struct fib_context : public CnC::context< fib_context >
{
    // the step collection for the instances of the compute-kernel
    CnC::step_collection< fib_step >      m_steps;
    // Item collections
    CnC::item_collection< int, fib_type > m_fibs;
    // Tag collections
    CnC::tag_collection< int >            m_tags;

    // The context class constructor
    fib_context()
        : CnC::context< fib_context >(),
          // Initialize each step collection
          m_steps( *this, "fib_step" ),
          // Initialize each tag collection
          m_fibs( *this, "fibs" ),
          // Initialize each item collection
          m_tags( *this, "tags" )
    {
        // prescribe compute steps with this (context) as argument
        m_tags.prescribes( m_steps, *this );
        // step consumes m_fibs
        m_steps.consumes( m_fibs );
        // step also produces m_fibs
        m_steps.produces( m_fibs );
    }
};

#endif // fib_H_ALREADY_INCLUDED
