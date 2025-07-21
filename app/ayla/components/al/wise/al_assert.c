/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <assert.h>
#include <ayla/log.h>
#include <al/al_assert.h>

void al_assert_handle(const char *file, int line)
{
	/*
	 * can't use assert() as it is a macro, use hal_assert_fail instead
	 * - provide "ayla" as expression is not passed
	 * - provide the file to the function parameter as well
	 */
	hal_assert_fail("Ayla", file, line, file);
}
