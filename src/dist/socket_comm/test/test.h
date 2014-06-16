#ifndef _CNC_TEST_H_INCLUDED_
#define _CNC_TEST_H_INCLUDED_

namespace CnC {
namespace Internal {

    /**
     * Host initializer:
     */
    void init_host_data();

    /**
     * Synchronization variable between host application thread and 
     * host receiver thread:
     */
    extern volatile bool g_host_communication_done;
}
}

/**
 * sleep:
 */
#ifdef _WIN32
# include <windows.h> // Sleep
# define sleep( n ) Sleep( 1000 * (n) )
#else
# include <unistd.h> // sleep
#endif

#endif // _CNC_TEST_H_INCLUDED_
