/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)route.h	8.4 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _NET_ROUTE_H_
#define _NET_ROUTE_H_

/*
 * Structures for routing messages.
 */
struct rt_msghdr {
	u_short	rtm_msglen;	/* to skip over non-understood messages */
	u_char	rtm_version;	/* future binary compatibility */
	u_char	rtm_type;	/* message type */
	u_short	rtm_index;	/* index for associated ifp */
	int	rtm_flags;	/* flags, incl. kern & message, e.g. DONE */
	int	rtm_addrs;	/* bitmask identifying sockaddrs in msg */
#ifdef __not_yet__
	pid_t	rtm_pid;	/* identify sender */
#endif
	int	rtm_seq;	/* for sender to identify action */
	int	rtm_errno;	/* why failed */
	int	rtm_fmask;	/* bitmask used in RTM_CHANGE message */
	u_long	rtm_inits;	/* which metrics we are initializing */
#ifdef __not_yet__
	struct	rt_metrics rtm_rmx; /* metrics themselves */
#endif
};

#define RTM_VERSION	5	/* Up the ante and ignore older versions */

/*
 * Message types.
 *
 * The format for each message is annotated below using the following
 * identifiers:
 *
 * (1) struct rt_msghdr
 * (2) struct ifa_msghdr
 * (3) struct if_msghdr
 * (4) struct ifma_msghdr
 * (5) struct if_announcemsghdr
 *
 */
#define	RTM_ADD		0x1	/* (1) Add Route */
#define	RTM_DELETE	0x2	/* (1) Delete Route */
#define	RTM_CHANGE	0x3	/* (1) Change Metrics or flags */
#define	RTM_GET		0x4	/* (1) Report Metrics */
#define	RTM_LOSING	0x5	/* (1) Kernel Suspects Partitioning */
#define	RTM_REDIRECT	0x6	/* (1) Told to use different route */
#define	RTM_MISS	0x7	/* (1) Lookup failed on this address */
#define	RTM_LOCK	0x8	/* (1) fix specified metrics */
		    /*	0x9  */
		    /*	0xa  */
#define	RTM_RESOLVE	0xb	/* (1) req to resolve dst to LL addr */
#define	RTM_NEWADDR	0xc	/* (2) address being added to iface */
#define	RTM_DELADDR	0xd	/* (2) address being removed from iface */
#define	RTM_IFINFO	0xe	/* (3) iface going up/down etc. */
#define	RTM_NEWMADDR	0xf	/* (4) mcast group membership being added to if */
#define	RTM_DELMADDR	0x10	/* (4) mcast group membership being deleted */
#define	RTM_IFANNOUNCE	0x11	/* (5) iface arrival/departure */
#define	RTM_IEEE80211	0x12	/* (5) IEEE80211 wireless event */

#ifdef _KERNEL
void _rt_ieee80211msg(struct ifnet *, int, void *, size_t);
extern void (*rt_ieee80211msg)(struct ifnet *ifp, int what, void *data, size_t data_len);

void rt_ifannouncemsg(struct ifnet *, int);
void rt_ifmsg(struct ifnet *);
int route_output(struct ifnet *, struct mbuf *);
#ifdef CONFIG_LWIP
void ieee80211msg_init(void);
#else
#define ieee80211msg_init(...)

#endif /* CONFIG_LWIP */
#endif

#endif
