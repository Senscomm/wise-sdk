/*
 * Copyright 2009, Colin Gunther, coling@gmx.de. All rights reserved.
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
/*-
 * Copyright (c) 2003-2008 Sam Leffler, Errno Consulting
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
 */
#ifndef _FBSD_COMPAT_NET80211_IEEE80211_WISE_H_
#define _FBSD_COMPAT_NET80211_IEEE80211_WISE_H_


#include <stdint.h>

#ifdef __cplusplus
// Those includes are needed to avoid C/C++ function export clashes
#include <new>
#include <thread.h>
extern "C" {
#endif

#define INVARIANTS 1

#include "compat_types.h"
#include "kernel.h"
#include "mutex.h"
#include "taskqueue.h"

/*
 * Standard relevant, but not defined in ieee80211.h
 */

/* Maximum PPDU duration for HT/VHT/HE PPDU. */
#define IEEE80211_PPDU_MAX_TXTIME	5484

/*
 * Common state locking definitions.
 */
typedef struct {
	struct mtx	mtx;
} ieee80211_com_lock_t;

#define	IEEE80211_LOCK_INIT(_ic, _name) do {				\
	ieee80211_com_lock_t *cl = &(_ic)->ic_comlock;			\
	mtx_init(&cl->mtx, _name, NULL, MTX_DEF | MTX_RECURSE);	\
} while (0)
#define	IEEE80211_LOCK_OBJ(_ic)	(&(_ic)->ic_comlock.mtx)
#define	IEEE80211_LOCK_DESTROY(_ic) mtx_destroy(IEEE80211_LOCK_OBJ(_ic))
#define	IEEE80211_LOCK(_ic)	   mtx_lock(IEEE80211_LOCK_OBJ(_ic))
#define	IEEE80211_UNLOCK(_ic)	   mtx_unlock(IEEE80211_LOCK_OBJ(_ic))
#define	IEEE80211_LOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_LOCK_OBJ(_ic), MA_OWNED)
#ifdef __WISE__
#define IEEE80211_UNLOCK_ASSERT(ic)
#else
#define	IEEE80211_UNLOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_LOCK_OBJ(_ic), MA_NOTOWNED)
#endif
#define	IEEE80211_LOCK_IS_LOCKED(_ic) mtx_owned(IEEE80211_LOCK_OBJ(_ic))

/*
 * Transmit lock.
 *
 * This is a (mostly) temporary lock designed to serialise all of the
 * transmission operations throughout the stack.
 */
typedef struct {
	struct mtx	mtx;
} ieee80211_tx_lock_t;
#define	IEEE80211_TX_LOCK_INIT(_ic, _name) do {				\
	ieee80211_tx_lock_t *cl = &(_ic)->ic_txlock;			\
	mtx_init(&cl->mtx, _name, NULL, MTX_DEF);	\
} while (0)
#define	IEEE80211_TX_LOCK_OBJ(_ic)	(&(_ic)->ic_txlock.mtx)
#define	IEEE80211_TX_LOCK_DESTROY(_ic) mtx_destroy(IEEE80211_TX_LOCK_OBJ(_ic))
#define	IEEE80211_TX_LOCK(_ic)	   mtx_lock(IEEE80211_TX_LOCK_OBJ(_ic))
#define	IEEE80211_TX_UNLOCK(_ic)	   mtx_unlock(IEEE80211_TX_LOCK_OBJ(_ic))
#define	IEEE80211_TX_LOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_TX_LOCK_OBJ(_ic), MA_OWNED)
#define	IEEE80211_TX_UNLOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_TX_LOCK_OBJ(_ic), MA_NOTOWNED)

#ifdef IEEE80211_SUPPORT_SUPERG
/*
 * Stageq / ni_tx_superg lock
 */
typedef struct {
	char		name[16];		/* e.g. "ath0_ff_lock" */
	struct mtx	mtx;
} ieee80211_ff_lock_t;
#define IEEE80211_FF_LOCK_INIT(_ic, _name) do {				\
	ieee80211_ff_lock_t *fl = &(_ic)->ic_fflock;			\
	snprintf(fl->name, sizeof(fl->name), "%s_ff_lock", _name);	\
	mtx_init(&fl->mtx, fl->name, NULL, MTX_DEF);			\
} while (0)
#define IEEE80211_FF_LOCK_OBJ(_ic)	(&(_ic)->ic_fflock.mtx)
#define IEEE80211_FF_LOCK_DESTROY(_ic)	mtx_destroy(IEEE80211_FF_LOCK_OBJ(_ic))
#define IEEE80211_FF_LOCK(_ic)		mtx_lock(IEEE80211_FF_LOCK_OBJ(_ic))
#define IEEE80211_FF_UNLOCK(_ic)	mtx_unlock(IEEE80211_FF_LOCK_OBJ(_ic))
#define IEEE80211_FF_LOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_FF_LOCK_OBJ(_ic), MA_OWNED)
#endif

/*
 * Node locking definitions.
 */
