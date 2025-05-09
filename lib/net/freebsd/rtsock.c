/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1991, 1993
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
 *	@(#)rtsock.c	8.7 (Berkeley) 10/12/95
 * $FreeBSD$
 */

#include <stdio.h>
#include <sys/param.h>
#include <hal/kernel.h>

#include "hal/console.h"
#include "freebsd/compat_if_types.h"
#include "freebsd/sys/sockio.h"
#include "freebsd/kernel.h"
#include "freebsd/taskqueue.h"
#include "freebsd/ethernet.h"
#include "freebsd/compat_if.h"
#include "freebsd/if_arp.h"
#include "freebsd/if_media.h"
#include "freebsd/if_var.h"
#include "freebsd/route.h"

#include "net80211/ieee80211_var.h"

#include "lwip/netifapi.h"
#include "lwip/etharp.h"
#include <sys/socket.h>
#include "netif/ethernet.h"

#include <netinet/in.h>

#include "malloc.h"

struct ieee80211msg_ctx g_ieee80211msg_ctx;

#define MSG_QUEUE_LEN CONFIG_MEMP_NUM_MBUF_CACHE + 64

#define ifq_lock(ifq)           do {IF_LOCK(ifq);} while (0);
#define ifq_unlock(ifq)         do {IF_UNLOCK(ifq);} while (0);
#define ifq_dequeue(ifq)	    ({ struct mbuf *__m; IFQ_DEQUEUE(ifq, __m); __m; })
#define ifq_enqueue(ifq, m)	    ({ int __ret; IFQ_ENQUEUE(ifq, m, __ret); __ret; })

static struct mbuf *rtsock_msg_mbuf(int type, struct ifnet *ifp);
static void rt_dispatch(struct ifnet *, struct mbuf *);

#ifndef _SOCKADDR_UNION_DEFINED
#define	_SOCKADDR_UNION_DEFINED
/*
 * The union of all possible address formats we handle.
 */
union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
#if LWIP_IPV6
	struct sockaddr_in6	sin6;
#endif
};
#endif /* _SOCKADDR_UNION_DEFINED */

