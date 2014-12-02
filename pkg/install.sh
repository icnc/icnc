#!/bin/sh
# ********************************************************************************
#  Copyright (c) 2008-2013 Intel Corporation. All rights reserved.              **
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
# This script installs Intel(R) Concurrent Collections for C++

cwd=`dirname $0`
cd $cwd

clear
echo "Installing Intel(R) Concurrent Collections for C++ requires you to accept the following license agreement."
echo "Press 'enter' to view the EULA."
read _l_
less LICENSE

echo "Type 'accept' if a accept the license agrement."
read answer

if [ "$answer" != "accept" ] ; then
    echo ""
    echo "You did not accept the license agreement."
    echo "Intel(R) Concurrent Collections for C++ has not been installed."
    echo "Installation aborted."
    exit 1
fi

echo ""
echo "You accepted the license agreement"
echo ""

ok="0"
while [ "$ok" != "yes" ]
do
    ip="/opt/intel/cnc/$CNCVER"
    echo "Please enter install path [just pressing 'enter' defaults to $ip]:"
    read uip
    if [ "$uip" != "" ] ; then
	ip=$uip
    fi
    if [ -d "$ip" ] ; then
	echo ""
	echo "$ip exists. This might not be what you want."
	ok="yes"
    elif ! mkdir -p $ip ; then
	echo "Could not create directory $ip"
    else
	echo ""
	ok="yes"
   fi
done

echo "To start extracting files to $ip press 'enter'"
echo "Everything else will abort the installation."
read ok
if [ "$ok" != "" ] ; then
    echo "Installation aborted."
    exit 2
fi

echo ""
echo "Extracting CnC files to $ip..."
if ! gpg -d -q --batch --passphrase "I $answer the EULA" cnc_b_$CNCVER_install.files | tar -xj -C $ip -f - ; then
    echo "Problems during installation. Please contact the Intel support team."
    echo "Installation failed."
    exit 3
fi
echo "Done extracting CnC files (gpg warnings can be ignored)."
echo ""

echo "You can optionally install the Intel(R) Threading Building Blocks files needed for CnC."
echo "It can co-exist with other installations of TBB, so if in doubt it is recommended to install them."
q="Install TBB ($TBBVER) files needed for CnC? [y,Y,n,N]"
answer="noanswer"
while [ "$answer" != "N" ] && [ "$answer" != "n" ] && [ "$answer" != "y" ] && [ "$answer" != "Y" ]
do
    echo $q
    read answer
done
if [ "$answer" != "N" ] && [ "$answer" != "n" ] ; then
    echo "Extracting TBB files needed for CnC..."
    if ! cat $TBBVER_cnc_files.tbz | tar xj -C $ip -f - ; then
	echo "Problems when installing TBB files. Please check TBB installation."
	TBBFiles=""
    else
	echo "Done extracting TBB files."
	TBBFiles="$TBBVER/bin/tbbvars.sh $TBBVER/bin/tbbvars.csh"
    fi
else
    echo "Skipping installing TBB files."
fi
echo ""

CnCFiles="bin/cncvars.sh bin/cncvars.csh"
for file in $CnCFiles $TBBFiles; do
    if [ -f "$ip/$file" ] ; then
	echo "Setting up $ip/$file"
	mv $ip/$file $ip/$file.org
	dir=`dirname $ip/$file.org`
	dir=`dirname ${dir}`
	cat $ip/$file.org | sed -e "s|SUBSTITUTE_INSTALL_DIR_HERE|$dir|g"\
                                -e "s|SUBSTITUTE_TBB_INSTALL_DIR_HERE|$TBBVER|g"\
                                -e "s|SUBSTITUTE_INTEL64_ARCH_HERE|cc4.1.0_libc2.4_kernel2.6.16.21/|g" > $ip/$file
	rm -f $ip/$file.org
	chmod a+x $ip/$file
    else
	echo "Couldn't find file $ip/$file. Your installation might be incomplete."
	echo "Please contact the Intel support team if problems with the installation exist."
    fi
done

echo "Setting permissions in $ip"
find $ip -type d | xargs chmod a+x
find $ip | xargs chmod a+r

echo ""
echo "You have successfully installed Intel(R) Concurrent Collections for C++."
echo ""
echo "Before using CnC it is recommended to source one of $CnCFiles (bash or t/csh)."
echo "It will set up your environment properly."
exit 0
