/*
 * Copyright 2011-2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_ADC_CRYPTO_H__
#define __AYLA_ADC_CRYPTO_H__

/*
 * This file is deprecated but kept for host-app source compatibility.
 * Do not use in new code.
 */

#include <stdlib.h>
#include <string.h>
#include <mbedtls/sha256.h>

#define SHA256_SIG_LEN	32

struct adc_sha256 {
	mbedtls_sha256_context ctx;
};

static inline void adc_sha256_init(struct adc_sha256 *sha_ctx)
{
	mbedtls_sha256_init(&sha_ctx->ctx);
	mbedtls_sha256_starts_ret(&sha_ctx->ctx, 0);
}

static inline void adc_sha256_update(struct adc_sha256 *sha_ctx,
		const void *buf, size_t len, const void *buf2,
		size_t len2)
{
	mbedtls_sha256_update_ret(&sha_ctx->ctx, buf, len);
	if (buf2) {
		mbedtls_sha256_update_ret(&sha_ctx->ctx, buf2, len2);
	}
}

static inline void adc_sha256_final(struct adc_sha256 *sha_ctx,
		void *sign)
{
	mbedtls_sha256_finish_ret(&sha_ctx->ctx, sign);
	mbedtls_sha256_free(&sha_ctx->ctx);
}

#endif /* __AYLA_ADC_CRYPTO_H__ */
