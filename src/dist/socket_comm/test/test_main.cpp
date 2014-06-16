#include <cnc/internal/dist/distributor.h>
#include <cnc/serializer.h>
using namespace CnC;
using namespace CnC::Internal;

#include "test.h"
#include <iostream>

int main()
{
    init_host_data();
    distributor::start();

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    /*
     * Host receiver thread is running now. 
     * Trigger communication phase!
     */

    // Get a serializer:
    serializer * ser = distributor::new_serializer( 0 );
    int id = 123;
    (*ser) & id;

     // host sends a message to client 1.
     // the other messages are triggered from my special version
     // of distributor::recv_msg in "test_distributor.cpp".
    if ( ! distributor::remote() ) {
        distributor::send_msg( ser, 1 );
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

     // Wait until all clients have sent their termination message:
     // Host receiver thread will set g_host_communication_done accordingly.
    while ( ! g_host_communication_done ) {
        sleep( 1 );
    }

     // All clients are finished, we can shut down:
    distributor::stop();

    return 0;
}
