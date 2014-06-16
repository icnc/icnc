generic_comm : contains the channel independent pieces

socket_comm  : socket specific stuff

lrb_comm     : Larrabee specific stuff

cnc_dummy    : only for standalone testing, provides the relevant
               pieces of CnC. Used for the small test cases
               "socket_comm/test" and "lrb_comm/test" which therefore
               don't need a full CnC installation

#~~~~~~~~~~~~~~~~
#  generic_comm
#~~~~~~~~~~~~~~~~

GenericCommunicator : implements the top-level CnC communicator interface

ChannelInterface    : base class which comprises the channel specific
                      low-level functionality (send, recv, etc.)
                      
    GenericCommunicator has an attribute pointer m_channel which 
    has to provide the channel specific functionality. 
    It must be allocated accordingly by derived classes of 
    GenericCommunicator.
    
    Send and receive in GenericCommunicator may be done in separate
    threads. Their logic is implemented by the inner classes
    GenericCommunicator::SendThread resp. GenericCommunicator::RecvThread
    and is provided by the attributes m_sendThread resp. m_recvThread
    of GeneralCommunicator. These objects use the above channel
    object m_channel for handling the low-level communication aspects.
    In this way, they don't know the channel specific details; they need
    only the ChannelInterface. The communication logic and the send/recv
    event loops are implemented by m_sendThread resp. m_recvThread.

ThreadExecuter      : [implementational detail]
                      provides an interface for a thread class.
                      The thread function must be provided by implementing
                      its "runEventLoop" virtual function in a derived class.

Settings            : [implementational detail]
                      provides access to config variables


#~~~~~~~~~~~~~~~
#  socket_comm
#~~~~~~~~~~~~~~~

SocketCommunicator  : extends GenericCommunicator
                      reimplements init() and fini() so that the central
                      m_channel attribute of GenericCommunicator is
                      allocated resp. deallocated properly

SocketChannelInterface : extends ChannelInterface
                      adds socket specific attributes. The m_channel
                      object for the socket case will be of this type
                      (allocated in SocketCommunicator::init)

SocketHostInitializer : implementational detail of SocketCommunicator::init
                      does the host specific socket initialization work
                      of SocketCommunicator::init. Builds and initializes
                      the socket related attributes of m_channel.
                      
SocketClientInitializer : implementational detail of SocketCommunicator::init
                      the analogue of SocketHostInitializer for the client
                      processes. Initialization protocols must be consistent 
                      with SocketHostInitializer.

pal_socket          : low-level socket functions, thin wrappers around
                      the socket system calls, taken from old ITAC source
                      code
                      
pal_util            : some utility functions for pal_socket, also taken
                      from old ITAC source code

                      
#~~~~~~~~~~~~
#  lrb_comm
#~~~~~~~~~~~~

mimics socket_comm, same structure 

LrbCommunicator     : analogue of SocketCommunicator, extends GenericCommunicator,
                      reimplements init() and initializes m_channel for Lrb

LrbChannelInterface : analogue of SocketChannelInterface, extends ChannelInterface
                      concrete type of m_channel for the Lrb case
                      
LrbHostInitializer  : analogue of SocketHostInitializer, implementational detail
                      of LrbCommunicator::init() for the host

LrbClientInitializer : analogue of SocketClientInitializer, implementational detail
                      of LrbCommunicator::init() for the Lrb side

