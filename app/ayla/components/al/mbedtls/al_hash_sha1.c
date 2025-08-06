/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <ayla/assert.h>
#include <al/al_hash_sha1.h>
#include <al/al_os_mem.h>
#include <mbedtls/sha1.h>

struct al_hash_sha1_ctxt {
	mbedtls_sha1_context ctxt;
};

struct al_hash_sha1_ctxt *al_hash_sha1_ctxt_alloc(void)
{
	struct al_hash_sha1_ctxt *ctxt;

	ctxt = al_os_mem_calloc(sizeof(*ctxt));
	return ctxt;
}

void al_hash_sha1_ctxt_free(struct al_hash_sha1_ctxt *ctxt)
{
	al_os_mem_free(ctxt);
}

void al_hash_sha1_ctxt_init(struct al_hash_sha1_ctxt *ctxt)
{
	ASSERT(ctxt);

	mbedtls_sha1_init(&ctxt->ctxt);
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha1_starts(&ctxt->ctxt);
#else
	mbedtls_sha1_starts_ret(&ctxt->ctxt);
#endif
}

void al_hash_sha1_add(struct al_hash_sha1_ctxt *ctxt,
			const void *buf, size_t len)
{
	ASSERT(ctxt);
	ASSERT(buf);
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha1_update(&ctxt->ctxt, buf, len);
#else
	mbedtls_sha1_update_ret(&ctxt->ctxt, buf, len);
#endif
}

void al_hash_sha1_final(struct al_hash_sha1_ctxt *ctxt, void *buf)
{
	ASSERT(ctxt);
	ASSERT(buf);
#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_sha1_finish(&ctxt->ctxt, buf);
#else
	mbedtls_sha1_finish_ret(&ctxt->ctxt, buf);
#endif
	mbedtls_sha1_free(&ctxt->ctxt);	 /* zeros context, does not free */
}
