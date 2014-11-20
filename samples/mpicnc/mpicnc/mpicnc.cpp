/* *******************************************************************************
 *  Copyright (c) 2013-2014, Intel Corporation
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
  Showcasing ths SPMD style interface to CnC.  We'll alternate between
  MPI and CnC phases.  Additionally we'll let different groups of
  processes concurrently operate on separate CnC phases (we'll use the
  same context, but different instances of it).

  This is an "ariticial" example, we don't really compute anything
  meaningful.  The MPI-phase is a simple All_to_all communication. The
  CnC phase prescribes a set of steps which simple take an input and
  produce one output.  We use the control tag as the key of the output
  item.  We use tag/F as the key of the input item, as tags are
  integers this makes F many steps depend on the same item instance.
 */

#include <mpi.h>
#ifdef _DIST_
#include <cnc/dist_cnc.h>
#else
#include <cnc/cnc.h>
#endif
#include <cnc/debug.h>

struct my_context;

int rank = 0;

// our simple step
struct MyStep
{
    int execute( int n, my_context & c ) const;
};

const int N = 480; // number of step-instances, will create control tags 0-N
const int F = 6;   // number of steps reading the same item

// include the tuner, needed for testing (only)
#include "mpicnc.h"

// The CnC context defines a trivial graph with 3 collections.
// Nothing specific to our MPI/CNC/SPMD combo
struct my_context : public CnC::context< my_context >
{
    CnC::step_collection< MyStep, my_tuner > m_steps;
    CnC::tag_collection<int>                 m_tags;
    CnC::item_collection<int, int, my_tuner> m_items;
    my_context() 
        : CnC::context< my_context >(),
          m_steps( *this ),
          m_tags( *this ),
          m_items( *this )
    {
        m_tags.prescribes( m_steps, *this );
        m_steps.consumes( m_items );
        m_steps.produces( m_items );
        CnC::debug::trace( m_items );
        //CnC::debug::trace_all( *this );
    }
};

// ok, let's get one item and put another one.
// we don't care what the data is.
int MyStep::execute( int t, my_context & c ) const
{
    int item = 11;
    // tags are integers; so t/F returns the same value for multiple values of t(ag)
    if( t ) c.m_items.get( t/F, item );
    c.m_items.put( t, t+item*item );

    return CnC::CNC_Success;
}

// This is our CnC phase.
// We instantiate the dist_cnc_init object, put a number of tags and wait.
// The function can be called with different parameters.  We need the
// communicator in any case, but of course it can be MPI_Comm_World.
// If dist_env is true we do it the SPMD-way: every process, not only
// root, executes everything. If dist_env is false only the root
// executes things outside the dist_cnc_init constructor.
void cnc_phase( MPI_Comm mc, bool dist_env )
{
#ifdef _DIST_
    // Init distribution infrastructure (RAII), most likely behaves like a barrier
    // if dist_env==false client processes get blocked in here forever.
    CnC::dist_cnc_init< my_context > dc_init( mc, dist_env );
#endif

    // as we provided a communicator to dc_init, all processes
    // continue here (by default only rank 0 gets here)

    std::cout << "Let's go\n";
    // now let's get MPI rank etc.
    int numranks = 0;
    MPI_Comm_rank(mc,&rank);
    MPI_Comm_size(mc,&numranks);

    // The environment code should be executed on rank 0 and
    // on all client processes of a distributed environment.
    // We might have more than one rank 0
    // if different communicators are in use.
    // Multiple environments are semantically tricky, combined with
    // CnC they make sense if the MPI phase creates distributed
    // data to be used by CnC. We mimic this by distributing
    // the control-puts across all processes of a distributed env.
    if( rank == 0 || dist_env ) {
        // create the context, the runtime will distribute it under the hood
        my_context c;
        // dist_env requires a sync, e.g. a barrier
        if( dist_env ) MPI_Barrier( mc );
        // start tag, 0 if root
        int s = rank;
        // in a dist env each process puts a subset of tags
        // we do this by setting the loop-increment to the number
        // of processes in the env
        int inc = dist_env ? numranks : 1;
        // Simply put all (owned) control tags
        for (int number = s; number < N; number += inc) {
            c.m_tags.put(number);
        }
        // everyone in the env waits for quiescence
        c.wait();
        std::cout << "done\n";
    }
    // here the CnC distribution infrastructure gets shut down 
    // when the dist_cnc_init object gets destructed (RAII)
}

int main( int argc, char *argv[] )
{
    int  numranks; 
    int p;
    MPI_Init_thread( 0, NULL, MPI_THREAD_MULTIPLE, &p );
    if( p != MPI_THREAD_MULTIPLE ) std::cerr << "Warning: not MPI_THREAD_MULTIPLE (" << MPI_THREAD_MULTIPLE << "), but " << p << std::endl;

    MPI_Comm_size(MPI_COMM_WORLD,&numranks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);

    if( rank == 0 ) {
        system("ldd /home/fschlimb/cnc/trunk/distro/tests_runtime/tests-linux-intel64/distmpicnc");
    }

    // Let's split COMM_WORLD into 2 MPI_Comm's
    MPI_Comm  newcomm;
    int color = rank%2;
    MPI_Comm_split(MPI_COMM_WORLD, color, rank, &newcomm);
    
    // let's not only iterate through alternating CnC/MPI phases, but
    // also let 2 CnC "instances" work concurrently (each has its own
    // MPICommunicator)
    for( int i = 0; i < 2; ++i ) { // you can loop as often as you like
        // Now let's do some CnC, all processes execute this, but on different MPI_Comm's.
        // One communicator is executed as a dist_env (color==0)
        // the other as a "normal" single-env on its root (color==1)
        cnc_phase( newcomm, color == 1 );
        // And some MPI in between
        // All procs do this together on MPI_COMM_WORLD
        int res;
        MPI_Allreduce(&color, &res, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    }
    // Done: collectively shut down MPI
    MPI_Finalize();

    if( rank == 0 ) std::cerr << "\n\nFinalized and out.\nThis should be the last message.\n";
}
