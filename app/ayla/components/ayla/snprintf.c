/*
 * Copyright 2011 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdio.h>
#include <ayla/utypes.h>
#include <ayla/snprintf.h>

int snprintf(char *buf, size_t len, const char *fmt, ...)
{
	ADA_VA_LIST args;

	ADA_VA_START(args, fmt);
	len = vsnprintf(buf, len, fmt, args);
	ADA_VA_END(args);
	return len;
}

