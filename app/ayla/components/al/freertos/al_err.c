/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stddef.h>
#include <ayla/utypes.h>
#include <al/al_err.h>

static const char *al_err_str[] = AL_ERR_STRINGS;

const char *al_err_string(enum al_err err)
{
	const char *msg = NULL;

	if ((unsigned)err < ARRAY_LEN(al_err_str)) {
		msg = al_err_str[err];
	}
	if (!msg) {
		msg = "unknown";
	}
	return msg;
}

