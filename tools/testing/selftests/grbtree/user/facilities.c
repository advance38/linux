/* facilities.c - userspace facilities used by common.c/h
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "common.h"

long run_test(unsigned int count);

static struct timezone tz;

void facilities_init(void)
{
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;

	memset(&objects, 0, sizeof(objects));
}

u64 getCurTicks(void) {
	struct timeval now;
	gettimeofday(&now, &tz);
	return 1000000LL * now.tv_sec + now.tv_usec;
}

/* We aren't actually allocating anything here, unlike the kernelspace version
 * but a null pointer means error, so we'll return one instead.
 */
void *rand_init(u64 *seed)
{
	BUG_ON(!seed);

	if (!*seed)
		*seed = getCurTicks();

	/* reduce to 32 bits */
	*seed = (*seed & 0xffffffff) ^ (*seed >> 32);
	srand(*seed & 0xffffffff);

	return (void*)1;
}
