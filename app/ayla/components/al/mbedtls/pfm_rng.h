/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_RNG_H__
#define __AYLA_PFM_RNG_H__

#include <mbedtls/ctr_drbg.h>

/*
 * Return the mbedTLS RNG context after initializing if needed.
 */
mbedtls_ctr_drbg_context *pfm_rng_mbedtls_drbg(void);

#endif /* __AYLA_PFM_RNG_H__ */

