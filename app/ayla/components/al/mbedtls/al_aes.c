/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ayla/assert.h>
#include <ayla/log.h>
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include <al/al_aes.h>

#define AES_BLOCK_SIZE 16

struct al_aes_ctxt {
	mbedtls_aes_context ctxt;
	unsigned char iv[AES_BLOCK_SIZE];
};

struct al_aes_ctxt *al_aes_ctxt_alloc(void)
{
	return calloc(1, sizeof(struct al_aes_ctxt));
}

void al_aes_ctxt_free(struct al_aes_ctxt *ctxt)
{
	if (ctxt) {
		memset(ctxt, 0, sizeof(*ctxt));		/* overkill, perhaps */
	}
	free(ctxt);
}

int al_aes_cbc_key_set(struct al_aes_ctxt *aes,
		void *key, size_t key_len, void *iv, int decrypt)
{
	if (!decrypt) {
		if (mbedtls_aes_setkey_enc(&aes->ctxt, key, key_len * 8)) {
			log_put(LOG_ERR "mbedtls_aes_setkey_enc failed");
			return -1;
		}
	} else {
		if (mbedtls_aes_setkey_dec(&aes->ctxt, key, key_len * 8)) {
			log_put(LOG_ERR "mbedtls_aes_setkey_dec failed");
			return -1;
		}
	}
	memcpy(aes->iv, iv, sizeof(aes->iv));
	return 0;
}

int al_aes_iv_get(struct al_aes_ctxt *aes, void *buf, size_t len)
{
	if (len > sizeof(aes->iv)) {
		len = sizeof(aes->iv);
	}
	memcpy(buf, aes->iv, len);
	return 0;
}

int al_aes_cbc_encrypt(struct al_aes_ctxt *aes,
			const void *in, void *out, size_t len)
{
	if (mbedtls_aes_crypt_cbc(&aes->ctxt,
			MBEDTLS_AES_ENCRYPT, len, aes->iv, in, out) < 0) {
		return -1;
	}
	return 0;
}

int al_aes_cbc_decrypt(struct al_aes_ctxt *aes,
			const void *in, void *out, size_t len)
{
	if (mbedtls_aes_crypt_cbc(&aes->ctxt,
			MBEDTLS_AES_DECRYPT, len, aes->iv, in, out) < 0) {
		return -1;
	}
	return 0;
}
