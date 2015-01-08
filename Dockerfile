# A bleeding edge Ubuntu 14.04 image for compiling CnC Programs (C++).
FROM        ubuntu:14.04
MAINTAINER  fschlimb

# update and install dependencies
RUN         apt-get update && apt-get -y upgrade && apt-get install -y gcc g++ make cmake git wget mpich2 libmpich2-dev python doxygen \
         && apt-get -y autoremove

# get TBB
RUN         wget -nv https://www.threadingbuildingblocks.org/sites/default/files/software_releases/linux/tbb43_20141204oss_lin.tgz \
         && tar -xz -C /usr/share -f tbb43_20141204oss_lin.tgz \
         && ln -s /usr/share/tbb43_20141204oss/lib/intel64/gcc4.4/* /usr/lib/ \
         && ln -s /usr/share/tbb43_20141204oss/include/tbb /usr/include/ \
         && ln -s /usr/share/tbb43_20141204oss/include/serial /usr/include/ \
         && rm tbb* 
# build CnC
#RUN         git clone https://github.com/icnc/icnc.git \
RUN mkdir icnc
COPY        . icnc/         
RUN         cd icnc \
         && python make_kit.py --tbb=/usr --mpi=/usr --itac=NONE \
         && mv `pwd`/kit.pkg/cnc /usr/share/ \
         && ln -s /usr/share/cnc/current/lib/intel64/* /usr/lib/ \
         && ln -s /usr/share/cnc/current/include/cnc /usr/include/ \
         && cd .. &&  rm -r icnc icnc.github.io
ENV         CNCROOT=/usr TBBROOT=/usr MPIROOT=/usr