typedef struct {
	struct mtx	mtx;
} ieee80211_node_lock_t;
#define	IEEE80211_NODE_LOCK_INIT(_nt, _name) do {			\
	ieee80211_node_lock_t *nl = &(_nt)->nt_nodelock;		\
	mtx_init(&nl->mtx, _name, NULL, MTX_DEF | MTX_RECURSE);	\
} while (0)
#define	IEEE80211_NODE_LOCK_OBJ(_nt)	(&(_nt)->nt_nodelock.mtx)
#define	IEEE80211_NODE_LOCK_DESTROY(_nt) \
	mtx_destroy(IEEE80211_NODE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_LOCK(_nt) \
	mtx_lock(IEEE80211_NODE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_IS_LOCKED(_nt) \
	mtx_owned(IEEE80211_NODE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_UNLOCK(_nt) \
	mtx_unlock(IEEE80211_NODE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_LOCK_ASSERT(_nt)	\
	mtx_assert(IEEE80211_NODE_LOCK_OBJ(_nt), MA_OWNED)

typedef struct {
	struct mtx	mtx;
} ieee80211_twt_lock_t;

#define	IEEE80211_TWT_LOCK_INIT(_ic, _name) do {				\
	ieee80211_twt_lock_t *cl = &(_ic)->ic_twtlock;			\
	mtx_init(&cl->mtx, _name, NULL, MTX_DEF);	\
} while (0)
#define	IEEE80211_TWT_LOCK_OBJ(_ic)	(&(_ic)->ic_twtlock.mtx)
#define	IEEE80211_TWT_LOCK_DESTROY(_ic) mtx_destroy(IEEE80211_TWT_LOCK_OBJ(_ic))
#define	IEEE80211_TWT_LOCK(_ic)	   mtx_lock(IEEE80211_TWT_LOCK_OBJ(_ic))
#define	IEEE80211_TWT_UNLOCK(_ic)	   mtx_unlock(IEEE80211_TWT_LOCK_OBJ(_ic))
#define	IEEE80211_TWT_LOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_TWT_LOCK_OBJ(_ic), MA_OWNED)
#define	IEEE80211_TWT_UNLOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_TWT_LOCK_OBJ(_ic), MA_NOTOWNED)

#ifdef CONFIG_SUPPORT_MCC
typedef struct {
	struct mtx	mtx;
} ieee80211_mcc_lock_t;

#define	IEEE80211_MCC_LOCK_INIT(_ic, _name) do {				\
	ieee80211_mcc_lock_t *cl = &(_ic)->ic_mcclock;			\
	mtx_init(&cl->mtx, _name, NULL, MTX_DEF);	\
} while (0)
#define	IEEE80211_MCC_LOCK_OBJ(_ic)	(&(_ic)->ic_mcclock.mtx)
#define	IEEE80211_MCC_LOCK_DESTROY(_ic) mtx_destroy(IEEE80211_MCC_LOCK_OBJ(_ic))
#define	IEEE80211_MCC_LOCK(_ic)	   mtx_lock(IEEE80211_MCC_LOCK_OBJ(_ic))
#define	IEEE80211_MCC_UNLOCK(_ic)	   mtx_unlock(IEEE80211_MCC_LOCK_OBJ(_ic))
#define	IEEE80211_MCC_LOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_MCC_LOCK_OBJ(_ic), MA_OWNED)
#define	IEEE80211_MCC_UNLOCK_ASSERT(_ic) \
	mtx_assert(IEEE80211_MCC_LOCK_OBJ(_ic), MA_NOTOWNED)
#endif

/*
 * Node table iteration locking definitions; this protects the
 * scan generation # used to iterate over the station table
 * while grabbing+releasing the node lock.
 */
typedef struct {
#ifndef __WISE__
	char		name[16];		/* e.g. "ath0_scan_lock" */
#endif
	struct mtx	mtx;
} ieee80211_scan_lock_t;
#define	IEEE80211_NODE_ITERATE_LOCK_INIT(_nt, _name) do {		\
	ieee80211_scan_lock_t *sl = &(_nt)->nt_scanlock;		\
	mtx_init(&sl->mtx, _name, NULL, MTX_DEF);			\
} while (0)
#define	IEEE80211_NODE_ITERATE_LOCK_OBJ(_nt)	(&(_nt)->nt_scanlock.mtx)
#define	IEEE80211_NODE_ITERATE_LOCK_DESTROY(_nt) \
	mtx_destroy(IEEE80211_NODE_ITERATE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_ITERATE_LOCK(_nt) \
	mtx_lock(IEEE80211_NODE_ITERATE_LOCK_OBJ(_nt))
#define	IEEE80211_NODE_ITERATE_UNLOCK(_nt) \
	mtx_unlock(IEEE80211_NODE_ITERATE_LOCK_OBJ(_nt))

/*
 * Power-save queue definitions.
 */
typedef struct mtx ieee80211_psq_lock_t;
#define	IEEE80211_PSQ_INIT(_psq, _name) \
	mtx_init(&(_psq)->psq_lock, _name, NULL, MTX_DEF)
#define	IEEE80211_PSQ_DESTROY(_psq)	mtx_destroy(&(_psq)->psq_lock)
#define	IEEE80211_PSQ_LOCK(_psq)	mtx_lock(&(_psq)->psq_lock)
#define	IEEE80211_PSQ_UNLOCK(_psq)	mtx_unlock(&(_psq)->psq_lock)

#ifndef IF_PREPEND_LIST
#define _IF_PREPEND_LIST(ifq, mhead, mtail, mcount) do {	\
	(mtail)->m_nextpkt = (ifq)->ifq_head;			\
	if ((ifq)->ifq_tail == NULL)				\
		(ifq)->ifq_tail = (mtail);			\
	(ifq)->ifq_head = (mhead);				\
	(ifq)->ifq_len += (mcount);				\
} while (0)
#define IF_PREPEND_LIST(ifq, mhead, mtail, mcount) do {		\
	IF_LOCK(ifq);						\
	_IF_PREPEND_LIST(ifq, mhead, mtail, mcount);		\
	IF_UNLOCK(ifq);						\
} while (0)
#endif /* IF_PREPEND_LIST */

/*
 * Age queue definitions.
 */
