/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#define PFM_OS_MEM_IMPLEMENT

#include <stdlib.h>
#include <al/al_os_mem.h>

void al_os_mem_set_type(enum al_os_mem_type type)
{
}

enum al_os_mem_type al_os_mem_get_type(void)
{
	return al_os_mem_type_long_period;
}

void *al_os_mem_alloc(size_t size)
{
	return malloc(size);
}

void *al_os_mem_calloc(size_t size)
{
	return calloc(1, size);
}

void al_os_mem_free(void *mem)
{
	free(mem);
}
