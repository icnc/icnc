//********************************************************************************
// Copyright (c) 2007-2013 Intel Corporation. All rights reserved.              **
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

/*
 *  my_getopt.c - my re-implementation of getopt.
 *  Copyright 1997, 2000, 2001, 2002, 2006, Benjamin Sittler
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

int optind=1, opterr=1, optopt=0;
char *optarg=0;

/* reset argument parser to start-up values */
int getopt_reset(void)
{
    optind = 1;
    opterr = 1;
    optopt = 0;
    optarg = 0;
    return 0;
}

/* this is the plain old UNIX getopt, with GNU-style extensions. */
/* if you're porting some piece of UNIX software, this is all you need. */
/* this supports GNU-style permution and optional arguments */

int getopt(int argc, char * argv[], const char *opts)
{
  static int charind=0;
  const char *s;
  char mode, colon_mode;
  int off = 0, opt = -1;

  if(getenv("POSIXLY_CORRECT")) colon_mode = mode = '+';
  else {
    if((colon_mode = *opts) == ':') off ++;
    if(((mode = opts[off]) == '+') || (mode == '-')) {
      off++;
      if((colon_mode != ':') && ((colon_mode = opts[off]) == ':'))
        off ++;
    }
  }
  optarg = 0;
  if(charind) {
    optopt = argv[optind][charind];
    for(s=opts+off; *s; s++) if(optopt == *s) {
      charind++;
      if((*(++s) == ':') || ((optopt == 'W') && (*s == ';'))) {
        if(argv[optind][charind]) {
          optarg = &(argv[optind++][charind]);
          charind = 0;
        } else if(*(++s) != ':') {
          charind = 0;
          if(++optind >= argc) {
            if(opterr) fprintf(stderr,
                                "%s: option requires an argument -- %c\n",
                                argv[0], optopt);
            opt = (colon_mode == ':') ? ':' : '?';
            goto getopt_ok;
          }
          optarg = argv[optind++];
        }
      }
      opt = optopt;
      goto getopt_ok;
    }
    if(opterr) fprintf(stderr,
                        "%s: illegal option -- %c\n",
                        argv[0], optopt);
    opt = '?';
    if(argv[optind][++charind] == '\0') {
      optind++;
      charind = 0;
    }
  getopt_ok:
    if(charind && ! argv[optind][charind]) {
      optind++;
      charind = 0;
    }
  } else if((optind >= argc) ||
             ((argv[optind][0] == '-') &&
              (argv[optind][1] == '-') &&
              (argv[optind][2] == '\0'))) {
    optind++;
    opt = -1;
  } else if((argv[optind][0] != '-') ||
             (argv[optind][1] == '\0')) {
    char *tmp;
    int i, j, k;

    if(mode == '+') opt = -1;
    else if(mode == '-') {
      optarg = argv[optind++];
      charind = 0;
      opt = 1;
    } else {
      for(i=j=optind; i<argc; i++) if((argv[i][0] == '-') &&
                                        (argv[i][1] != '\0')) {
        optind=i;
        opt=getopt(argc, argv, opts);
        while(i > j) {
          tmp=argv[--i];
          for(k=i; k+1<optind; k++) argv[k]=argv[k+1];
          argv[--optind]=tmp;
        }
        break;
      }
      if(i == argc) opt = -1;
    }
  } else {
    charind++;
    opt = getopt(argc, argv, opts);
  }
  if (optind > argc) optind = argc;
  return opt;
}
