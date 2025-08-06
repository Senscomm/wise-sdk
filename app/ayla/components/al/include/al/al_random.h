/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_RANDOM_H__
#define __AYLA_AL_COMMON_RANDOM_H__

#include <al/al_utypes.h>

/**
 * @file
 * Random Number Generator Interfaces.
 */

/**
 * Fills the supplied buffer with random bytes.
 *
 * This interface should initialize the RNG only if it has not
 * already been initialized.  The platform is expected to be able
 * to supply sufficient entropy for the RNG.  Consider using sources
 * such as packet sizes and arrival intervals to increase entropy if needed.
 *
 * \param buf the buffer to hold the random result.
 * \param len the length of the buffer.
 * \returns 0 on success, and -1 if a random number generator
 * is not available on the platform.
 */
int al_random_fill(void *buf, size_t len);

#endif /* __AYLA_AL_COMMON_RANDOM_H__ */
