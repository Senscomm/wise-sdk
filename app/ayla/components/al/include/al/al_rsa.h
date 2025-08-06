/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_AL_COMMON_RSA_H__
#define __AYLA_AL_COMMON_RSA_H__

#include <al/al_utypes.h>

/**
 * @file
 * RSA Cryptography Interfaces.
 */

/**
 * Platform-defined context structure for RSA operations.
 */
struct al_rsa_ctxt;

/**
 * Allocate an RSA context.
 *
 * The RsA context is used in all RSA operations and can be used multiple
 * times until it is freed.
 *
 * \returns an RSA context pointer or NULL on allocation failure.
 */
struct al_rsa_ctxt *al_rsa_ctxt_alloc(void);

/**
 * Free an RSA context.
 *
 * \param ctxt points to the RSA context structure.  It may be NULL.
 */
void al_rsa_ctxt_free(struct al_rsa_ctxt *ctxt);

/**
 * Set public RSA key from binary ASN-1 sequence buffer.
 *
 * \param ctxt points to the RSA context to be initialized.
 * \param key points to the public key in ASN-1 format.
 * \param keylen is the length of the key in bytes.
 * \returns RSA key length in bytes. 0 on failure.
 *
 * The caller must call al_rsa_key_clear(), even on failure.
 */
size_t al_rsa_pub_key_set(struct al_rsa_ctxt *ctxt,
			const void *key, size_t keylen);

/**
 * Set private RSA key from binary ASN-1 sequence buffer.
 *
 * \param ctxt points to the RSA context to be initialized.
 * \param key points to the private key in ASN-1 format.
 * \param keylen is the length of the key in bytes.
 * \returns RSA key length in bytes. 0 on failure.
 *
 * The caller must call al_rsa_key_clear(), even on failure.
 */
size_t al_rsa_prv_key_set(struct al_rsa_ctxt *ctxt,
			const void *key, size_t keylen);

/**
 * Erase and free key material from the RSA context.
 *
 * \param ctxt points to the RSA context to be uninitialized.
 */
void al_rsa_key_clear(struct al_rsa_ctxt *ctxt);

/**
 * Perform RSA encryption. It can be used to make signature or
 * encryption. If RSA private key is used, the output is signature.
 * If is RSA public is used, the output is encrypt data.
 *
 * \param ctx points to the RSA context, initialized with the key.
 * \param in points to the input buffer.
 * \param in_len gives the input buffer length, in bytes. Note: in_len must
 * be less than or equal to (RSA_key_size_in_bytes - 11).
 * \param out points to the output buffer. If out is null, the return is the
 * output buffer size required.
 * \param out_len gives the output buffer length, in bytes.
 * \returns the length of the buffer used, or -1 on error.
 */
ssize_t al_rsa_encrypt(struct al_rsa_ctxt *ctx,
			const void *in, size_t in_len,
			void *out, size_t out_len);

/**
 * Perform an RSA decryption. It can be used to verify signature or make
 * decryption. If RSA public is used, it is used to verify a signature.
 * When RSA private is used, it is used to decrypt data.
 *
 * \param ctxt points to the RSA context, initialized with the key.
 * \param in points to the input buffer.
 * \param in_len gives the input buffer length, in bytes. Note: in_len must
 * be equal to the RSA key size in bytes.
 * \param out points to the output buffer. If out is null, it's return value
 * is the output buffer size required.
 * \param out_len gives the output buffer length, in bytes. It must be greater
 * than or equal to (RSA_key_size_in_bytes - 11).
 * \returns the length of the buffer used, or -1 on error.
 */
ssize_t al_rsa_decrypt(struct al_rsa_ctxt *ctxt,
		const void *in, size_t in_len,
		void *out, size_t out_len);

/**
 * Make signature. Please set RSA private key to the context before
 * calling al_rsa_sign().
 * See al_rsa_encrypt for parameters.
 */
#define al_rsa_sign(ctxt, in, ilen, out, osize) \
	al_rsa_encrypt(ctxt, in, ilen, out, osize);

/**
 * Verify signature. Please set RSA public key to the context before
 * calling al_rsa_verify().
 * See al_rsa_decrypt for parameters.
 */
#define al_rsa_verify(ctx, in, ilen, out, osize) \
	al_rsa_decrypt(ctx, in, ilen, out, osize)

/**
 * Make encryption. Please set RSA public key to the context before
 * calling al_rsa_encrypt_pub().
 * See al_rsa_encrypt for parameters.
 */
#define al_rsa_encrypt_pub(ctx, in, ilen, out, osize) \
	al_rsa_encrypt(ctx, in, ilen, out, osize)

/**
 * Make decryption. Please set RSA private key to the context before
 * calling al_rsa_decrypt_prv().
 * See al_rsa_decrypt for parameters.
 */
#define al_rsa_decrypt_prv(ctx, in, ilen, out, osize) \
	al_rsa_decrypt(ctx, in, ilen, out, osize)

#endif /* __AYLA_AL_COMMON_RSA_H__ */
