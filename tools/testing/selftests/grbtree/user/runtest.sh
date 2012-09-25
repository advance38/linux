#!/bin/bash

# runtest.sh - script to compile and run userspace tests of gerneric red-black
#              tree implementation for a single compiler.
#
# Copyright (C) 2012  Daniel Santos <daniel.santos@pobox.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

# Variables affecting code generation
CC="${CC:-gcc}"
KERNELDIR="${KERNELDIR:-../../../../..}"
CFLAGS="${CFLAGS:-"-O2 -pipe -march=k8"}"
# CONFIG parameters:
key_type=${key_type:-u64}
use_leftmost=${use_leftmost:-0}
use_rightmost=${use_rightmost:-0}
use_count=${use_count:-0}
unique_keys=${unique_keys:-0}
insert_replaces=${insert_replaces:-0}
augmented=${augmented:-0}
# For test_num, see grbtest -h
test_num=${test_num:-0}

# Variables passed to grbtest program
reps=0x20000
count=0x800
keymask=0xfff

# Output files
logfile=runtest.log
datafile=runtest.out

build_desc[0]="hand-coded"
build_desc[1]="generic"

die() {
	echo "ERROR${@:+": "}$@" 1>&2
	exit -1
}

. /etc/profile || die

do_cpp() {
	echo "$1" > /tmp/gnucver.$$.c || die
	${CC} -E /tmp/gnucver.$$.c | grep -v '^#' | tr -d ' '
	rm /tmp/gnucver.$$.c
}

gccverstr=$(do_cpp "__GNUC__.__GNUC_MINOR__.__GNUC_PATCHLEVEL__") || die

execute_tests() {
	for build_type in 1 0; do
		CONFIG=$(echo						\
			-DGRBTEST_KEY_TYPE=${key_type}			\
			-DGRBTEST_BUILD_GENERIC=${build_type}		\
			-DGRBTEST_USE_LEFTMOST=${use_leftmost}		\
			-DGRBTEST_USE_RIGHTMOST=${use_rightmost}	\
			-DGRBTEST_USE_COUNT=${use_count}		\
			-DGRBTEST_UNIQUE_KEYS=${unique_keys}		\
			-DGRBTEST_INSERT_REPLACES=${insert_replaces}	\
			-DGRBTEST_USE_AUGMENTED=${augmented}		\
		)

		echo "********************************************************"
		echo "Starting build at $(date '+%Y-%m-%d %H:%M:%S')..."
		echo "  build_type = ${build_desc[${build_type}]}"
		echo "  compiler   = ${gccverstr}"
		echo "  CFLAGS     = ${CFLAGS}"
		echo "  KERNELDIR  = ${KERNELDIR}"
		echo

		#set -x
		CC="${CC}"			\
		CFLAGS="${CFLAGS}"		\
		CONFIG="${CONFIG}"		\
		KERNELDIR="${KERNELDIR}"	\
		make clean all || die
		set +x

		echo
		echo "Executing test..."
		echo
		./grbtest --seed 1		\
			  --reps ${reps}	\
			  --count ${count}	\
			  --keymask ${keymask}	\
			  --delim "|"		\
			  --quote ""		\
			  --test ${test_num} | tee -a "${datafile}"
		echo
		echo
	done
}

execute_tests | tee -a ${logfile}
