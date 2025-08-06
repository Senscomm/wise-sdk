/*
 * Copyright 2015 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <ayla/utypes.h>
#include <ayla/log.h>
#include <ada/err.h>

static const char * const ada_err_strings[] = ADA_ERR_STRINGS;

const char *ada_err_string(enum ada_err err)
{
	int i = -(int)err;

	if (i < 0 || i >= ARRAY_LEN(ada_err_strings)) {
		return "unknown";
	}
	return ada_err_strings[i];
}
