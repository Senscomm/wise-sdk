/*
 * Copyright 2013 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_RANDOM_H__
#define __AYLA_RANDOM_H__

#include <al/al_random.h>

/*
 * Note: this file is obsolete but kept for source compatibility.
 * It should not be used in new code.
 */

static inline void random_fill(void *buf, size_t len)
{
	al_random_fill(buf, len);
}

#endif /* __AYLA_RANDOM_H__ */