typedef struct mtx ieee80211_ageq_lock_t;
#define	IEEE80211_AGEQ_INIT(_aq, _name) \
	mtx_init(&(_aq)->aq_lock, _name, NULL, MTX_DEF)
#define	IEEE80211_AGEQ_DESTROY(_aq)	mtx_destroy(&(_aq)->aq_lock)
#define	IEEE80211_AGEQ_LOCK(_aq)	mtx_lock(&(_aq)->aq_lock)
#define	IEEE80211_AGEQ_UNLOCK(_aq)	mtx_unlock(&(_aq)->aq_lock)

/*
 * Scan table definitions.
 */
typedef struct mtx ieee80211_scan_table_lock_t;
#define	IEEE80211_SCAN_TABLE_LOCK_INIT(_st, _name) \
	mtx_init(&(_st)->st_lock, _name, NULL, MTX_DEF)
#define	IEEE80211_SCAN_TABLE_LOCK_DESTROY(_st)	mtx_destroy(&(_st)->st_lock)
#define	IEEE80211_SCAN_TABLE_LOCK(_st)		mtx_lock(&(_st)->st_lock)
#define	IEEE80211_SCAN_TABLE_UNLOCK(_st)	mtx_unlock(&(_st)->st_lock)

typedef struct mtx ieee80211_scan_iter_lock_t;
#define	IEEE80211_SCAN_ITER_LOCK_INIT(_st, _name) \
	mtx_init(&(_st)->st_scanlock, _name, "802.11 scangen", MTX_DEF)
#define	IEEE80211_SCAN_ITER_LOCK_DESTROY(_st)	mtx_destroy(&(_st)->st_scanlock)
#define	IEEE80211_SCAN_ITER_LOCK(_st)		mtx_lock(&(_st)->st_scanlock)
#define	IEEE80211_SCAN_ITER_UNLOCK(_st)	mtx_unlock(&(_st)->st_scanlock)

/*
 * Mesh node/routing definitions.
 */
typedef struct mtx ieee80211_rte_lock_t;
#define	MESH_RT_ENTRY_LOCK_INIT(_rt, _name) \
	mtx_init(&(rt)->rt_lock, _name, NULL, MTX_DEF)
#define	MESH_RT_ENTRY_LOCK_DESTROY(_rt) \
	mtx_destroy(&(_rt)->rt_lock)
#define	MESH_RT_ENTRY_LOCK(rt)	mtx_lock(&(rt)->rt_lock)
#define	MESH_RT_ENTRY_LOCK_ASSERT(rt) mtx_assert(&(rt)->rt_lock, MA_OWNED)
#define	MESH_RT_ENTRY_UNLOCK(rt)	mtx_unlock(&(rt)->rt_lock)

typedef struct mtx ieee80211_rt_lock_t;
#define	MESH_RT_LOCK(ms)	mtx_lock(&(ms)->ms_rt_lock)
#define	MESH_RT_LOCK_ASSERT(ms)	mtx_assert(&(ms)->ms_rt_lock, MA_OWNED)
#define	MESH_RT_UNLOCK(ms)	mtx_unlock(&(ms)->ms_rt_lock)
#define	MESH_RT_LOCK_INIT(ms, name) \
	mtx_init(&(ms)->ms_rt_lock, name, NULL, MTX_DEF)
#define	MESH_RT_LOCK_DESTROY(ms) \
	mtx_destroy(&(ms)->ms_rt_lock)

/*
 * Node reference counting definitions.
 *
 * ieee80211_node_initref	initialize the reference count to 1
 * ieee80211_node_incref	add a reference
 * ieee80211_node_decref	remove a reference
 * ieee80211_node_dectestref	remove a reference and return 1 if this
 *				is the last reference, otherwise 0
 * ieee80211_node_refcnt	reference count for printing (only)
 */
#include "atomic.h"

#define ieee80211_node_initref(_ni) \
	do { ((_ni)->ni_refcnt = 1); } while (0)
#define ieee80211_node_incref(_ni) \
	atomic_add_int((int *) &(_ni)->ni_refcnt, 1)
#define	ieee80211_node_decref(_ni) \
	atomic_subtract_int((int *) &(_ni)->ni_refcnt, 1)

struct ieee80211_node;
int	ieee80211_node_dectestref(struct ieee80211_node *ni);

#define	ieee80211_node_refcnt(_ni)	(_ni)->ni_refcnt

struct ifqueue;
struct ieee80211vap;
void	ieee80211_drain_ifq(struct ifqueue *);
void	ieee80211_flush_ifq(struct ifqueue *, struct ieee80211vap *);

void	ieee80211_vap_destroy(struct ieee80211vap *);

#define	IFNET_IS_UP_RUNNING(_ifp) \
	(((_ifp)->if_flags & IFF_UP) && \
	 ((_ifp)->if_drv_flags & IFF_DRV_RUNNING))

/* XXX TODO: cap these at 1, as hz may not be 1000 */
#define	msecs_to_ticks(ms)	(((ms)*hz)/1000)
#define	ticks_to_msecs(t)	(1000*(t) / hz)
#define	ticks_to_secs(t)	((t) / hz)

#define ieee80211_time_after(a,b) 	((long)(b) - (long)(a) < 0)
#define ieee80211_time_before(a,b)	ieee80211_time_after(b,a)
#define ieee80211_time_after_eq(a,b)	((long)(a) - (long)(b) >= 0)
#define ieee80211_time_before_eq(a,b)	ieee80211_time_after_eq(b,a)

struct mbuf *_ieee80211_getmgtframe(uint8_t **frm, int headroom, int pktlen);
extern struct mbuf *(*ieee80211_getmgtframe) (uint8_t ** frm, int headroom, int pktlen);

