/*Copyright (c) 2013-, Ravi Teja Mullapudi, CSA Indian Institue of Science
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  3. Neither the name of the Indian Institute of Science nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ''AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "utils.h"
#include <math.h>
#if defined( _WIN32 ) || defined( WIN32 )
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <float.h>
#ifndef isnan
#define isnan(x) _isnan(x)
#endif
#ifndef isinf
#define isinf(x) (!_finite(x))
#endif
#else
#include <sys/time.h>
#endif
#include <stdio.h>


bool fequal(double x, double y)
{
    double diff, epsilon = 1e-5;
    int isnan_x, isnan_y;
    int isinf_x, isinf_y;
    isnan_x = isnan(x);
    isnan_y = isnan(y);
    if (isnan_x && isnan_y) return true;
    if (isnan_x || isnan_y) return false;
    isinf_x = isinf(x);
    isinf_y = isinf(y);
    if (isinf_x && isinf_y) {
        return (x > 0 && y > 0) || (x < 0 && y < 0);
    }
    if (isinf_x || isinf_y) return false;
    diff = fabs(x-y);
    // NTV: 03/30/2009 need this case distinction otherwise comparison against
    // 0.0 will always fail.
    if ((x == 0.0 && y != 0.0) ||
        (x != 0.0 && y == 0.0)) {
        return diff <= epsilon;
    }
    // NTV: 05/06/2009 need this case to rule out differences due to
    // imprecisions close to 0 on cell. Ideally it would only be activated on
    // cell.
    if (x <= epsilon && -epsilon <= x && 
        y <= epsilon && -epsilon <= y) {
        return diff <= epsilon;
    }
    return diff <= epsilon * fabs(x) ||
        diff <= epsilon * fabs(y);
}

double rtclock()
{
#if defined( _WIN32 ) || defined( WIN32 )
  struct _timeb tv;
  _ftime64_s( &tv );
  return ( ((double)tv.time) + ((double)tv.millitm) * 1E-3 );
#else
  struct timezone Tzp;
  struct timeval Tp;
  int stat;
  stat = gettimeofday (&Tp, &Tzp);
  if (stat != 0) printf("Error return from gettimeofday: %d",stat);
  return(Tp.tv_sec + Tp.tv_usec*1.0e-6);
#endif
}
