#!/bin/bash

# This script is designed for use on Gentoo systems, using gcc-config to
# change the compiler and must be run as root. I'm lazy, so alter to fit your
# system.

user=daniel
outfile=runtests.$$.out

rm -f runtest.log runtest.out

if [[ -e ${outfile} ]]; then
	echo "File ${outfile} exists, please move it out of the way."
	exit
fi


for ((gcc_inst_num = 1; gcc_inst_num < 10; ++gcc_inst_num)); do
	gcc-config $gcc_inst_num || exit
	. /etc/profile
	nice -n -3 sudo -Hu ${user}	\
		key_type=u32		\
		use_leftmost=1		\
		use_rightmost=1		\
		use_count=1		\
		unique_keys=1		\
		insert_replaces=0	\
		./runtest.sh >> ${outfile}

done