/* tx path usage */
#define	M_ENCAP		M_PROTO1		/* 802.11 encap done */
#ifdef __WISE__
#define M_AMPDU_1	M_PROTO2		/* single frame AMPDU */
#endif
#define	M_EAPOL		M_PROTO3		/* PAE/EAPOL frame */
#define	M_PWR_SAV	M_PROTO4		/* bypass PS handling */
#define	M_MORE_DATA	M_PROTO5		/* more data frames to follow */
#define	M_FF		M_PROTO6		/* fast frame / A-MSDU */
#define	M_TXCB		M_PROTO7		/* do tx complete callback */
#define	M_AMPDU_MPDU	M_PROTO8		/* ok for A-MPDU aggregation */

#if 0 /* defined in mbuf.h */
#define	M_FRAG		M_PROTO9		/* frame fragmentation */
#define	M_FIRSTFRAG	M_PROTO10		/* first frame fragment */
#define	M_LASTFRAG	M_PROTO11		/* last frame fragment */
#endif

#ifdef CONFIG_ACK_ENABLED_MULTI_TID
#define	M_SW_TWEAK	M_PROTO9		/* Dummy Frames for A-MPDU aggregation */
#endif

#ifdef __WISE__
#define	M_80211_TX \
	(M_ENCAP|M_EAPOL|M_PWR_SAV|M_MORE_DATA|M_FF|M_TXCB| \
	 M_AMPDU_MPDU|M_FRAG|M_FIRSTFRAG|M_LASTFRAG|M_AMPDU_1)
#else
	(M_ENCAP|M_EAPOL|M_PWR_SAV|M_MORE_DATA|M_FF|M_TXCB| \
	 M_AMPDU_MPDU|M_FRAG|M_FIRSTFRAG|M_LASTFRAG)
#endif

/* rx path usage */
#define	M_AMPDU		M_PROTO1		/* A-MPDU subframe */
#define	M_WEP		M_PROTO2		/* WEP done by hardware */
#define	M_80211_RX	(M_AMPDU|M_WEP|M_AMPDU_MPDU)

#define	IEEE80211_MBUF_TX_FLAG_BITS \
	M_FLAG_BITS \
	"\15M_ENCAP\17M_EAPOL\20M_PWR_SAV\21M_MORE_DATA\22M_FF\23M_TXCB" \
	"\24M_AMPDU_MPDU\25M_FRAG\26M_FIRSTFRAG\27M_LASTFRAG"

#define	IEEE80211_MBUF_RX_FLAG_BITS \
	M_FLAG_BITS \
	"\15M_AMPDU\16M_WEP\24M_AMPDU_MPDU"

/*
 * Store WME access control bits in the vlan tag.
 * This is safe since it's done after the packet is classified
 * (where we use any previous tag) and because it's passed
 * directly in to the driver and there's no chance someone
 * else will clobber them on us.
 */
#define	M_WME_SETAC(m, ac) \
	((m)->m_pkthdr.ether_vtag = (ac))
#define	M_WME_GETAC(m)	((m)->m_pkthdr.ether_vtag)

/*
 * Mbufs on the power save queue are tagged with an age and
 * timed out.  We reuse the hardware checksum field in the
 * mbuf packet header to store this data.
 */
#define	M_AGE_SET(m,v)		(m->m_pkthdr.csum_data = v)
#define	M_AGE_GET(m)		(m->m_pkthdr.csum_data)
#define	M_AGE_SUB(m,adj)	(m->m_pkthdr.csum_data -= adj)

/*
 * Store the sequence number.
 */
#define	M_SEQNO_SET(m, seqno) \
	((m)->m_pkthdr.tso_segsz = (seqno))
#define	M_SEQNO_GET(m)	((m)->m_pkthdr.tso_segsz)

#define	MTAG_ABI_NET80211	1132948340	/* net80211 ABI */

struct ieee80211_cb {
	void	(*func)(struct ieee80211_node *, void *, int status);
	void	*arg;
};
#define	NET80211_TAG_CALLBACK		0 /* xmit complete callback */
/*
 * Message formats for messages from the net80211 layer to user
 * applications via the routing socket.  These messages are appended
 * to an if_announcemsghdr structure.
 */
struct ieee80211_join_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_leave_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_replay_event {
	uint8_t		iev_src[6];	/* src MAC */
	uint8_t		iev_dst[6];	/* dst MAC */
	uint8_t		iev_cipher;	/* cipher type */
	uint8_t		iev_keyix;	/* key id/index */
	uint64_t	iev_keyrsc;	/* RSC from key */
	uint64_t	iev_rsc;	/* RSC from frame */
};

struct ieee80211_michael_event {
	uint8_t		iev_src[6];	/* src MAC */
	uint8_t		iev_dst[6];	/* dst MAC */
	uint8_t		iev_cipher;	/* cipher type */
	uint8_t		iev_keyix;	/* key id/index */
};

struct ieee80211_wds_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_csa_event {
	uint32_t	iev_flags;	/* channel flags */
	uint16_t	iev_freq;	/* setting in Mhz */
	uint8_t		iev_ieee;	/* IEEE channel number */
	uint8_t		iev_mode;	/* CSA mode */
	uint8_t		iev_count;	/* CSA count */
};

struct ieee80211_cac_event {
	uint32_t	iev_flags;	/* channel flags */
	uint16_t	iev_freq;	/* setting in Mhz */
	uint8_t		iev_ieee;	/* IEEE channel number */
	/* XXX timestamp? */
	uint8_t		iev_type;	/* IEEE80211_NOTIFY_CAC_* */
};

