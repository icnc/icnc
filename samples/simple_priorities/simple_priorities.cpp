// priority test
// adapted from fibonacci example
// see accompanying README.txt for more details

//********************************************************************************
// Copyright (c) 2010-2014 Intel Corporation. All rights reserved.              **
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
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"  **
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE    **
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE   **
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE     **
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR          **
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF         **
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS     **
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN      **
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)      **
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF       **
// THE POSSIBILITY OF SUCH DAMAGE.                                              **
//********************************************************************************

#define _CRT_SECURE_NO_DEPRECATE // to keep the VS compiler happy with TBB

#include "cnc/cnc.h"
#include <cnc/cnc.h>
#include <cnc/debug.h>

static inline void stepDelay() {
    tbb::this_tbb_thread::sleep(tbb::tick_count::interval_t(0.5));
}

struct pt_context;

struct pt_tuner : public CnC::step_tuner<>
{
    int priority( const int & tag, pt_context & c ) const;
};

// Forward declaration of the context class (also known as graph)
struct pt_context;

// The step classes

struct pt_step
{
    int execute( const int & t, pt_context & c ) const;
};

// The context class
struct pt_context : public CnC::context< pt_context >
{
    // the step collection for the instances of the compute-kernel
    CnC::step_collection< pt_step, pt_tuner > m_steps;
    // Tag collections
    CnC::tag_collection< int >            m_tags;

    // The context class constructor
    pt_context()
        : CnC::context< pt_context >(),
          // Initialize each step collection
          m_steps( *this, "pt_step" ),
          // Initialize each item collection
          m_tags( *this, "tags" )
    {
        // prescribe compute steps with this (context) as argument
        m_tags.prescribes( m_steps, *this );
    }
};

int pt_tuner::priority( const int & tag, pt_context & c ) const
{
    std::cout << "Step priority is " << tag << std::endl;
    return tag;
}

int pt_step::execute( const int & tag, pt_context & ctxt ) const
{
    std::cout << "Executing step " << tag << std::endl;
    stepDelay();
    return CnC::CNC_Success;
}

int main( int argc, char* argv[] )
{
    // create context
    pt_context ctxt;

    // show scheduler statistics when done
    CnC::debug::collect_scheduler_statistics( ctxt );

    // put tags to initiate evaluation
    for( int i = 0; i < 30; i += 3 ) {
        ctxt.m_tags.put( i % 10 );
    }

    stepDelay();

    // wait for completion
    ctxt.wait();

    return 0;
}
