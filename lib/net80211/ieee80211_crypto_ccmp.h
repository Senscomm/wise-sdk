/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_CRYPTO_CCMP_H_
#define _NET80211_IEEE80211_CRYPTO_CCMP_H_

#include <net80211/ieee80211_var.h>

#ifdef __WISE__
#include <net80211/ieee80211_crypto_rijndael.h>
#else
#include <crypto/rijndael/rijndael.h>
#endif

#define AES_BLOCK_LEN 16

void *_ccmp_attach(struct ieee80211vap *, struct ieee80211_key *);
extern void *
 (*ccmp_attach)(struct ieee80211vap *, struct ieee80211_key *);

void _ccmp_detach(struct ieee80211_key *);
extern void
 (*ccmp_detach)(struct ieee80211_key *);

int _ccmp_setkey(struct ieee80211_key *);
extern int
 (*ccmp_setkey)(struct ieee80211_key *);

void _ccmp_setiv(struct ieee80211_key *, uint8_t *);
extern void
 (*ccmp_setiv)(struct ieee80211_key *, uint8_t *);

int _ccmp_encap(struct ieee80211_key *, struct mbuf *);
extern int
 (*ccmp_encap)(struct ieee80211_key *, struct mbuf *);

int _ccmp_enmic(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*ccmp_enmic)(struct ieee80211_key *, struct mbuf *, int);

int _ccmp_decap(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*ccmp_decap)(struct ieee80211_key *, struct mbuf *, int);

int _ccmp_demic(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*ccmp_demic)(struct ieee80211_key *, struct mbuf *, int);

#ifdef CONFIG_IEEE80211W
int _ccmp_encrypt(struct ieee80211_key *key, struct mbuf *m0,
              int hdrlen, int mfp);
extern int (*ccmp_encrypt)(struct ieee80211_key * key, struct mbuf * m0,
                 int hdrlen, int mfp);
int _ccmp_decrypt(struct ieee80211_key *, u_int64_t pn, struct mbuf *,
              int hdrlen, int mfp);
extern int
(*ccmp_decrypt)(struct ieee80211_key *, u_int64_t pn, struct mbuf *,
                 int hdrlen, int mfp);

void _ccmp_init_blocks(rijndael_ctx * ctx, struct ieee80211_frame *wh,
                  u_int64_t pn, size_t dlen,
                  uint8_t b0[AES_BLOCK_LEN],
                  uint8_t aad[2 * AES_BLOCK_LEN],
                  uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN],
                  uint8_t mfp);

extern void (*ccmp_init_blocks)(rijndael_ctx * ctx, struct ieee80211_frame *wh,
                  u_int64_t pn, size_t dlen,
                  uint8_t b0[AES_BLOCK_LEN],
                  uint8_t aad[2 * AES_BLOCK_LEN],
                  uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN],
                  uint8_t mfp);
#else
int ccmp_encrypt(struct ieee80211_key *, struct mbuf *, int hdrlen);

int ccmp_decrypt(struct ieee80211_key *, u_int64_t pn, struct mbuf *, int hdrlen);

void ccmp_init_blocks(rijndael_ctx * ctx, struct ieee80211_frame *wh,
                  u_int64_t pn, size_t dlen,
                  uint8_t b0[AES_BLOCK_LEN],
                  uint8_t aad[2 * AES_BLOCK_LEN],
                  uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN]);
#endif


#endif /* _NET80211_IEEE80211_CRYPTO_CCMP_H_ */