struct ieee80211_radar_event {
	uint32_t	iev_flags;	/* channel flags */
	uint16_t	iev_freq;	/* setting in Mhz */
	uint8_t		iev_ieee;	/* IEEE channel number */
	/* XXX timestamp? */
};

struct ieee80211_auth_event {
	uint8_t		iev_addr[6];
	uint16_t	auth_type;
	uint16_t	auth_transaction;
	uint16_t	status_code;
	uint16_t	auth_data_len;
	uint8_t		auth_data[0];
};

struct ieee80211_assoc_reject_event {
	uint8_t		bssid[6];
	uint16_t	status_code;
	uint16_t	assoc_resp_data_len; /* optional */
	uint8_t		assoc_resp_data[0];
};

struct ieee80211_deauth_event {
	uint8_t		iev_addr[6];
};

struct ieee80211_country_event {
	uint8_t		iev_addr[6];
	uint8_t		iev_cc[2];	/* ISO country code */
};

struct ieee80211_radio_event {
	uint8_t		iev_state;	/* 1 on, 0 off */
};

struct ieee80211_action_event {
	uint16_t frame_len;
	uint8_t frame[0];
};

struct ieee80211_scan_entry_event {
	struct ieee80211req_scan_result *sr;
	void (* free_entry)(void *);
};

struct ieee80211_unprot_disassoc_event {
	uint8_t		is_disassoc;
	uint8_t		iev_src[6];	/* src MAC */
	uint8_t		iev_dst[6];	/* dst MAC */
	uint16_t	reason_code;
};

#define	RTM_IEEE80211_ASSOC	100	/* station associate (bss mode) */
#define	RTM_IEEE80211_REASSOC	101	/* station re-associate (bss mode) */
#define	RTM_IEEE80211_DISASSOC	102	/* station disassociate (bss mode) */
#define	RTM_IEEE80211_JOIN	103	/* station join (ap mode) */
#define	RTM_IEEE80211_LEAVE	104	/* station leave (ap mode) */
#define	RTM_IEEE80211_SCAN	105	/* scan complete, results available */
#define	RTM_IEEE80211_REPLAY	106	/* sequence counter replay detected */
#define	RTM_IEEE80211_MICHAEL	107	/* Michael MIC failure detected */
#define	RTM_IEEE80211_REJOIN	108	/* station re-associate (ap mode) */
#define	RTM_IEEE80211_WDS	109	/* WDS discovery (ap mode) */
#define	RTM_IEEE80211_CSA	110	/* Channel Switch Announcement event */
#define	RTM_IEEE80211_RADAR	111	/* radar event */
#define	RTM_IEEE80211_CAC	112	/* Channel Availability Check event */
#define	RTM_IEEE80211_DEAUTH	113	/* station deauthenticate */
#define	RTM_IEEE80211_AUTH	114	/* station authenticate (ap mode) */
#define	RTM_IEEE80211_COUNTRY	115	/* discovered country code (sta mode) */
#define	RTM_IEEE80211_RADIO	116	/* RF kill switch state change */
#define RTM_IEEE80211_ACTION	117	/* Recv action frames event */
#define RTM_IEEE80211_ASSOC_REJECT	118	/* Recv action frames event */
#define RTM_IEEE80211_SCAN_ENTRY	119	/* Recv action frames event */
#define RTM_IEEE80211_UNPROT_DISASSOC	120	/* Recv Unprotected Disassociation/Deauthentication */

#ifdef __WISE__
#define	NET80211_TAG_XMIT_PARAMS	1
/* See below; this is after the bpf_params definition */
#define	NET80211_TAG_RECV_PARAMS	2
#define	NET80211_TAG_TOA_PARAMS		3
#define	NET80211_TAG_DRV_PARAMS		4
#define	NET80211_TAG_AMSDU_PARAMS	5
#endif

int	_ieee80211_add_callback(struct mbuf *m,
		void (*func)(struct ieee80211_node *, void *, int), void *arg);
extern int (*ieee80211_add_callback)(struct mbuf* m,
	void (*func)(struct ieee80211_node*, void*, int), void* arg);

void	_ieee80211_process_callback(struct ieee80211_node *, struct mbuf *, int);
extern void (*ieee80211_process_callback)(struct ieee80211_node * ni, struct mbuf * m, int status);

int 	_ieee80211_add_drv_params(struct mbuf *m, void *params, int len);
extern int (*ieee80211_add_drv_params)(struct mbuf * m, void *params, int len);

int	_ieee80211_get_drv_params(struct mbuf *m, void *params);
extern int (*ieee80211_get_drv_params)(struct mbuf * m, void *params);

void *_ieee80211_get_drv_params_ptr(struct mbuf *m);
extern void *(*ieee80211_get_drv_params_ptr)(struct mbuf * m);

void	_get_random_bytes(void *, size_t);
extern void (*get_random_bytes)(void *p, size_t n);

struct ieee80211com;

int _ieee80211_parent_xmitpkt(struct ieee80211com *ic, struct mbuf *m);
extern int (*ieee80211_parent_xmitpkt)(struct ieee80211com * ic, struct mbuf * m);

int _ieee80211_vap_xmitpkt(struct ieee80211vap *vap, struct mbuf *m);
extern int (*ieee80211_vap_xmitpkt)(struct ieee80211vap * vap, struct mbuf * m);

void	_ieee80211_sysctl_attach(struct ieee80211com *);
extern void (*ieee80211_sysctl_attach)(struct ieee80211com * ic);

void	ieee80211_sysctl_detach(struct ieee80211com *);
void	_ieee80211_sysctl_vattach(struct ieee80211vap *);
extern void (*ieee80211_sysctl_vattach)(struct ieee80211vap * vap);

