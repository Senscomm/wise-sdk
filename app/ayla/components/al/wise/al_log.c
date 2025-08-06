/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <hal/console.h>

/*
 * Low-level output function.
 *
 * Using newlib's printf is not a good option since it tries to allocate
 * and initialize a lock, which may fail in low-memory situations.
 *
 * The caller supplies '\n', which should be converted to '\r\n' (CR/LF).
 */
void al_log_print(const char *str)
{
	printf("%s", str);
}

void *al_log_buf_alloc(size_t size)
{
	void *buf;

	printf("allocating log buf size=%d\n", size);

	/* TODO: what if aligned support is needed? for e.g. log-snap */
	buf = malloc(size);

	return buf;
}
