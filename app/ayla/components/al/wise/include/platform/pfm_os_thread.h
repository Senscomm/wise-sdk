/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_OS_THREAD_H__
#define __AYLA_PFM_OS_THREAD_H__

#include <al/al_os_thread.h>
#include <al/al_os_mem.h>

/**
 * @file
 * Private public functions defined in al_os_thread.c
 */

/**
 * Initialize thread manager.
 */
void pfm_os_thread_init(void);

/**
 * Finalize thread manager.
 */
void pfm_os_thread_final(void);

/**
 * Set memory type (or pool) for memory allocation of the current thread.
 *
 * \param type is the memory type.
 */
void pfm_os_thread_set_mem_type(enum al_os_mem_type type);

/**
 * Get memory type (or pool) for memory allocation of the current thread.
 *
 * \returns the memory type.
 */
enum al_os_mem_type pfm_os_thread_get_mem_type(void);

#endif /* __AYLA_PFM_OS_THREAD_H__ */
