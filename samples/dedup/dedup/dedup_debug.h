#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>
#include <stdlib.h>

#define TRACE(fmt, ...) {                                                  \
       fprintf(stderr, "[%s] " fmt, __FUNCTION__, __VA_ARGS__);            \
       }						                   \

#define EXIT_TRACE1(fmt) {                                                 \
        fprintf(stderr,fmt);                                               \
        abort();                                                           \
}
#define EXIT_TRACE2(fmt, s) {                                              \
        fprintf(stderr,fmt,s);                                             \
        abort();                                                           \
}
#define EXIT_TRACE(fmt, ...) {                                          \
       TRACE(fmt, __VA_ARGS__);                                                  \
       exit(-1);                                                           \
}


#ifndef HERE
#define HERE TRACE("file %s, line %d, func %s\n",  __FILE__, __LINE__, __FUNCTION__)
#endif

#endif