/*ARGSUSED*/
int
route_output(struct ifnet *ifp, struct mbuf *m)
{
#ifdef __not_yet__
	struct rt_msghdr *rtm = NULL;
	struct rtentry *rt = NULL;
	struct rib_head *rnh;
	struct rt_addrinfo info;
	struct sockaddr_storage ss;
#ifdef INET6
	struct sockaddr_in6 *sin6;
	int i, rti_need_deembed = 0;
#endif
	int alloc_len = 0, len, error = 0, fibnum;
	struct ifnet *ifp = NULL;
	union sockaddr_union saun;
	sa_family_t saf = AF_UNSPEC;
	struct rawcb *rp = NULL;
	struct walkarg w;

	fibnum = so->so_fibnum;

#define senderr(e) { error = e; goto flush;}
	if (m == NULL || ((m->m_len < sizeof(long)) &&
		       (m = m_pullup(m, sizeof(long))) == NULL))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen)
		senderr(EINVAL);

	/*
	 * Most of current messages are in range 200-240 bytes,
	 * minimize possible re-allocation on reply using larger size
	 * buffer aligned on 1k boundaty.
	 */
	alloc_len = roundup2(len, 1024);
	if ((rtm = malloc(alloc_len, M_TEMP, M_NOWAIT)) == NULL)
		senderr(ENOBUFS);

	m_copydata(m, 0, len, (caddr_t)rtm);
	bzero(&info, sizeof(info));
	bzero(&w, sizeof(w));

	if (rtm->rtm_version != RTM_VERSION) {
		/* Do not touch message since format is unknown */
		free(rtm, M_TEMP);
		rtm = NULL;
		senderr(EPROTONOSUPPORT);
	}

	/*
	 * Starting from here, it is possible
	 * to alter original message and insert
	 * caller PID and error value.
	 */

	rtm->rtm_pid = curproc->p_pid;
	info.rti_addrs = rtm->rtm_addrs;

	info.rti_mflags = rtm->rtm_inits;
	info.rti_rmx = &rtm->rtm_rmx;

	/*
	 * rt_xaddrs() performs s6_addr[2] := sin6_scope_id for AF_INET6
	 * link-local address because rtrequest requires addresses with
	 * embedded scope id.
	 */
	if (rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, &info))
		senderr(EINVAL);

	info.rti_flags = rtm->rtm_flags;
	if (info.rti_info[RTAX_DST] == NULL ||
	    info.rti_info[RTAX_DST]->sa_family >= AF_MAX ||
	    (info.rti_info[RTAX_GATEWAY] != NULL &&
	     info.rti_info[RTAX_GATEWAY]->sa_family >= AF_MAX))
		senderr(EINVAL);
	saf = info.rti_info[RTAX_DST]->sa_family;
	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET) {
		error = priv_check(curthread, PRIV_NET_ROUTE);
		if (error)
			senderr(error);
	}

	/*
	 * The given gateway address may be an interface address.
	 * For example, issuing a "route change" command on a route
	 * entry that was created from a tunnel, and the gateway
	 * address given is the local end point. In this case the
	 * RTF_GATEWAY flag must be cleared or the destination will
	 * not be reachable even though there is no error message.
	 */
	if (info.rti_info[RTAX_GATEWAY] != NULL &&
	    info.rti_info[RTAX_GATEWAY]->sa_family != AF_LINK) {
		struct rt_addrinfo ginfo;
		struct sockaddr *gdst;

		bzero(&ginfo, sizeof(ginfo));
		bzero(&ss, sizeof(ss));
		ss.ss_len = sizeof(ss);

		ginfo.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&ss;
		gdst = info.rti_info[RTAX_GATEWAY];

		/*
		 * A host route through the loopback interface is
		 * installed for each interface address. In pre 8.0
		 * releases the interface address of a PPP link type
		 * is not reachable locally. This behavior is fixed as
		 * part of the new L2/L3 redesign and rewrite work. The
		 * signature of this interface address route is the
		 * AF_LINK sa_family type of the rt_gateway, and the
		 * rt_ifp has the IFF_LOOPBACK flag set.
		 */
		if (rib_lookup_info(fibnum, gdst, NHR_REF, 0, &ginfo) == 0) {
			if (ss.ss_family == AF_LINK &&
			    ginfo.rti_ifp->if_flags & IFF_LOOPBACK) {
				info.rti_flags &= ~RTF_GATEWAY;
				info.rti_flags |= RTF_GWFLAG_COMPAT;
			}
			rib_free_info(&ginfo);
		}
	}

	switch (rtm->rtm_type) {
		struct rtentry *saved_nrt;

	case RTM_ADD:
	case RTM_CHANGE:
		if (info.rti_info[RTAX_GATEWAY] == NULL)
			senderr(EINVAL);
		saved_nrt = NULL;

		/* support for new ARP code */
		if (info.rti_info[RTAX_GATEWAY]->sa_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLDATA) != 0) {
			error = lla_rt_output(rtm, &info);
#ifdef INET6
			if (error == 0)
				rti_need_deembed = (V_deembed_scopeid) ? 1 : 0;
#endif
			break;
		}
		error = rtrequest1_fib(rtm->rtm_type, &info, &saved_nrt,
		    fibnum);
		if (error == 0 && saved_nrt != NULL) {
#ifdef INET6
			rti_need_deembed = (V_deembed_scopeid) ? 1 : 0;
#endif
			RT_LOCK(saved_nrt);
			rtm->rtm_index = saved_nrt->rt_ifp->if_index;
			RT_REMREF(saved_nrt);
			RT_UNLOCK(saved_nrt);
		}
		break;

	case RTM_DELETE:
		saved_nrt = NULL;
		/* support for new ARP code */
		if (info.rti_info[RTAX_GATEWAY] &&
		    (info.rti_info[RTAX_GATEWAY]->sa_family == AF_LINK) &&
		    (rtm->rtm_flags & RTF_LLDATA) != 0) {
			error = lla_rt_output(rtm, &info);
#ifdef INET6
			if (error == 0)
				rti_need_deembed = (V_deembed_scopeid) ? 1 : 0;
#endif
			break;
		}
		error = rtrequest1_fib(RTM_DELETE, &info, &saved_nrt, fibnum);
		if (error == 0) {
			RT_LOCK(saved_nrt);
			rt = saved_nrt;
			goto report;
		}
#ifdef INET6
		/* rt_msg2() will not be used when RTM_DELETE fails. */
		rti_need_deembed = (V_deembed_scopeid) ? 1 : 0;
#endif
		break;

	case RTM_GET:
		rnh = rt_tables_get_rnh(fibnum, saf);
		if (rnh == NULL)
			senderr(EAFNOSUPPORT);

		RIB_RLOCK(rnh);

		if (info.rti_info[RTAX_NETMASK] == NULL &&
		    rtm->rtm_type == RTM_GET) {
			/*
			 * Provide longest prefix match for
			 * address lookup (no mask).
			 * 'route -n get addr'
			 */
			rt = (struct rtentry *) rnh->rnh_matchaddr(
			    info.rti_info[RTAX_DST], &rnh->head);
		} else
			rt = (struct rtentry *) rnh->rnh_lookup(
			    info.rti_info[RTAX_DST],
			    info.rti_info[RTAX_NETMASK], &rnh->head);

		if (rt == NULL) {
			RIB_RUNLOCK(rnh);
			senderr(ESRCH);
		}
