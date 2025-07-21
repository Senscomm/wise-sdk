/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_HMAC_SHA256_H__
#define __AYLA_AL_COMMON_HMAC_SHA256_H__

#include <al/al_utypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * HMAC-SHA256 Cryptographic Hash Interfaces.
 */

/**
 * Size of HMAC-SHA256 hash.
 */
#define AL_HMAC_SHA256_SIZE	32

/**
 * Platform-defined context structure for HMAC-SHA256 operations.
 */
struct al_hmac_ctx;

/**
 * Allocate a HMAC-SHA256 context.
 *
 * The HMAC-SHA256 context is used in all HMAC operations and can be used
 * multiple times until it is freed.
 *
 * \returns an HMAC-SHA256 context pointer or NULL on allocation failure.
 */
struct al_hmac_ctx *al_hmac_sha256_new(void);

/**
 * Free a HMAC-SHA256 context.
 *
 * \param ctx points to the HMAC-SHA256 context structure. It may be NULL.
 */
void al_hmac_sha256_free(struct al_hmac_ctx *ctx);

/**
 * Initialize HMAC-SHA256 context to start accumulating a HNAC-SHA256
 * operation.
 *
 * \param ctx points to the context to be initialized.
 * \param key a key which is used to initialize the context.
 * \param size size fo the key.
 */
void al_hmac_sha256_init(struct al_hmac_ctx *ctx, const void *key,
	size_t size);

/**
 * Accumulate a HMAC-SHA256 hash by incorporating the supplied buffer into
 * the previously-computed hash.
 *
 * \param ctx points to the initialized HMAC-SHA256 context.
 * \param data points to the buffer to use as input for the hash.
 * \param len length of the buffer.
 */
void al_hmac_sha256_update(struct al_hmac_ctx *ctx, const void *data,
	size_t len);

/**
 * Retreive the HMAC-SHA256 hash result.
 *
 * \param ctx points to the initialized HMAC-SHA256 context.
 * \param buf points to the buffer to receive 32-byte HMAC-SHA256 hash result.
 */
void al_hmac_sha256_final(struct al_hmac_ctx *ctx, void *buf);

#ifdef __cplusplus
}
#endif

#endif /* __AYLA_AL_COMMON_HMAC_SHA256_H__ */
