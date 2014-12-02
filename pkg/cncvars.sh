#!/bin/sh
# ********************************************************************************
#  Copyright (c) 2008-2014 Intel Corporation. All rights reserved.              **
#                                                                               **
#  Redistribution and use in source and binary forms, with or without           **
#  modification, are permitted provided that the following conditions are met:  **
#    * Redistributions of source code must retain the above copyright notice,   **
#      this list of conditions and the following disclaimer.                    **
#    * Redistributions in binary form must reproduce the above copyright        **
#      notice, this list of conditions and the following disclaimer in the      **
#      documentation and/or other materials provided with the distribution.     **
#    * Neither the name of Intel Corporation nor the names of its contributors  **
#      may be used to endorse or promote products derived from this software    **
#      without specific prior written permission.                               **
#                                                                               **
#  This software is provided by the copyright holders and contributors "as is"  **
#  and any express or implied warranties, including, but not limited to, the    **
#  implied warranties of merchantability and fitness for a particular purpose   **
#  are disclaimed. In no event shall the copyright owner or contributors be     **
#  liable for any direct, indirect, incidental, special, exemplary, or          **
#  consequential damages (including, but not limited to, procurement of         **
#  substitute goods or services; loss of use, data, or profits; or business     **
#  interruption) however caused and on any theory of liability, whether in      **
#  contract, strict liability, or tort (including negligence or otherwise)      **
#  arising in any way out of the use of this software, even if advised of       **
#  the possibility of such damage.                                              **
# ********************************************************************************
# 
#
# This is the script file to set up the user's environment variables
# for Intel(R) Concurrent Collections for C++.

CNCROOT="SUBSTITUTE_INSTALL_DIR_HERE"; export CNCROOT
__TBB_DIR="$CNCROOT/SUBSTITUTE_TBB_INSTALL_DIR_HERE"

#if [ $# -eq 0 ]; then
CNC_ARCH_PLATFORM="intel64"; export CNC_ARCH_PLATFORM
#else
#    CNC_ARCH_PLATFORM="$1"; export CNC_ARCH_PLATFORM
#fi
    
if [ -z "${MIC_LD_LIBRARY_PATH}" ] ; then
   MIC_LD_LIBRARY_PATH="$CNCROOT/lib/mic"; export MIC_LD_LIBRARY_PATH
else
   MIC_LD_LIBRARY_PATH="$CNCROOT/lib/mic:${MIC_LD_LIBRARY_PATH}"; export MIC_LD_LIBRARY_PATH
fi
if [ -z "${MIC_LIBRARY_PATH}" ] ; then
   MIC_LIBRARY_PATH="$CNCROOT/lib/mic"; export MIC_LIBRARY_PATH
else
   MIC_LIBRARY_PATH="$CNCROOT/lib/mic:${MIC_LIBRARY_PATH}"; export MIC_LIBRARY_PATH
fi

if [ -z "${LIBRARY_PATH}" ]; then
    LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"; export LIBRARY_PATH
else
    LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$LIBRARY_PATH"; export LIBRARY_PATH
fi
if [ -z "${LD_LIBRARY_PATH}" ]; then
    LD_LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"; export LD_LIBRARY_PATH
else
    LD_LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$LD_LIBRARY_PATH"; export LD_LIBRARY_PATH
fi
if [ -z "${DYLD_LIBRARY_PATH}" ]; then
    DYLD_LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"; export DYLD_LIBRARY_PATH
else
    DYLD_LIBRARY_PATH="${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$DYLD_LIBRARY_PATH"; export DYLD_LIBRARY_PATH
fi
if [ -z "${CPATH}" ]; then
    CPATH="${CNCROOT}/include"; export CPATH
else
    CPATH="${CNCROOT}/include:$CPATH"; export CPATH
fi
#if [ -z "${PATH}" ]; then
#    PATH="${CNCROOT}/bin/${CNC_ARCH_PLATFORM}"; export PATH
#else
#    PATH="${CNCROOT}/bin/${CNC_ARCH_PLATFORM}:$PATH"; export PATH
#fi

if [ -f "$__TBB_DIR/bin/tbbvars.sh" ] ; then
    if [ -n "${TBBROOT}" ]; then
	echo "Overwriting existing TBB settings."
    fi
    source $__TBB_DIR/bin/tbbvars.sh $CNC_ARCH_PLATFORM
fi
