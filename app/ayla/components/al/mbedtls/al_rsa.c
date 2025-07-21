/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include <al/al_rsa.h>
#include <al/al_random.h>
#include <al/al_os_mem.h>
#include <platform/pfm_random.h>
#include "pfm_rng.h"

struct al_rsa_ctxt {
	mbedtls_pk_context key;
	u8 private_key;			/* non-zero if private key */
};

struct al_rsa_ctxt *al_rsa_ctxt_alloc(void)
{
	return al_os_mem_calloc(sizeof(struct al_rsa_ctxt));
}

void al_rsa_ctxt_free(struct al_rsa_ctxt *ctxt)
{
	mbedtls_pk_free(&ctxt->key);
	al_os_mem_free(ctxt);
}

size_t al_rsa_pub_key_set(struct al_rsa_ctxt *ctxt,
			const void *key, size_t keylen)
{
	mbedtls_pk_init(&ctxt->key);
	if (mbedtls_pk_parse_public_key(&ctxt->key, key, keylen) != 0) {
		return 0;
	}
	ctxt->private_key = 0;
	return mbedtls_pk_get_len(&ctxt->key);
}

size_t al_rsa_prv_key_set(struct al_rsa_ctxt *ctxt,
			const void *key, size_t keylen)
{
	int ret;

	mbedtls_pk_init(&ctxt->key);
#if MBEDTLS_VERSION_MAJOR >= 3
	ret = mbedtls_pk_parse_key(&ctxt->key, key, keylen, NULL, 0,
	    pfm_random_fill, NULL);
#else
	ret = mbedtls_pk_parse_key(&ctxt->key, key, keylen, NULL, 0);
#endif
	if (ret) {
		return 0;
	}
	ctxt->private_key = 1;
	return mbedtls_pk_get_len(&ctxt->key);
}

void al_rsa_key_clear(struct al_rsa_ctxt *ctxt)
{
	mbedtls_pk_free(&ctxt->key);
}

ssize_t al_rsa_encrypt(struct al_rsa_ctxt *ctxt,
	const void *in_buf, size_t in_len,
	void *out_buf, size_t out_size)
{
	size_t olen = 0;
	mbedtls_ctr_drbg_context *rng;
	int rc;

	rng = pfm_rng_mbedtls_drbg();
	if (!rng) {
		return -1;
	}

#if MBEDTLS_VERSION_MAJOR >= 3
	if (ctxt->private_key) {
		rc = mbedtls_pk_sign(&ctxt->key, MBEDTLS_MD_NONE,
		    in_buf, in_len,
		    out_buf, out_size, &olen,
		    mbedtls_ctr_drbg_random, rng);
	} else
#endif
	{
		rc = mbedtls_pk_encrypt(&ctxt->key, in_buf, in_len,
		    out_buf, &olen, out_size,
		    mbedtls_ctr_drbg_random, rng);
	}
	if (rc) {
		return -1;
	}
	return (ssize_t)olen;
}

#if MBEDTLS_VERSION_MAJOR >= 3
static ssize_t pfm_rsa_decrypt_priv(struct al_rsa_ctxt *ctxt,
			const void *in, size_t in_len,
			void *out, size_t out_len)
#else
ssize_t al_rsa_decrypt(struct al_rsa_ctxt *ctxt,
			const void *in, size_t in_len,
			void *out, size_t out_len)
#endif
{
	size_t output_len = 0;
	int ret;
	mbedtls_ctr_drbg_context *rng;

	rng = pfm_rng_mbedtls_drbg();
	if (!rng) {
		return -1;
	}

#if MBEDTLS_VERSION_MAJOR >= 3
	ret = mbedtls_rsa_pkcs1_decrypt(mbedtls_pk_rsa(ctxt->key),
	    mbedtls_ctr_drbg_random, rng,
	    &output_len, in, out, out_len);
#else
	ret = mbedtls_rsa_pkcs1_decrypt((mbedtls_rsa_context *)ctxt->key.pk_ctx,
	    mbedtls_ctr_drbg_random, rng,
	    ctxt->private_key ? MBEDTLS_RSA_PRIVATE : MBEDTLS_RSA_PUBLIC,
	    &output_len, in, out, out_len);
#endif
	if (ret != 0) {
		log_put(LOG_ERR "%s: decrypt failed rc -%#x", __func__, -ret);
		return -1;
	}
	return (ssize_t)output_len;
}

#if MBEDTLS_VERSION_MAJOR >= 3
ssize_t al_rsa_decrypt(struct al_rsa_ctxt *ctxt,
			const void *in, size_t in_len,
			void *out, size_t out_len)
{
	ssize_t output_len = -1;
	size_t len;
	int ret;
	u8 *buf;
	u8 *bp;

	if (ctxt->private_key) {
		return pfm_rsa_decrypt_priv(ctxt, in, in_len, out, out_len);
	}

	len = mbedtls_pk_get_len(&ctxt->key);
	if (len != in_len) {
		return -1;
	}
	buf = al_os_mem_alloc(len);
	if (!buf) {
		return -1;
	}

	/*
	 * mbedTLS 3.0 removed the ability to decrypt with the public
	 * key via the normal APIs.
	 * Use the lower-level primitive and remove the padding here.
	 */
	ret = mbedtls_rsa_public(mbedtls_pk_rsa(ctxt->key), in, buf);
	if (ret) {
		log_put(LOG_ERR "%s: decrypt failed rc -%#x", __func__, -ret);
		goto free;
	}

	/*
	 * Remove padding.
	 */
	if (buf[0] ||
	    (buf[1] != MBEDTLS_RSA_SIGN && buf[1] !=  MBEDTLS_RSA_CRYPT)) {
		goto free;
	}
	for (bp = buf + 2; bp < buf + len; bp++) {
		if (!*bp) {
			break;
		}
	}
	bp++;
	if (bp >= buf + len) {
		goto free;
	}
	output_len = buf + len - bp;
	if (output_len > out_len) {
		output_len = -1;
		goto free;
	}
	memcpy(out, bp, output_len);
free:
	al_os_mem_free(buf);
	return output_len;
}
#endif /* MBEDTLS_VERSION_MAJOR */
