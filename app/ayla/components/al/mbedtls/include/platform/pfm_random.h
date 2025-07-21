/*
 * Copyright 2023 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_RANDOM_H__
#define __AYLA_PFM_RANDOM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the platform's hardware RNG.
 *
 * \returns 0 on success, -1 on error.
 */
int pfm_random_init(void);

/**
 * Get random numbers, preferably from hardware.
 *
 * This is occasionally called from mbedTLS.
 * \param arg an unused parameter.
 * \param buf the buffer to fill.
 * \param len the length of data.
 * \returns 0 on success, -1 on error.
 */
int pfm_random_fill(void *arg, unsigned char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_PFM_RANDOM_H__ */

