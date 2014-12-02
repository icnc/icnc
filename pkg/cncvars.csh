#!/bin/csh
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

setenv CNCROOT "SUBSTITUTE_INSTALL_DIR_HERE"
setenv __TBB_DIR "$CNCROOT/SUBSTITUTE_TBB_INSTALL_DIR_HERE"
#if ($#argv < 1) then
setenv CNC_ARCH_PLATFORM "intel64"
#else
#    setenv CNC_ARCH_PLATFORM $1
#endif

if (! $?MIC_LD_LIBRARY_PATH) then
    setenv MIC_LD_LIBRARY_PATH "${CNCROOT}/lib/mic"
else
    setenv MIC_LD_LIBRARY_PATH "${CNCROOT}/lib/mic:$MIC_LD_LIBRARY_PATH"
endif
if (! $?MIC_LIBRARY_PATH) then
    setenv MIC_LIBRARY_PATH "${CNCROOT}/lib/mic"
else
    setenv MIC_LIBRARY_PATH "${CNCROOT}/lib/mic:$MIC_LIBRARY_PATH"
endif

if (! $?LIBRARY_PATH) then
    setenv LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"
else
    setenv LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$LIBRARY_PATH"
endif
if (! $?LD_LIBRARY_PATH) then
    setenv LD_LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"
else
    setenv LD_LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$LD_LIBRARY_PATH"
endif
if (! $?DYLD_LIBRARY_PATH) then
    setenv DYLD_LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}"
else
    setenv DYLD_LIBRARY_PATH "${CNCROOT}/lib/${CNC_ARCH_PLATFORM}:$DYLD_LIBRARY_PATH"
endif
if (! $?CPATH) then
    setenv CPATH "${CNCROOT}/include"
else
    setenv CPATH "${CNCROOT}/include:$CPATH"
endif
#if (! $?PATH) then
#    setenv PATH "${CNCROOT}/bin/${CNC_ARCH_PLATFORM}"
#else
#    setenv PATH "${CNCROOT}/bin/${CNC_ARCH_PLATFORM}:$PATH"
#endif

if( -e $__TBB_DIR/bin/tbbvars.csh ) then
    if ($?TBBROOT) then
	echo "Overwriting existing TBB settings."
    endif
    source $__TBB_DIR/bin/tbbvars.csh $CNC_ARCH_PLATFORM
endif
unsetenv __TBB_DIR
