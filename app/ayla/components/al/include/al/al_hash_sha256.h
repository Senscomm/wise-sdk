/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_HASH_SHA256_H__
#define __AYLA_AL_COMMON_HASH_SHA256_H__

#include <al/al_utypes.h>

/**
 * @file
 * SHA-256 Cryptographic Hash Interfaces.
 */

/**
 * Size of SHA-256 hash.
 */
#define AL_HASH_SHA256_SIZE	32

/**
 * Platform-defined context structure for SHA-256 operations.
 */
struct al_hash_sha256_ctxt;

/**
 * Allocate a SHA-256 context.
 *
 * The SHA-1 context is used in all SHA-1 operations and can be used multiple
 * times until it is freed.
 *
 * \returns an SHA-256 context pointer or NULL on allocation failure.
 */
struct al_hash_sha256_ctxt *al_hash_sha256_ctxt_alloc(void);

/**
 * Free a SHA-256 context.
 *
 * \param ctxt points to the SHA-256 context structure.  It may be NULL.
 */
void al_hash_sha256_ctxt_free(struct al_hash_sha256_ctxt *ctxt);

/**
 * Initialize a platform-defined structure to start accumulating a SHA-256 hash.
 *
 * \param ctxt points to the context to be initialized.  This can be allocated
 * on the stack by the application.
 */
void al_hash_sha256_ctxt_init(struct al_hash_sha256_ctxt *ctxt);

/**
 * Accumulate a SHA-256 hash by incorporating the supplied buffer into
 * the previously-computed hash.
 *
 * \param ctxt points to the initialized SHA-256 context.
 * \param buf points to the buffer to use as input for the hash.
 * \param len is the length of the buffer.
 */
void al_hash_sha256_add(struct al_hash_sha256_ctxt *ctxt,
			const void *buf, size_t len);

/**
 * Retreive the SHA-256 hash result
 *
 * Puts the resulting 32-byte SHA-256 hash in the buffer.
 *
 * \param ctxt points to the initialized SHA-256 context.
 * \param buf points to the buffer to receive the 32-byte SHA-256 hash result.
 */
void al_hash_sha256_final(struct al_hash_sha256_ctxt *ctxt, void *buf);

#endif /* __AYLA_AL_COMMON_HASH_SHA256_H__ */