void	_ieee80211_sysctl_vdetach(struct ieee80211vap *);
extern void (*ieee80211_sysctl_vdetach)(struct ieee80211vap * vap);

void
ieee80211_notify_scan_entry(struct ieee80211vap *vap,
                       struct ieee80211req_scan_result *sr, int len, void *f);
void ieee80211_del_rxs(struct mbuf *m);

#ifdef __WISE__
#define	atomic_set_32(p, v)			atomic_set_int((volatile int *)p, v)
#define	atomic_subtract_32(p, v)	atomic_subtract_int((volatile int *)p, v)
#define	atomic_fetchadd_32(p, v)	atomic_fetchadd_int((volatile int *)p, v)
#define	atomic_load_32(p)			(*(volatile uint32_t *)(p))

#define MPASS(ex)
#else
// It originally defined in freebsd/sys/sys/lock.h
/*
 * Helpful macros for quickly coming up with assertions with informative
 * panic messages.
 */
#define MPASS(ex)		MPASS4(ex, #ex, __FILE__, __LINE__)
#define MPASS2(ex, what)	MPASS4(ex, what, __FILE__, __LINE__)
#define MPASS3(ex, file, line)	MPASS4(ex, #ex, file, line)
#define MPASS4(ex, what, file, line)					\
	KASSERT((ex), ("Assertion %s failed at %s:%d", what, file, line))
#endif

int	_ieee80211_com_vincref(struct ieee80211vap *);
extern int (*ieee80211_com_vincref)(struct ieee80211vap * vap);

void	_ieee80211_com_vdecref(struct ieee80211vap *);
extern void (*ieee80211_com_vdecref)(struct ieee80211vap * vap);

void	ieee80211_com_vdetach(struct ieee80211vap *);

void	_ieee80211_load_module(const char *);
extern void (*ieee80211_load_module)(const char *modname);

/*
 * A "policy module" is an adjunct module to net80211 that provides
 * functionality that typically includes policy decisions.  This
 * modularity enables extensibility and vendor-supplied functionality.
 */