#ifdef RADIX_MPATH
		/*
		 * for RTM_CHANGE/LOCK, if we got multipath routes,
		 * we require users to specify a matching RTAX_GATEWAY.
		 *
		 * for RTM_GET, gate is optional even with multipath.
		 * if gate == NULL the first match is returned.
		 * (no need to call rt_mpath_matchgate if gate == NULL)
		 */
		if (rt_mpath_capable(rnh) &&
		    (rtm->rtm_type != RTM_GET || info.rti_info[RTAX_GATEWAY])) {
			rt = rt_mpath_matchgate(rt, info.rti_info[RTAX_GATEWAY]);
			if (!rt) {
				RIB_RUNLOCK(rnh);
				senderr(ESRCH);
			}
		}
#endif
		/*
		 * If performing proxied L2 entry insertion, and
		 * the actual PPP host entry is found, perform
		 * another search to retrieve the prefix route of
		 * the local end point of the PPP link.
		 */
		if (rtm->rtm_flags & RTF_ANNOUNCE) {
			struct sockaddr laddr;

			if (rt->rt_ifp != NULL &&
			    rt->rt_ifp->if_type == IFT_PROPVIRTUAL) {
				struct ifaddr *ifa;

				ifa = ifa_ifwithnet(info.rti_info[RTAX_DST], 1,
						RT_ALL_FIBS);
				if (ifa != NULL)
					rt_maskedcopy(ifa->ifa_addr,
						      &laddr,
						      ifa->ifa_netmask);
			} else
				rt_maskedcopy(rt->rt_ifa->ifa_addr,
					      &laddr,
					      rt->rt_ifa->ifa_netmask);
			/*
			 * refactor rt and no lock operation necessary
			 */
			rt = (struct rtentry *)rnh->rnh_matchaddr(&laddr,
			    &rnh->head);
			if (rt == NULL) {
				RIB_RUNLOCK(rnh);
				senderr(ESRCH);
			}
		}
		RT_LOCK(rt);
		RT_ADDREF(rt);
		RIB_RUNLOCK(rnh);

report:
		RT_LOCK_ASSERT(rt);
		if ((rt->rt_flags & RTF_HOST) == 0
		    ? jailed_without_vnet(curthread->td_ucred)
		    : prison_if(curthread->td_ucred,
		    rt_key(rt)) != 0) {
			RT_UNLOCK(rt);
			senderr(ESRCH);
		}
		info.rti_info[RTAX_DST] = rt_key(rt);
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rtsock_fix_netmask(rt_key(rt),
		    rt_mask(rt), &ss);
		info.rti_info[RTAX_GENMASK] = 0;
		if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
			ifp = rt->rt_ifp;
			if (ifp) {
				info.rti_info[RTAX_IFP] =
				    ifp->if_addr->ifa_addr;
				error = rtm_get_jailed(&info, ifp, rt,
				    &saun, curthread->td_ucred);
				if (error != 0) {
					RT_UNLOCK(rt);
					senderr(error);
				}
				if (ifp->if_flags & IFF_POINTOPOINT)
					info.rti_info[RTAX_BRD] =
					    rt->rt_ifa->ifa_dstaddr;
				rtm->rtm_index = ifp->if_index;
			} else {
				info.rti_info[RTAX_IFP] = NULL;
				info.rti_info[RTAX_IFA] = NULL;
			}
		} else if ((ifp = rt->rt_ifp) != NULL) {
			rtm->rtm_index = ifp->if_index;
		}

		/* Check if we need to realloc storage */
		rtsock_msg_buffer(rtm->rtm_type, &info, NULL, &len);
		if (len > alloc_len) {
			struct rt_msghdr *new_rtm;
			new_rtm = malloc(len, M_TEMP, M_NOWAIT);
			if (new_rtm == NULL) {
				RT_UNLOCK(rt);
				senderr(ENOBUFS);
			}
			bcopy(rtm, new_rtm, rtm->rtm_msglen);
			free(rtm, M_TEMP);
			rtm = new_rtm;
			alloc_len = len;
		}

		w.w_tmem = (caddr_t)rtm;
		w.w_tmemsize = alloc_len;
		rtsock_msg_buffer(rtm->rtm_type, &info, &w, &len);

		if (rt->rt_flags & RTF_GWFLAG_COMPAT)
			rtm->rtm_flags = RTF_GATEWAY |
				(rt->rt_flags & ~RTF_GWFLAG_COMPAT);
		else
			rtm->rtm_flags = rt->rt_flags;
		rt_getmetrics(rt, &rtm->rtm_rmx);
		rtm->rtm_addrs = info.rti_addrs;

		RT_UNLOCK(rt);
		break;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rt != NULL)
		RTFREE(rt);
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (V_route_cb.any_count <= 1) {
			if (rtm != NULL)
				free(rtm, M_TEMP);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}

	if (rtm != NULL) {
#ifdef INET6
		if (rti_need_deembed) {
			/* sin6_scope_id is recovered before sending rtm. */
			sin6 = (struct sockaddr_in6 *)&ss;
			for (i = 0; i < RTAX_MAX; i++) {
				if (info.rti_info[i] == NULL)
					continue;
				if (info.rti_info[i]->sa_family != AF_INET6)
					continue;
				bcopy(info.rti_info[i], sin6, sizeof(*sin6));
				if (sa6_recoverscope(sin6) == 0)
					bcopy(sin6, info.rti_info[i],
						    sizeof(*sin6));
			}
		}
#endif
		if (error != 0)
			rtm->rtm_errno = error;
		else
			rtm->rtm_flags |= RTF_DONE;

		m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm, 0);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);

		free(rtm, M_TEMP);
	}
	if (m != NULL) {
		M_SETFIB(m, fibnum);
		m->m_flags |= RTS_FILTER_FIB;
		if (rp) {
			/*
			 * XXX insure we don't get a copy by
			 * invalidating our protocol
			 */
			unsigned short family = rp->rcb_proto.sp_family;
			rp->rcb_proto.sp_family = 0;
			rt_dispatch(m, saf);
			rp->rcb_proto.sp_family = family;
		} else
			rt_dispatch(m, saf);
	}

	return (error);
