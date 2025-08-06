/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <ayla/assert.h>
#include <mbedtls/sha256.h>
#include <al/al_hash_sha256.h>
#include <al/al_os_mem.h>

struct al_hash_sha256_ctxt {
	mbedtls_sha256_context ctxt;
};

struct al_hash_sha256_ctxt *al_hash_sha256_ctxt_alloc(void)
{
	return al_os_mem_calloc(sizeof(struct al_hash_sha256_ctxt));
}

void al_hash_sha256_ctxt_free(struct al_hash_sha256_ctxt *ctxt)
{
	al_os_mem_free(ctxt);
}

void al_hash_sha256_ctxt_init(struct al_hash_sha256_ctxt *ctxt)
{
	mbedtls_sha256_init(&ctxt->ctxt);
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha256_starts(&ctxt->ctxt, 0);
#else
	mbedtls_sha256_starts_ret(&ctxt->ctxt, 0);
#endif
}

void al_hash_sha256_add(struct al_hash_sha256_ctxt *ctxt,
			const void *buf, size_t len)
{
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha256_update(&ctxt->ctxt, buf, len);
#else
	mbedtls_sha256_update_ret(&ctxt->ctxt, buf, len);
#endif
}

void al_hash_sha256_final(struct al_hash_sha256_ctxt *ctxt, void *buf)
{
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha256_finish(&ctxt->ctxt, buf);
#else
	mbedtls_sha256_finish_ret(&ctxt->ctxt, buf);
#endif
	mbedtls_sha256_free(&ctxt->ctxt);	/* zeros, does not free */
}
