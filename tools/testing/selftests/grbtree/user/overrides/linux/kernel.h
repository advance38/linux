/* kernel.h userspace override hack
 * Copyright (C) 2012  Daniel Santos <daniel.santos@pobox.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This file is an override hack for the file normally found at
 * include/linux/kernel.h in the kernel tree (not /usr) to allow you to compile
 * some portions of the Linux kernel code in user-space and is expected to
 * break fairly often as the internals of the kernel tree change.  It's
 * designed for use with GNU libc and is generally not expected to work
 * elsewhere as-is.
 */

#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

/* avoid warning: __always_inline defined in both /usr/include/sys/cdefs.h as
 * well as linux/compiler.h.  So we include cdefs.h now, #undef it and let
 * linux/compiler.h redefine it.
 */
#include <sys/cdefs.h>
#if defined(__always_inline) && !defined(__LINUX_COMPILER_H)
# undef __always_inline
#endif
#include <linux/compiler.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <linux/err.h>
#include <linux/bug.h>

#ifndef __GNU_LIBRARY__
# warning This file is a hack written to compile with the GNU C Library and \
	  is not generally expected to work elsewhere.
#endif

/* glibc-backed versions of BUG{,_ON} */
#undef BUG
#undef BUG_ON
#define BUG()		assert(0)
#define BUG_ON(arg)	assert(!(arg))

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

#endif /* _LINUX_KERNEL_H */
