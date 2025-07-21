/*
 * Copyright 2017 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_OS_MEM_H__
#define __AYLA_PFM_OS_MEM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Private public functions defined in al_os_mem.c
 */

/**
 * Memory pool (or heap) size.
 */
#define PFM_MEM_POOL0_SIZE	(32 * 1024)
#define PFM_MEM_POOL1_SIZE	(128 * 1024)

/**
 * Initialize memory pool and memory manager.
 */
void  pfm_os_mem_init(void);

/**
 * Memory manager finalize.
 */
void  pfm_os_mem_final(void);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_PFM_OS_MEM_H__ */