#else
	return (0);
#endif
}

/*
 * Assumes MCLBYTES is enough to construct any message.
 * Used for OS notifications of various events (if/ifa announces,etc)
 *
 * Returns allocated mbuf or NULL on failure.
 */
static struct mbuf *
rtsock_msg_mbuf(int type, struct ifnet *ifp)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	int len;

	switch (type) {
	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;
	case RTM_IFANNOUNCE:
	case RTM_IEEE80211:
		len = sizeof(struct if_announcemsghdr);
		break;
	default:
		len = sizeof(struct rt_msghdr);
	}

	/* XXXGL: can we use MJUMPAGESIZE cluster here? */
	KASSERT(len <= MCLBYTES, ("%s: message too big", __func__));
#ifndef __WISE__
	if (len > MHLEN)
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_gethdr(M_NOWAIT, MT_DATA);
#else
	/*
	 * MBUF_CACHE will not be used except for
	 * wlan driver as an actual Tx and Rx buffers.
	 */
	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR | M_HEAP);
#endif
	if (m == NULL)
		return (m);

	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = ifp;
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, len);

	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr *ifm;
	struct mbuf *m;

	m = rtsock_msg_mbuf(RTM_IFINFO, ifp);
	if (m == NULL)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
	if_data_copy(ifp, &ifm->ifm_data);
	ifm->ifm_addrs = 0;
	rt_dispatch(ifp, m);
}

static struct mbuf *
rt_makeifannouncemsg(struct ifnet *ifp, int type, int what)
{
	struct if_announcemsghdr *ifan;
	struct mbuf *m;

	m = rtsock_msg_mbuf(type, ifp);
	if (m != NULL) {
		ifan = mtod(m, struct if_announcemsghdr *);
		ifan->ifan_index = ifp->if_index;
		memset(ifan->ifan_name, 0, sizeof(ifan->ifan_name));
		strncpy(ifan->ifan_name, ifp->if_xname,
			sizeof(ifan->ifan_name) - 1);
		ifan->ifan_what = what;
	}
	return m;
}

struct ieee80211msg_ctx {
	struct ifqueue msg_queue;
	osSemaphoreId_t sync;
	osThreadId_t tid;
};

#ifdef CONFIG_LWIP
extern err_t
routeif_input(struct netif *netif, struct mbuf *m);

