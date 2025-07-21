/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <platform/pfm_random.h>

#include "scm_crypto.h"

int pfm_random_init(void)
{
	return 0;
}

int pfm_random_fill(void *arg, unsigned char *buf, size_t len)
{
	int ret;

	ret = scm_crypto_trng_read(buf, len);

	return ret;
}
