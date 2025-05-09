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
#ifndef _NET80211_IEEE80211_CRYPTO_NONE_H_
#define _NET80211_IEEE80211_CRYPTO_NONE_H_

#include <net80211/ieee80211_var.h>

void *_none_attach(struct ieee80211vap *, struct ieee80211_key *);
extern void *
 (*none_attach)(struct ieee80211vap *, struct ieee80211_key *);

void _none_detach(struct ieee80211_key *);
extern void
 (*none_detach)(struct ieee80211_key *);

int _none_setkey(struct ieee80211_key *);
extern int
 (*none_setkey)(struct ieee80211_key *);

void _none_setiv(struct ieee80211_key *, uint8_t *);
extern void
 (*none_setiv)(struct ieee80211_key *, uint8_t *);

int _none_encap(struct ieee80211_key *, struct mbuf *);
extern int
 (*none_encap)(struct ieee80211_key *, struct mbuf *);

int _none_decap(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*none_decap)(struct ieee80211_key *, struct mbuf *, int);

int _none_enmic(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*none_enmic)(struct ieee80211_key *, struct mbuf *, int);

int _none_demic(struct ieee80211_key *, struct mbuf *, int);
extern int
 (*none_demic)(struct ieee80211_key *, struct mbuf *, int);

#endif /* _NET80211_IEEE80211_CRYPTO_NONE_H_ */