#ifndef __fully_updated__
#define	_IEEE80211_POLICY_MODULE(policy, name, version)			\
typedef void (*policy##_setup)(int);					\
SET_DECLARE(policy##_set, policy##_setup);
#else
#define	_IEEE80211_POLICY_MODULE(policy, name, version, load, unload) \
	static void ieee80211_##policy##_##name##_load() { load; } \
	static void ieee80211_##policy##_##name##_unload() { unload; } \
	SYSINIT(ieee80211_##policy##_##name, SI_SUB_DRIVERS, SI_ORDER_ANY, \
		ieee80211_##policy##_##name##_load, NULL); \
	SYSUNINIT(ieee80211_##policy##_##name, SI_SUB_DRIVERS, SI_ORDER_ANY, \
		ieee80211_##policy##_##name##_unload, NULL)
#endif

/*
 * Authenticator modules handle 802.1x/WPA authentication.
 */
#ifndef __fully_updated__
#define	IEEE80211_AUTH_MODULE(name, version)				\
	_IEEE80211_POLICY_MODULE(auth, name, version)

#define	IEEE80211_AUTH_ALG(name, alg, v)				\
static void								\
name##_modevent(int type)						\
{									\
	if (type == MOD_LOAD)						\
		ieee80211_authenticator_register(alg, &v);		\
	else								\
		ieee80211_authenticator_unregister(alg);		\
}									\
TEXT_SET(auth_set, name##_modevent)
#else
#define	IEEE80211_AUTH_MODULE(name, version)
#define	IEEE80211_AUTH_ALG(name, alg, v) \
	_IEEE80211_POLICY_MODULE(auth, alg, v, \
		ieee80211_authenticator_register(alg, &v), \
		ieee80211_authenticator_unregister(alg))
#endif

/*
 * Crypto modules implement cipher support.
 */

#ifndef __fully_updated__
#define IEEE80211_CRYPTO_REASSIGN(name) \
    { \
        name.ic_attach = name##_attach; \
        name.ic_detach = name##_detach; \
        name.ic_setkey = name##_setkey; \
        name.ic_setiv = name##_setiv; \
        name.ic_encap = name##_encap; \
        name.ic_decap = name##_decap; \
        name.ic_enmic = name##_enmic; \
        name.ic_demic = name##_demic; \
    }

#define IEEE80211_CRYPTO_MODULE(name, version) \
	void \
	ieee80211_crypto_##name##_load() { \
		IEEE80211_CRYPTO_REASSIGN(name); \
		ieee80211_crypto_register(&name); \
	} \
\
\
	void \
	ieee80211_crypto_##name##_unload() \
	{ \
		ieee80211_crypto_unregister(&name); \
	}
#else
#define IEEE80211_CRYPTO_MODULE(name, version) \
	_IEEE80211_POLICY_MODULE(crypto, name, version, \
		ieee80211_crypto_register(&name), \
		ieee80211_crypto_unregister(&name))
#endif

/*
 * Scanner modules provide scanning policy.
 */
#ifndef __fully_updated__
#define	IEEE80211_SCANNER_MODULE(name, version)				\
	_IEEE80211_POLICY_MODULE(scanner, name, version);

#define	IEEE80211_SCANNER_ALG(name, alg, v)					\
	void ieee80211_scanner_##name##_load(void) {			\
		ieee80211_scanner_register(alg, &v);				\
	}														\
															\
	void ieee80211_scanner_##name##_unload(void) {			\
		ieee80211_scanner_unregister(alg, &v);				\
	}

extern void	(*ieee80211_scan_sta_init)(void);
void	ieee80211_scan_sta_uninit(void);
#else
#define	IEEE80211_SCANNER_MODULE(name, version)
#define	IEEE80211_SCANNER_ALG(name, alg, v) \
	_IEEE80211_POLICY_MODULE(scan, alg, v, \
		ieee80211_scanner_register(alg, &v), \
		ieee80211_scanner_unregister(alg, &v))
#endif

/*
 * Rate control modules provide tx rate control support.
 */
#ifndef __fully_updated__
#define IEEE80211_RATECTL_REASSIGN(name, ratectl) \
        { \
            ratectl.ir_init = name##_init; \
            ratectl.ir_deinit = name##_deinit; \
            ratectl.ir_node_init = name##_node_init; \
            ratectl.ir_node_deinit = name##_node_deinit; \
            ratectl.ir_rate = name##_rate; \
            ratectl.ir_tx_complete = name##_tx_complete; \
            ratectl.ir_tx_update = name##_tx_update; \
            ratectl.ir_setinterval = name##_setinterval; \
        }

#define	IEEE80211_RATECTL_MODULE(alg, version)				\
	_IEEE80211_POLICY_MODULE(ratectl, alg, version);		\

#define IEEE80211_RATECTL_ALG(name, alg, v) \
	void \
	ieee80211_ratectl_##name##_load() { \
        IEEE80211_RATECTL_REASSIGN(name, v); \
		ieee80211_ratectl_register(alg, &v); \
	} \
\
\
	void \
	ieee80211_ratectl_##name##_unload() \
	{ \
		ieee80211_ratectl_unregister(alg); \
	}
#else
#define	IEEE80211_RATECTL_MODULE(alg, version)
#define IEEE80211_RATECTL_ALG(name, alg, v) \
	_IEEE80211_POLICY_MODULE(ratectl, alg, v, \
		ieee80211_ratectl_register(alg, &v), \
		ieee80211_ratectl_unregister(alg))
#endif


struct ieee80211req;
typedef int ieee80211_ioctl_getfunc(struct ieee80211vap *,
    struct ieee80211req *);
SET_DECLARE(ieee80211_ioctl_getset, ieee80211_ioctl_getfunc);
#define	IEEE80211_IOCTL_GET(_name, _get) TEXT_SET(ieee80211_ioctl_getset, _get)

typedef int ieee80211_ioctl_setfunc(struct ieee80211vap *,
    struct ieee80211req *);
SET_DECLARE(ieee80211_ioctl_setset, ieee80211_ioctl_setfunc);
#define	IEEE80211_IOCTL_SET(_name, _set) TEXT_SET(ieee80211_ioctl_setset, _set)

/*
 * Structure prepended to raw packets sent through the bpf
 * interface when set to DLT_IEEE802_11_RADIO.  This allows
 * user applications to specify pretty much everything in
 * an Atheros tx descriptor.  XXX need to generalize.
 *
 * XXX cannot be more than 14 bytes as it is copied to a sockaddr's
 * XXX sa_data area.
 */
struct ieee80211_bpf_params {
	uint8_t		ibp_vers;	/* version */
#define	IEEE80211_BPF_VERSION	0
	uint8_t		ibp_len;	/* header length in bytes */
	uint8_t		ibp_flags;
#define	IEEE80211_BPF_SHORTPRE	0x01	/* tx with short preamble */
#define	IEEE80211_BPF_NOACK	0x02	/* tx with no ack */
#define	IEEE80211_BPF_CRYPTO	0x04	/* tx with h/w encryption */
#define	IEEE80211_BPF_FCS	0x10	/* frame includes FCS */
#define	IEEE80211_BPF_DATAPAD	0x20	/* frame includes data padding */
#define	IEEE80211_BPF_RTS	0x40	/* tx with RTS/CTS */
#define	IEEE80211_BPF_CTS	0x80	/* tx with CTS only */
	uint8_t		ibp_pri;	/* WME/WMM AC+tx antenna */
	uint8_t		ibp_try0;	/* series 1 try count */
	uint8_t		ibp_rate0;	/* series 1 IEEE tx rate */
	uint8_t		ibp_power;	/* tx power (device units) */
	uint8_t		ibp_ctsrate;	/* IEEE tx rate for CTS */
	uint8_t		ibp_try1;	/* series 2 try count */
	uint8_t		ibp_rate1;	/* series 2 IEEE tx rate */
	uint8_t		ibp_try2;	/* series 3 try count */
	uint8_t		ibp_rate2;	/* series 3 IEEE tx rate */
	uint8_t		ibp_try3;	/* series 4 try count */
	uint8_t		ibp_rate3;	/* series 4 IEEE tx rate */
#ifdef __WISE__
	uint8_t		ibp_chw;	/* channel bandwidth */
	uint8_t		ibp_fec_coding;	/* fec_coding_type */
	uint8_t		ibp_dcm;	/* DCM */
#endif
};

#ifdef CONFIG_ACK_ENABLED_MULTI_TID
struct tweak_params {
	uint8_t		tw_flags;	/* tweak mode */
#define TWEAK_QUICK_BA_AGREEMENT			0x01	/* Enable quick-BA session agreements */
#define TWEAK_TEST_QOS_FRAME				0x02	/* Inject test QoS Frame(s) */
#define TWEAK_ENABLE_MULTI_TID				0x04	/* Use different tid for test QoS Frame(s) */
#define TWEAK_TEST_MGMT_FRAME				0x08	/* Inject test Mgmt Frame */
#define TWEAK_TEST_WAIT_FRAME				0x10	/* wait n Frame in tx queue */
#define TWEAK_TEST_WAIT_MULTI_TID			0x20	/* wait n Frame in tx queue ans tweak TID */

#define TWEAK_TID_FOR_MULTI_TID				3
#define TWEAK_TEST_FRAME_IS_LAST			/* Put test frames after ping packet */

	/* Disable tweak */
#define TWEAK_DISABLED					0x00
	/* Enable quick-BA session agreements only */
#define TWEAK_QUICK_BA_AGREEMENT_ONLY			1
	/* Enable single-tid ampdu only */
#define TWEAK_SINGLE_TID_AMPDU_WAIT_MODE		2
	/* Build single-tid a-mpdu(s) with test qos frame(s) */
#define TWEAK_SINGLE_TID_AMPDU				3
	/* Build multi-tid a-mpdu(s) with test qos frame(s) */
#define TWEAK_MULTI_TID_AMPDU				4
	/* Build ack-enabled multi-tid a-mpdu(s) with test mgmt/qos frame(s) */
#define TWEAK_ACK_ENABLED_MULTI_TID_AMPDU		5
	/* Enable multi-tid ampdu only */
#define TWEAK_MULTI_TID_AMPDU_WAIT_MODE			6

        uint16_t tw_len;        /* Length of test QoS frame(s) to be injected */
        uint8_t tw_num;         /* Number of test QoS frame(s) to be injected */
        uint8_t tw_tx_hold;     /* Hold tx */
        uint8_t tw_count;       /* Tweak tx count */
        uint8_t tw_ack_enabled; /* ack-enabled frame is included */
        uint8_t tw_wait_count;  /* Tweak wait mpdu count */
#define TWEAK_SINGLE_TID_AMPDU_WAIT_COUNT_FOR_SMOOTH_ASSOCIATION	20
        uint8_t tw_wait_enable_count;   /* Tweak wait enable mpdu count for smooth assoc */
        uint8_t tw_wait_tid;    /* Tweak wait enable tid (multi-tid case) */
        uint8_t tw_wait_cur_tid;        /* Tweak wait enable tid (multi-tid case) */
        uint8_t tw_all_ack_disable;     /* Tweak disable all-ack mac capa */
        uint8_t tw_hetb_only;   /* After assoc, Tweak enable HETB only tx */
};
#endif

#ifdef __WISE__
typedef int status_t;

status_t init_wlan_stack(void);
void uninit_wlan_stack(void);

#ifdef __not_yet__
status_t wlan_control(void*, uint32, void*, size_t);
#endif
status_t wlan_close(void*);
status_t wlan_if_l2com_alloc(void*);
#endif

int _ieee80211_add_xmit_params(struct mbuf *m, const struct ieee80211_bpf_params *params);
extern int (*ieee80211_add_xmit_params)(struct mbuf * m,
	const struct ieee80211_bpf_params * params);

int ieee80211_get_xmit_params(struct mbuf *m, struct ieee80211_bpf_params *params);

struct ieee80211_tx_params {
	struct ieee80211_bpf_params params;
};

struct ieee80211_rx_params;
struct ieee80211_rx_stats;

int _ieee80211_add_rx_params(struct mbuf *m, const struct ieee80211_rx_stats *rxs);
extern int (*ieee80211_add_rx_params)(struct mbuf *m, const struct ieee80211_rx_stats *rxs);

int _ieee80211_get_rx_params(struct mbuf *m, struct ieee80211_rx_stats *rxs);
extern int (*ieee80211_get_rx_params)(struct mbuf * m,
	struct ieee80211_rx_stats * rxs);


const struct ieee80211_rx_stats * _ieee80211_get_rx_params_ptr(struct mbuf *m);
extern const struct ieee80211_rx_stats *
	(*ieee80211_get_rx_params_ptr) (struct mbuf * m);

struct ieee80211_toa_params {
	int request_id;
};
int	ieee80211_add_toa_params(struct mbuf *m, const struct ieee80211_toa_params *p);
int	ieee80211_get_toa_params(struct mbuf *m, struct ieee80211_toa_params *p);

#ifdef __cplusplus
}
#endif

