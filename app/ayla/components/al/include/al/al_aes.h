/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_AES_H__
#define __AYLA_AL_COMMON_AES_H__

#include <al/al_utypes.h>

/**
 * @file
 * AES Cryptography Interfaces.
 */

/**
 * AES block size.
 */
#define AL_AES_BLOCK_SIZE	16

/**
 * Platform-defined context structure for AES operations.
 */
struct al_aes_ctxt;

/**
 * Allocate an AES context.
 *
 * The AES context is used in all AES operations and can be used multiple
 * times until it is freed.
 *
 * \returns an AES context pointer or NULL on allocation failure.
 */
struct al_aes_ctxt *al_aes_ctxt_alloc(void);

/**
 * Free an AES context.
 *
 * \param ctxt points to the AES context structure.  It may be NULL.
 */
void al_aes_ctxt_free(struct al_aes_ctxt *ctxt);

/**
 * Initialize the AES context with the key and IV for CBC mode.
 *
 * \param ctxt points to the AES context structure.
 * \param key points to the binary AES key.
 * \param key_len gives the key length in bytes.
 * \param iv points to the initialization vector.
 * \param decrypt should be non-zero if setting up for decryption.
 * \returns zero on success, non-zero on error.
 */
int al_aes_cbc_key_set(struct al_aes_ctxt *ctxt,
		void *key, size_t key_len, void *iv, int decrypt);

/**
 * Extract the IV from the AES context.
 *
 * Note: this should be possible on all platforms.
 * For CBC, the IV is the last 16 bytes of ciphertext.
 * The implementation may need to do extra work to keep that available.
 *
 * \param ctxt points to the AES context structure.
 * \param buf points to the buffer to receive the IV.
 * \param len indicates the length of the buffer.
 * \returns zero on success, non-zero on error.
 */
int al_aes_iv_get(struct al_aes_ctxt *ctxt, void *buf, size_t len);


/**
 * Encrypt a block of data with AES.
 *
 * \param ctxt points to the AES context structure.
 * \param in points to the input clear-text data.
 * \param out points to the output buffer to receive the cipher-text.
 * It may be the same as in.
 * \param len gives the length of the in and out buffers.
 * \returns zero on success, non-zero on error.
 */
int al_aes_cbc_encrypt(struct al_aes_ctxt *ctxt,
			const void *in, void *out, size_t len);

/**
 * Decrypt a block of data with AES.
 *
 * \param ctxt points to the AES context structure.
 * \param in points to the input cipher-text data.
 * \param out points to the output buffer to receive the clear-text data.
 * It may be the same as in.
 * \param len gives the length of the in and out buffers.
 * \returns zero on success, non-zero on error.
 */
int al_aes_cbc_decrypt(struct al_aes_ctxt *ctxt,
			const void *in, void *out, size_t len);

#endif /* __AYLA_AL_COMMON_AES_H__ */
