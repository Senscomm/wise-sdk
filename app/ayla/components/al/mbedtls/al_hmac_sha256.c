/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <al/al_utypes.h>
#include <al/al_hmac_sha256.h>
#include <al/al_os_mem.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include <ayla/assert.h>

struct al_hmac_ctx {
	mbedtls_md_context_t ctx;
};

struct al_hmac_ctx *al_hmac_sha256_new(void)
{
	struct al_hmac_ctx *ctx;

	ctx = al_os_mem_calloc(sizeof(*ctx));
	return ctx;
}

void  al_hmac_sha256_free(struct al_hmac_ctx *ctx)
{
	al_os_mem_free(ctx);
}

void al_hmac_sha256_init(struct al_hmac_ctx *ctx, const void *key, size_t size)
{
	ASSERT(ctx);
	mbedtls_md_init(&ctx->ctx);
	mbedtls_md_setup(&ctx->ctx,
	    mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1); /* use hmac */
	mbedtls_md_hmac_starts(&ctx->ctx, key, size);
}

void al_hmac_sha256_update(struct al_hmac_ctx *ctx, const void *buf, size_t len)
{
	ASSERT(ctx);
	ASSERT(buf);
	mbedtls_md_hmac_update(&ctx->ctx, buf, len);
}

void al_hmac_sha256_final(struct al_hmac_ctx *ctx, void *buf)
{
	ASSERT(ctx);
	ASSERT(buf);
	mbedtls_md_hmac_finish(&ctx->ctx, buf);
	mbedtls_md_free(&ctx->ctx);
}
