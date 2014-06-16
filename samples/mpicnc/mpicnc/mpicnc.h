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

#ifndef _MPI_CNC_INCLUDED_
#define _MPI_CNC_INCLUDED_

const int NT = 8;

// A tuner, we provide consumed_on and compute_on so that things likely
// to fail if the communication channels are setup incorrectly
struct my_tuner : public CnC::step_tuner<>, public CnC::hashmap_tuner
{
#ifdef _CNC_TESTING_
    std::vector<int> consumed_on( const int & t ) const
    {
        std::vector<bool> tmp(numProcs(),false);
        // all t*F <= t' < (t+1)*F consume t
        const int s = t * F;
        const int e = std::min( std::min( F, (numProcs()*NT) ), N-s );
        int cnt = 0;
        for( int i = 0; i<e; ++i ) if( tmp[((s+i) / NT)%numProcs()] == false ) {
                ++cnt;
                tmp[((s+i) / NT)%numProcs()] = true;
            }
        std::vector<int> _c(cnt);
        for( int i = 0; i < tmp.size(); ++i ) {
            if( tmp[i] ) _c[--cnt] = i;
        }
        return _c;
    }

    int compute_on( const int & p, my_context & c ) const
    {
        return (p / NT)%numProcs();
    }

    int get_count( const int t ) const
    {
        int mx = N/F;
        if( t > mx ) return 0;
        return t ? (t == mx ? N%F : F ) : F-1;
    }
#endif
};

#endif //_MPI_CNC_INCLUDED_