static void rt_msg_action(struct ifnet *ifp, struct if_announcemsghdr *ifan)
{
	if (ifan->ifan_type == RTM_IEEE80211) {
		switch(ifan->ifan_what) {
			case RTM_IEEE80211_REASSOC:
			case RTM_IEEE80211_ASSOC:
				if_link_state_change(ifp, LINK_STATE_UP);
				break;
			case RTM_IEEE80211_DISASSOC:
				if_link_state_change(ifp, LINK_STATE_DOWN);
				break;
		}
	}
}

static void ieee80211msg_thread(void *argument)
{
	int ret;
	bool need_errfree;
	struct ieee80211_scan_entry_event ise;
	struct mbuf *m;
	struct ifnet *ifp;
	struct if_announcemsghdr *ifan, _ifan;

	(void)argument;

	while (1) {
		osSemaphoreAcquire(g_ieee80211msg_ctx.sync, osWaitForever);
		while ((m = ifq_dequeue(&g_ieee80211msg_ctx.msg_queue))) {
			ifp = mget_ifp(m);
			assert(ifp != NULL);
			ifan = mtod(m, struct if_announcemsghdr *);
			memcpy(&_ifan, ifan, sizeof(_ifan));
			need_errfree = (ifan->ifan_type == RTM_IEEE80211) && (ifan->ifan_what == RTM_IEEE80211_SCAN_ENTRY);
			if (need_errfree) {
				memcpy(&ise, &ifan[1], sizeof(ise));
			}
			ret = routeif_input(&ifp->routeif, m); /* mbuf is free'd on failure. */
			if (ret && need_errfree) {
				ise.free_entry(ise.sr);
			}
			rt_msg_action(ifp, &_ifan);
		}
	}
}

void ieee80211msg_init(void)
{
	osThreadAttr_t attr = {
	.name = "rt_msg",
	.stack_size = (CONFIG_DEFAULT_STACK_SIZE/4) + 512,
	.priority = osPriorityNormal,
	};

	ifq_init(&g_ieee80211msg_ctx.msg_queue, NULL);
	IFQ_SET_MAX_LEN(&g_ieee80211msg_ctx.msg_queue, MSG_QUEUE_LEN);

	g_ieee80211msg_ctx.tid = osThreadNew(ieee80211msg_thread, &g_ieee80211msg_ctx.msg_queue, &attr);
	assert(g_ieee80211msg_ctx.tid);
	g_ieee80211msg_ctx.sync = osSemaphoreNew(1, 0, NULL);
}
#endif

/*
 * This is called to generate routing socket messages indicating
 * IEEE80211 wireless events.
 * XXX we piggyback on the RTM_IFANNOUNCE msg format in a clumsy way.
 */

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(rt_ieee80211msg, &rt_ieee80211msg, &_rt_ieee80211msg);
#else
__func_tab__ void (*rt_ieee80211msg)(struct ifnet *ifp, int what, void *data, size_t data_len) = _rt_ieee80211msg;
#endif

void
_rt_ieee80211msg(struct ifnet *ifp, int what, void *data, size_t data_len)
{
	struct mbuf *m;

	m = rt_makeifannouncemsg(ifp, RTM_IEEE80211, what);
	if (m != NULL) {
		/*
		 * Append the ieee80211 data.  Try to stick it in the
		 * mbuf containing the ifannounce msg; otherwise allocate
		 * a new mbuf and append.
		 *
		 * NB: we assume m is a single mbuf.
		 */
		if (data_len > M_TRAILINGSPACE(m)) {
			struct mbuf *n = m_get(M_NOWAIT, MT_DATA);
			if (n == NULL) {
				m_freem(m);
				return;
			}
			bcopy(data, mtod(n, void *), data_len);
			n->m_len = data_len;
			m->m_next = n;
		} else if (data_len > 0) {
			bcopy(data, mtod(m, u_int8_t *) + m->m_len, data_len);
			m->m_len += data_len;
		}
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len += data_len;
		mtod(m, struct if_announcemsghdr *)->ifan_msglen += data_len;
		rt_dispatch(ifp, m);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
rt_ifannouncemsg(struct ifnet *ifp, int what)
{
	struct mbuf *m;

	m = rt_makeifannouncemsg(ifp, RTM_IFANNOUNCE, what);
	if (m != NULL)
		rt_dispatch(ifp, m);
}

static void
rt_dispatch(struct ifnet *ifp, struct mbuf *m)
{
#ifdef CONFIG_LWIP
	int ret;
	ret = ifq_enqueue(&g_ieee80211msg_ctx.msg_queue, m);
	assert(ret == 0);
	osSemaphoreRelease(g_ieee80211msg_ctx.sync);
#else
	/* free here before implement proper dispatch */
	m_freem(m);
#endif
}
