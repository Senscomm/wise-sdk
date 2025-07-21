/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <mbedtls/ctr_drbg.h>
#include <al/al_random.h>
#include <platform/pfm_random.h>
#include "pfm_rng.h"

static mbedtls_ctr_drbg_context pfm_random_ctxt;

/*
 * Initialize the random generator context, if needed, and return the pointer.
 */
mbedtls_ctr_drbg_context *pfm_rng_mbedtls_drbg(void)
{
	mbedtls_ctr_drbg_context *rng = &pfm_random_ctxt;
	static u8 done;
	int rc;

	if (done) {
		return rng;
	}
	done = 1;
	rc = pfm_random_init();
	if (rc) {
		log_put(LOG_ERR "%s: pfm_random_init failed", __func__);
		return NULL;
	}

	mbedtls_ctr_drbg_init(rng);
	rc = mbedtls_ctr_drbg_seed(rng, pfm_random_fill, NULL, NULL, 0);
	if (rc) {
		log_put(LOG_ERR "%s: drbg_seed failed rc -%x", __func__, rc);
		return NULL;
	}
	return rng;
}

int al_random_fill(void *buf, size_t len)
{
	mbedtls_ctr_drbg_context *rng;
	size_t tlen;
	int err;

	rng = pfm_rng_mbedtls_drbg();
	if (!rng) {
		return -1;
	}
	while (len) {
		tlen = MBEDTLS_CTR_DRBG_MAX_REQUEST;
		if (tlen > len) {
			tlen = len;
		}
		err = mbedtls_ctr_drbg_random(rng, buf, len);
		ASSERT(!err);
		if (err) {
			return -1;
		}
		len -= tlen;
		buf = (char *)buf + tlen;
	}
	return 0;
}
