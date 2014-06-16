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


// Find configurations on a nxn board where each row and each colum has esactly one queen.
// We use (partial) configurations as tags and have no items.
// A potential use of fouind configurations would just prescribe the post-processing step
// with the solution tag collections.
// As on optional flag exploration can be cancelled as soon as at least one valid
// solution was found.
// Try with boardsize<14 first.
// enable _DEBUG to print all sloutions found (note: if boardsize>7 this is a huge output).


#ifdef WIN32
#include <windows.h>
#include <mmsystem.h>
#endif
#include <iostream>
#include "tbb/tick_count.h"

#include <cnc/cnc.h>
#include <cnc/debug.h>
#include "nqueens.h"

// Forward declaration of the context class (also known as graph)
struct nq_cnc_context;

// The step classes
struct QueensStep
{
    int execute( const NQueensConfig & t, nq_cnc_context & c ) const;
};

// The context class
struct nq_cnc_context : public CnC::context< nq_cnc_context >
{
    // Tuner
    typedef CnC::cancel_tuner< NQueensConfig, false > tuner_type;
    tuner_type m_tuner;

    // Step collection
    CnC::step_collection< QueensStep, tuner_type > m_nqsteps;

    // Item collections
    // We don't create items, we just create tags if we found a solution.
    // Anyone who needs to do something with that solution can just
    // prescribe a step with that m_solutions

    // Tag collections
    CnC::tag_collection< NQueensConfig, CnC::preserve_tuner< NQueensConfig > > m_solutions;
    CnC::tag_collection< NQueensConfig > m_tags;

    // number of parallel exploration levels (#rows) before going sequential
    int m_tuning_factor;
    // this is set to false if we are interested only in some solutions (not all)
    bool m_findall;


    // The context class constructor
    nq_cnc_context( int tf = 1, bool find_all = true )
        : m_tuner( *this ),
          m_nqsteps( *this, m_tuner ),          
          m_solutions( *this ),
          m_tags( *this ),
          m_tuning_factor( tf ),
          m_findall( find_all )
    {
        // Prescriptive relations
        m_tags.prescribes( m_nqsteps, *this );
        // potentially prescribe post-processing step here
        // m_solutions.prescribe( m_postSteps );
    }
};

// console printout of given solution
void print(const char queens[MAX_BOARD_SIZE], int id, int size)
{
    std::cerr << "Solution " << id << std::endl; 
    for(int row=0; row<size; row++) {
        for(int col=0; col<size; col++) {
            if(queens[row]==col) {
                std::cout << "Q";
            } else {
                std::cout << "-";
            }
        }
        std::cout << std::endl;
    }
}

//verifies that the row and col inputs are a valid position for a
//queen based on the existing board state in currConfig
bool verifyNextQueenPosition( const NQueensConfig & currConfig, int col )
{
	for( int i=0; i<currConfig.m_nextRow; i++ ) {
        if( currConfig.m_config[i] == col || abs( currConfig.m_config[i] - col ) == ( currConfig.m_nextRow - i ) ) {
            return false;
		}
    }

	return true;
}

//Processes the next row in the gameboard
//Only creates new gameboard states that are valid and generate new step tags.
//If the board is completely filled, found a solution and put it in solution tags
// If we are looking for some solutions (and not all) we can cancel exploration
int QueensStep::execute(const NQueensConfig &t, nq_cnc_context &c) const
{
	for( int i=0; i < t.m_boardSize; ++i ) {
		if( verifyNextQueenPosition( t, i ) ) {
			NQueensConfig next( t, i );
			if( next.m_nextRow >= next.m_boardSize ) {
                c.m_solutions.put( next );
                if( c.m_findall == false ) {
                    // If we are looking for some solutions (and not all) we can cancel exploration
                    c.m_tuner.cancel_all();
                }
                return CnC::CNC_Success;

			} else if (next.m_nextRow <= c.m_tuning_factor) {
				// Branch into multiple steps
				c.m_tags.put( next );
			} else {
				// Serial.
				execute(next, c);
			}
		}
	}

	return CnC::CNC_Success;
}

// the driver routine
long long int solve(int size, bool findall, int tuning_factor)
{
    // Create an instance of the Concurrent Collection graph class
    // which defines the CnC graph
    nq_cnc_context c( tuning_factor, findall );

    // Create a tag to start CnC execution
    c.m_tags.put( NQueensConfig( size ) );

    // Wait for the CnC steps to finish
    c.wait();
 
#ifdef _DEBUG
    // Print solutions
    CnC::tag_collection< NQueensConfig, CnC::preserve_tuner< NQueensConfig > >::const_iterator ci;
    int id = 0;
    for( ci = c.m_solutions.begin(); ci != c.m_solutions.end(); ci++) {
        print(ci->first.m_config, ++id, size);
    }
#endif

    return c.m_solutions.size();
}

int main(int argc, char*argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: nq-cnc <board_size> [findsome] [tuning_factor]\n"
                  << "findsome=0 is the default (-> find all)\n"
                  << "tuning_factor is 1 by default. (1 levelof branching)\n";
        return 0;
    }
  
    int size = atoi(argv[1]);
    bool findall = true;
    int tuning_factor = 1;
  
    if (argc >= 3)
        if( strcmp( argv[2], "findsome" ) == 0 ) findall = false;

    if (argc >= 4)
        tuning_factor = atoi(argv[3]);
  
    if (size > MAX_BOARD_SIZE || size < 0) {
        std::cerr << "Cannot create board with N > " << MAX_BOARD_SIZE << " or N < 0.\n";
        return 0;
    }

    std::cout << "Starting Concurrent Collections solver for size " << size << "...\n";
    std::cout << "Using a tuning factor of " << tuning_factor << "\n";

    tbb::tick_count startTime = tbb::tick_count::now();
    long long int count = solve(size, findall, tuning_factor);
    tbb::tick_count endTime = tbb::tick_count::now();

    std::cout << "Number of solutions: " << count << std::endl;
    std::cout << "Calculations took " << (endTime-startTime).seconds() << "s.\n";


    return 0;
}