/*
 * Malloc API.  Other BSD operating systems have slightly
 * different malloc/free namings (eg DragonflyBSD.)
 */
#define	IEEE80211_M_NOWAIT	M_NOWAIT
#define	IEEE80211_M_WAITOK	M_WAITOK
#define	IEEE80211_M_ZERO	M_ZERO

#ifdef __WISE__
#define MALLOC_DEFINE(flag, key, desc)
#define MALLOC_DECLARE(type)
#define SYS_MALLOC(flag, size, p)	do {\
	if (flag & IEEE80211_M_ZERO)	    \
		p = calloc(1, size);	    \
	else				    \
		p = malloc(size);	    \
} while(0)
#define	IEEE80211_MALLOC(size, type, flag)	({ void *__p; SYS_MALLOC((flag), size, __p); __p; })
#define	IEEE80211_FREE(ptr, type)		free(ptr)

#define ticks 	((int)osKernelGetTickCount())
#define time_uptime ticks

#define IEEE80211_UNLOCK_ASSERT(ic)

#define mtx_sleep(vap, mtx, pri, wmesg, timo) osDelay(timo)
int _ieee80211_sw_bcn_attach(struct ieee80211vap *vap);
extern int (*ieee80211_sw_bcn_attach)(struct ieee80211vap * vap);

void _ieee80211_sw_bcn_detach(struct ieee80211vap *vap);
extern void (*ieee80211_sw_bcn_detach)(struct ieee80211vap * vap);

#ifdef CONFIG_SUPPORT_VAP_STATS
#define IEEE80211_VAP_STAT(x) x
#else
#define IEEE80211_VAP_STAT(x)
#endif
#endif /* __WISE__ */


#endif /* _FBSD_COMPAT_NET80211_IEEE80211_WISE_H_ */
