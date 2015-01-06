# A bleeding edge Ubuntu 14.04 image for compiling CnC Programs (C++).
FROM        ubuntu:14.04
MAINTAINER  fschlimb

# update and install dependencies
RUN         apt-get update && apt-get install -y gcc g++ make cmake git wget mpich2 libmpich2-dev python doxygen

# get TBB
RUN         wget https://www.threadingbuildingblocks.org/sites/default/files/software_releases/linux/tbb43_20141204oss_lin.tgz
RUN         tar -xz -C /usr/share -f tbb43_20141204oss_lin.tgz
RUN         ln -s /usr/share/tbb43_20141204oss/lib/intel64/gcc4.4/* /usr/lib/
RUN         ln -s /usr/share/tbb43_20141204oss/include/tbb /usr/include/
RUN         ln -s /usr/share/tbb43_20141204oss/include/serial /usr/include/
# build CnC
RUN         git clone https://github.com/icnc/icnc.git
WORKDIR     icnc
RUN         python make_kit.py --tbb=/usr --mpi=/usr --itac=NONE
RUN         mv `pwd`/kit.pkg/cnc /usr/share/
RUN         ln -s /usr/share/cnc/current/lib/intel64/* /usr/lib/
RUN         ln -s /usr/share/cnc/current/include/cnc /usr/include/
# cleanup
WORKDIR     /
RUN         rm -r icnc tbb*
RUN         apt-get autoremove
