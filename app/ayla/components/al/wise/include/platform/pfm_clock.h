/*
 * Copyright 2021 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_CLOCK_H__
#define __AYLA_PFM_CLOCK_H__

#include <al/al_utypes.h>

/**
 * @file
 * Platform clock interfaces.
 */

/**
 * Initialize clock system and save the device's startup time.
 * Obsolete: use al_clock_init().
 */
void pfm_clock_init(void);

/**
 * Finalize clock system and destroy the mutex.
 */
void pfm_clock_final(void);

/**
 * Set the clock source.  Used by SNTP.
 *
 * \param clock_src is the clock source used to set the platform clock.
 */
void pfm_clock_source_set(enum al_clock_src clock_src);

#endif /* __AYLA_PFM_CLOCK_H__ */
