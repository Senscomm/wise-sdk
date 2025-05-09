/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007-2008 Sam Leffler, Errno Consulting
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
#ifndef _NET80211_IEEE80211_HOSTAP_H_
#define _NET80211_IEEE80211_HOSTAP_H_

/*
 * Hostap implementation definitions.
 */
void _ieee80211_hostap_attach(struct ieee80211com *);

extern void
 (*ieee80211_hostap_attach)(struct ieee80211com *);
void ieee80211_hostap_detach(struct ieee80211com *);

/*
 * This method can be overridden
 */
void _ieee80211_recv_pspoll(struct ieee80211_node *, struct mbuf *);

extern void
 (*ieee80211_recv_pspoll)(struct ieee80211_node *, struct mbuf *);

int
_ieee80211_parse_rsn(struct ieee80211vap *vap, const uint8_t * frm,
                     struct ieee80211_rsnparms *rsn,
                     const struct ieee80211_frame *wh);

extern int
 (*ieee80211_parse_rsn)(struct ieee80211vap * vap, const uint8_t * frm,
                        struct ieee80211_rsnparms * rsn,
                        const struct ieee80211_frame * wh);

#endif                          /* !_NET80211_IEEE80211_HOSTAP_H_ */
