/*-
 * Copyright (c) 1982, 1986, 1989, 1993
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	From: @(#)if.h	8.1 (Berkeley) 6/10/93
 * $FreeBSD: src/sys/net/if_var.h,v 1.98.2.6 2006/10/06 20:26:05 andre Exp $
 */

#ifndef	_FBSD_COMPAT_NET_IF_VAR_H_
#define	_FBSD_COMPAT_NET_IF_VAR_H_

#include "lwip/netif.h"

/*
 * Structures defining a network interface, providing a packet
 * transport mechanism (ala level 0 of the PUP protocols).
 *
 * Each interface accepts output datagrams of a specified maximum
 * length, and provides higher level routines with input datagrams
 * received from its medium.
 *
 * Output occurs when the routine if_output is called, with three parameters:
 *	(*ifp->if_output)(ifp, m, dst, rt)
 * Here m is the mbuf chain to be sent and dst is the destination address.
 * The output routine encapsulates the supplied datagram if necessary,
 * and then transmits it on its medium.
 *
 * On input, each interface unwraps the data received by it, and either
 * places it on the input queue of an internetwork datagram routine
 * and posts the associated software interrupt, or passes the datagram to a raw
 * packet input routine.
 *
 * Routines exist for locating interfaces by their addresses
 * or for locating an interface on a certain network, as well as more general
 * routing and gateway routines maintaining information used to locate
 * interfaces.  These routines live in the files if.c and route.c
 */

#ifdef __STDC__
/*
 * Forward structure declarations for function prototypes [sic].
 */
struct	mbuf;
struct	thread;
struct	rtentry;
struct	rt_addrinfo;
struct	socket;
struct	ether_header;
struct	carp_if;
struct	route;
#endif

#include "if_dl.h"
#include "net/if.h"

#include <sys/queue.h>		/* get TAILQ macros */
#include <sys/socket.h>

#ifdef _KERNEL
#include "mbuf.h"
#endif /* _KERNEL */
#include "mutex.h"		/* XXX */
#include "_task.h"
#include "cmsis_os.h"


#define	IF_DUNIT_NONE	-1

#include "if_altq.h"

typedef enum {
	IFCOUNTER_IPACKETS = 0,
	IFCOUNTER_IERRORS,
	IFCOUNTER_OPACKETS,
	IFCOUNTER_OERRORS,
	IFCOUNTER_COLLISIONS,
	IFCOUNTER_IBYTES,
	IFCOUNTER_OBYTES,
	IFCOUNTER_IMCASTS,
	IFCOUNTER_OMCASTS,
	IFCOUNTER_IQDROPS,
	IFCOUNTER_OQDROPS,
	IFCOUNTER_NOPROTO,
	IFCOUNTERS /* Array size. */
} ift_counter;

typedef struct ifnet * if_t;

typedef	void (*if_start_fn_t)(if_t);
typedef	int (*if_ioctl_fn_t)(if_t, u_long, caddr_t);
typedef	void (*if_init_fn_t)(void *);
typedef void (*if_qflush_fn_t)(if_t);
typedef int (*if_transmit_fn_t)(if_t, struct mbuf *);

/*
 * Structure defining a queue for a network interface.
 */
struct	ifqueue {
	struct	mbuf *ifq_head;
	struct	mbuf *ifq_tail;
	int	ifq_len;
	int	ifq_maxlen;
	int	ifq_drops;
	struct	mtx ifq_mtx;
};

/*
 * Structure defining a network interface.
 *
 * (Would like to call this struct ``if'', but C isn't PL/1.)
 */

struct ifnet {
	void	*if_softc;		/* pointer to driver state */
	void	*if_l2com;		/* pointer to protocol bits */
	char	if_xname[IFNAMSIZ+4];	/* external name (name + unit) */
	const char *if_dname;		/* driver name */
	int	if_dunit;		/* unit or IF_DUNIT_NONE */
	u_short	if_index;		/* numeric abbreviation for this if  */
	int	if_flags;		/* up/down, broadcast, etc. */
	struct	if_data if_data;
	struct	ifaddr	*if_addr;	/* pointer to link-level address */
/* procedure handles */
	int	(*if_output)		/* output routine (enqueue) */
		(struct ifnet *, struct mbuf *, const struct sockaddr *,
		     struct route *);
	void	(*if_input)		/* input routine (from h/w driver) */
		(struct ifnet *, struct mbuf *);
	void	(*if_etherinput)	/* input routine (from h/w driver) */
		(struct ifnet *, struct mbuf *);
	void	(*if_start)		/* initiate output routine */
		(struct ifnet *);
	int	(*if_ioctl)		/* ioctl routine */
		(struct ifnet *, u_long, const caddr_t);
	void	(*if_init)		/* Init routine */
		(void *);
	int	(*if_transmit)		/* initiate output routine */
		(struct ifnet *, struct mbuf *);
	int	if_drv_flags;		/* driver-managed status flags */
	struct  ifaltq if_snd;		/* output queue (includes altq) */
	const u_int8_t *if_broadcastaddr; /* linklevel broadcast bytestring */

#ifdef __not_yet__
	/* these are only used by IPv6 */
	void	*if_afdata[AF_MAX];
	int	if_afdata_initialized;
	struct	mtx if_afdata_mtx;
#endif
	struct	task if_linktask;	/* task for link change events */
	struct	mtx if_addr_mtx;	/* mutex to protect address lists */
	if_qflush_fn_t	if_qflush;	/* flush any queue */
	/* Haiku additions */
	struct sockaddr_dl	if_lladdr;
	/* For romization etherif and routeif are better put in the end */
	struct 	netif etherif;
	struct 	netif routeif;
#if CONFIG_SUPPORT_IF_STATS && !CONFIG_STATS_IN_IF_DATA
	/* volatile statistics */
	uint64_t	ifi_ipackets;		/* packets received on interface */
	uint64_t	ifi_ierrors;		/* input errors on interface */
	uint64_t	ifi_opackets;		/* packets sent on interface */
	uint64_t	ifi_oerrors;		/* output errors on interface */
	uint64_t	ifi_ibytes;		/* total number of octets received */
	uint64_t	ifi_obytes;		/* total number of octets sent */
	uint64_t	ifi_omcasts;		/* packets sent via multicast */
	uint64_t	ifi_iqdrops;		/* dropped on input, this interface */
	uint64_t	ifi_oqdrops;		/* dropped on output, this interface */
#endif
};

#ifdef CONFIG_FREEBSD_IF_PRINTF
#define if_printf(ifp, format, ...)	printk("[%s] "format, ifp->if_xname, ##__VA_ARGS__)
#else
#define if_printf(ifp, format, ...)
#endif

#ifdef CONFIG_FREEBSD_IC_PRINTF
#define ic_printf(ic, format, ...)	printk("[%s] "format, ic->ic_name, ##__VA_ARGS__)
#else
#define ic_printf(ic, format, ...)
#endif

#if defined(__WISE__) && defined(CONFIG_FREEBSD_VAP_PRINTF)
#define vap_printf(vap, format, ...)	printk("[%s] "format, vap->iv_ic->ic_name, ##__VA_ARGS__)
#else
#define vap_printf(vap, format, ...)
#endif

/*
 * XXX These aliases are terribly dangerous because they could apply
 * to anything.
 */
#define	if_mtu		if_data.ifi_mtu
#define	if_type		if_data.ifi_type
#define	if_addrlen	if_data.ifi_addrlen
#define	if_hdrlen	if_data.ifi_hdrlen
#define	if_metric	if_data.ifi_metric
#define	if_link_state	if_data.ifi_link_state
#ifdef CONFIG_SUPPORT_IF_STATS
#if CONFIG_STATS_IN_IF_DATA
#define	if_ipackets	if_data.ifi_ipackets
#define	if_ierrors	if_data.ifi_ierrors
#define	if_opackets	if_data.ifi_opackets
#define	if_oerrors	if_data.ifi_oerrors
#define	if_ibytes	if_data.ifi_ibytes
#define	if_obytes	if_data.ifi_obytes
#define	if_iqdrops	if_data.ifi_iqdrops
#define	if_oqdrops	if_data.ifi_oqdrops
#define	if_omcasts	if_data.ifi_omcasts
#else
#define	if_ipackets	ifi_ipackets
#define	if_ierrors	ifi_ierrors
#define	if_opackets	ifi_opackets
#define	if_oerrors	ifi_oerrors
#define	if_ibytes	ifi_ibytes
#define	if_obytes	ifi_obytes
#define	if_iqdrops	ifi_iqdrops
#define	if_oqdrops	ifi_oqdrops
#define	if_omcasts	ifi_omcasts
#endif
#else	/* CONFIG_SUPPORT_IF_STATS */
#define	if_ipackets	if_data.dummy
#define	if_ierrors	if_data.dummy
#define	if_opackets	if_data.dummy
#define	if_oerrors	if_data.dummy
#define	if_ibytes	if_data.dummy
#define	if_obytes	if_data.dummy
#define	if_iqdrops	if_data.dummy
#define	if_oqdrops	if_data.dummy
#define	if_omcasts	if_data.dummy
#endif
#define if_rawoutput(if, m, sa) if_output(if, m, sa, (struct route *)NULL)

/*
 * Locks for address lists on the network interface.
 */

#define	IF_ADDR_LOCK_INIT(if)		mtx_init(&(if)->if_addr_mtx, NULL, NULL, MTX_DEF)
#define	IF_ADDR_LOCK_DESTROY(if)	mtx_destroy(&(if)->if_addr_mtx)


#define	IF_ADDR_LOCK(if)			mtx_lock(&(if)->if_addr_mtx)
#define	IF_ADDR_UNLOCK(if)			mtx_unlock(&(if)->if_addr_mtx)
#define	IF_ADDR_LOCK_ASSERT(if)		mtx_assert(&(if)->if_addr_mtx, MA_OWNED)

/*
 * Output queues (ifp->if_snd) and slow device input queues (*ifp->if_slowq)
 * are queues of messages stored on ifqueue structures
 * (defined above).  Entries are added to and deleted from these structures
 * by these macros, which should be called with ipl raised to splimp().
 */
#define IF_LOCK(ifq)		mtx_lock(&(ifq)->ifq_mtx)
#define IF_UNLOCK(ifq)		mtx_unlock(&(ifq)->ifq_mtx)
#define	IF_LOCK_ASSERT(ifq)	mtx_assert(&(ifq)->ifq_mtx, MA_OWNED)
#define	_IF_QFULL(ifq)		((ifq)->ifq_len >= (ifq)->ifq_maxlen)
#define	_IF_DROP(ifq)		((ifq)->ifq_drops++)
#define	_IF_QLEN(ifq)		((ifq)->ifq_len)

#define	_IF_ENQUEUE(ifq, m) do { 				\
	(m)->m_nextpkt = NULL;					\
	if ((ifq)->ifq_tail == NULL) 				\
		(ifq)->ifq_head = m; 				\
	else 							\
		(ifq)->ifq_tail->m_nextpkt = m; 		\
	(ifq)->ifq_tail = m; 					\
	(ifq)->ifq_len++; 					\
} while (0)

#define IF_ENQUEUE(ifq, m) do {					\
	IF_LOCK(ifq); 						\
	_IF_ENQUEUE(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_PREPEND(ifq, m) do {				\
	(m)->m_nextpkt = (ifq)->ifq_head; 			\
	if ((ifq)->ifq_tail == NULL) 				\
		(ifq)->ifq_tail = (m); 				\
	(ifq)->ifq_head = (m); 					\
	(ifq)->ifq_len++; 					\
} while (0)

#define IF_PREPEND(ifq, m) do {		 			\
	IF_LOCK(ifq); 						\
	_IF_PREPEND(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_DEQUEUE(ifq, m) do { 				\
	(m) = (ifq)->ifq_head; 					\
	if (m) { 						\
		if (((ifq)->ifq_head = (m)->m_nextpkt) == NULL)	\
			(ifq)->ifq_tail = NULL; 		\
		(m)->m_nextpkt = NULL; 				\
		(ifq)->ifq_len--; 				\
	} 							\
} while (0)

#define IF_DEQUEUE(ifq, m) do { 				\
	IF_LOCK(ifq); 						\
	_IF_DEQUEUE(ifq, m); 					\
	IF_UNLOCK(ifq); 					\
} while (0)

#define	_IF_POLL(ifq, m)	((m) = (ifq)->ifq_head)
#define	IF_POLL(ifq, m)		_IF_POLL(ifq, m)

#define _IF_DRAIN(ifq) do { 					\
	struct mbuf *m; 					\
	for (;;) { 						\
		_IF_DEQUEUE(ifq, m); 				\
		if (m == NULL) 					\
			break; 					\
		m_freem(m); 					\
	} 							\
} while (0)

#define IF_DRAIN(ifq) do {					\
	IF_LOCK(ifq);						\
	_IF_DRAIN(ifq);						\
	IF_UNLOCK(ifq);						\
} while(0)

#define IFQ_SET_MAX_LEN(ifq, max_len) ((ifq)->ifq_maxlen = max_len)

void ifq_init(struct ifqueue *, const char *);
void ifq_uninit(struct ifqueue *);

#ifdef _KERNEL
#ifdef __not_yet__
/* interface address change event */
typedef void (*ifaddr_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifaddr_event, ifaddr_event_handler_t);
/* new interface arrival event */
typedef void (*ifnet_arrival_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_arrival_event, ifnet_arrival_event_handler_t);
/* interface departure event */
typedef void (*ifnet_departure_event_handler_t)(void *, struct ifnet *);
EVENTHANDLER_DECLARE(ifnet_departure_event, ifnet_departure_event_handler_t);
#endif

#define	IF_AFDATA_LOCK_INIT(ifp)	\
    mtx_init(&(ifp)->if_afdata_mtx, NULL, NULL, MTX_DEF)
#define	IF_AFDATA_LOCK(ifp)	mtx_lock(&(ifp)->if_afdata_mtx)
#ifdef __not_yet__
#define	IF_AFDATA_TRYLOCK(ifp)	mtx_trylock(&(ifp)->if_afdata_mtx)
#endif
#define	IF_AFDATA_UNLOCK(ifp)	mtx_unlock(&(ifp)->if_afdata_mtx)
#define	IF_AFDATA_DESTROY(ifp)	mtx_destroy(&(ifp)->if_afdata_mtx)

#ifdef __not_yet__
#define	IFF_LOCKGIANT(ifp) do {						\
	if ((ifp)->if_flags & IFF_NEEDSGIANT)				\
		mtx_lock(&Giant);					\
} while (0)

#define	IFF_UNLOCKGIANT(ifp) do {					\
	if ((ifp)->if_flags & IFF_NEEDSGIANT)				\
		mtx_unlock(&Giant);					\
} while (0)

int	if_handoff(struct ifqueue *ifq, struct mbuf *m, struct ifnet *ifp,
	    int adjust);
#define	IF_HANDOFF(ifq, m, ifp)			\
	if_handoff((struct ifqueue *)ifq, m, ifp, 0)
#define	IF_HANDOFF_ADJ(ifq, m, ifp, adj)	\
	if_handoff((struct ifqueue *)ifq, m, ifp, adj)
#endif

void	if_start(struct ifnet *);
int	if_ioctl(struct ifnet *, const long cmd, const void *argp);

#ifdef __WISE__
#define	IFQ_ENQUEUE_NOLOCK(ifq, m, err)					\
do {									\
	if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_ENQUEUE(ifq, m, NULL, err);			\
	else {								\
		if (_IF_QFULL(ifq)) {					\
			m_freem(m);					\
			(err) = ENOBUFS;				\
		} else {						\
			_IF_ENQUEUE(ifq, m);				\
			(err) = 0;					\
		}							\
	}								\
	if (err)							\
		(ifq)->ifq_drops++;					\
} while (0)

#define	IFQ_ENQUEUE(ifq, m, err)					\
do {									\
	IF_LOCK(ifq);							\
	IFQ_ENQUEUE_NOLOCK(ifq, m, err);				\
	IF_UNLOCK(ifq);							\
} while (0)

#else
#define	IFQ_ENQUEUE(ifq, m, err)					\
do {									\
	IF_LOCK(ifq);							\
	if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_ENQUEUE(ifq, m, NULL, err);			\
	else {								\
		if (_IF_QFULL(ifq)) {					\
			m_freem(m);					\
			(err) = ENOBUFS;				\
		} else {						\
			_IF_ENQUEUE(ifq, m);				\
			(err) = 0;					\
		}							\
	}								\
	if (err)							\
		(ifq)->ifq_drops++;					\
	IF_UNLOCK(ifq);							\
} while (0)
#endif
#define	IFQ_DEQUEUE_NOLOCK(ifq, m)					\
do {									\
	if (TBR_IS_ENABLED(ifq))					\
		(m) = tbr_dequeue_ptr(ifq, ALTDQ_REMOVE);		\
	else if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_DEQUEUE(ifq, m);					\
	else								\
		_IF_DEQUEUE(ifq, m);					\
} while (0)

#define	IFQ_DEQUEUE(ifq, m)						\
do {									\
	IF_LOCK(ifq);							\
	IFQ_DEQUEUE_NOLOCK(ifq, m);					\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_POLL_NOLOCK(ifq, m)						\
do {									\
	if (TBR_IS_ENABLED(ifq))					\
		(m) = tbr_dequeue_ptr(ifq, ALTDQ_POLL);			\
	else if (ALTQ_IS_ENABLED(ifq))					\
		ALTQ_POLL(ifq, m);					\
	else								\
		_IF_POLL(ifq, m);					\
} while (0)

#define	IFQ_POLL(ifq, m)						\
do {									\
	IF_LOCK(ifq);							\
	IFQ_POLL_NOLOCK(ifq, m);					\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_PURGE_NOLOCK(ifq)						\
do {									\
	if (ALTQ_IS_ENABLED(ifq)) {					\
		ALTQ_PURGE(ifq);					\
	} else								\
		_IF_DRAIN(ifq);						\
} while (0)

#define	IFQ_PURGE(ifq)							\
do {									\
	IF_LOCK(ifq);							\
	IFQ_PURGE_NOLOCK(ifq);						\
	IF_UNLOCK(ifq);							\
} while (0)

#define	IFQ_SET_READY(ifq)						\
	do { ((ifq)->altq_flags |= ALTQF_READY); } while (0)

#define	IFQ_LOCK(ifq)			IF_LOCK(ifq)
#define	IFQ_UNLOCK(ifq)			IF_UNLOCK(ifq)
#define	IFQ_LOCK_ASSERT(ifq)		IF_LOCK_ASSERT(ifq)
#define	IFQ_IS_EMPTY(ifq)		((ifq)->ifq_len == 0)
#define	IFQ_INC_LEN(ifq)		((ifq)->ifq_len++)
#define	IFQ_DEC_LEN(ifq)		(--(ifq)->ifq_len)
#define	IFQ_INC_DROPS(ifq)		((ifq)->ifq_drops++)
#define	IFQ_SET_MAXLEN(ifq, len)	((ifq)->ifq_maxlen = (len))

/*
 * The IFF_DRV_OACTIVE test should really occur in the device driver, not in
 * the handoff logic, as that flag is locked by the device driver.
 */

#define IFQ_LEN(ifp, len)						\
do {									\
	len = _IF_QLEN(&(ifp)->if_snd);					\
} while (0)

#ifdef CONFIG_SUPPORT_IF_STATS
#define	IFQ_HANDOFF_ADJ(ifp, m, adj, err)				\
do {									\
	int len;							\
	short mflags;							\
									\
	len = (m)->m_pkthdr.len;					\
	mflags = (m)->m_flags;						\
	IFQ_ENQUEUE(&(ifp)->if_snd, m, err);				\
	if ((err) == 0) {						\
		atomic_add_64((int64_t *)&(ifp)->if_obytes, len + (adj)); \
		if (mflags & M_MCAST)					\
			(ifp)->if_omcasts++;				\
		if (((ifp)->if_drv_flags & IFF_DRV_OACTIVE) == 0)	\
			if_start(ifp);					\
	}								\
} while (0)
#else
#define	IFQ_HANDOFF_ADJ(ifp, m, adj, err)				\
	do {									\
		IFQ_ENQUEUE(&(ifp)->if_snd, m, err);				\
		if ((err) == 0) {						\
			if (((ifp)->if_drv_flags & IFF_DRV_OACTIVE) == 0)	\
				if_start(ifp);					\
		}								\
	} while (0)

#endif
#define	IFQ_HANDOFF(ifp, m, err)					\
	IFQ_HANDOFF_ADJ(ifp, m, 0, err)

#define	IFQ_DRV_DEQUEUE(ifq, m)						\
do {									\
	(m) = (ifq)->ifq_drv_head;					\
	if (m) {							\
		if (((ifq)->ifq_drv_head = (m)->m_nextpkt) == NULL)	\
			(ifq)->ifq_drv_tail = NULL;			\
		(m)->m_nextpkt = NULL;					\
		(ifq)->ifq_drv_len--;					\
	} else {							\
		IFQ_LOCK(ifq);						\
		IFQ_DEQUEUE_NOLOCK(ifq, m);				\
		while ((ifq)->ifq_drv_len < (ifq)->ifq_drv_maxlen) {	\
			struct mbuf *m0;				\
			IFQ_DEQUEUE_NOLOCK(ifq, m0);			\
			if (m0 == NULL)					\
				break;					\
			m0->m_nextpkt = NULL;				\
			if ((ifq)->ifq_drv_tail == NULL)		\
				(ifq)->ifq_drv_head = m0;		\
			else						\
				(ifq)->ifq_drv_tail->m_nextpkt = m0;	\
			(ifq)->ifq_drv_tail = m0;			\
			(ifq)->ifq_drv_len++;				\
		}							\
		IFQ_UNLOCK(ifq);					\
	}								\
} while (0)

#define	IFQ_DRV_PREPEND(ifq, m)						\
do {									\
	(m)->m_nextpkt = (ifq)->ifq_drv_head;				\
	if ((ifq)->ifq_drv_tail == NULL)				\
		(ifq)->ifq_drv_tail = (m);				\
	(ifq)->ifq_drv_head = (m);					\
	(ifq)->ifq_drv_len++;						\
} while (0)

#define	IFQ_DRV_IS_EMPTY(ifq)						\
	(((ifq)->ifq_drv_len == 0) && ((ifq)->ifq_len == 0))

#define	IFQ_DRV_PURGE(ifq)						\
do {									\
	struct mbuf *m, *n = (ifq)->ifq_drv_head;			\
	while((m = n) != NULL) {					\
		n = m->m_nextpkt;					\
		m_freem(m);						\
	}								\
	(ifq)->ifq_drv_head = (ifq)->ifq_drv_tail = NULL;		\
	(ifq)->ifq_drv_len = 0;						\
	IFQ_PURGE(ifq);							\
} while (0)

/*
 * 72 was chosen below because it is the size of a TCP/IP
 * header (40) + the minimum mss (32).
 */
#define	IF_MINMTU	72
#define	IF_MAXMTU	65535

#endif /* _KERNEL */

/*
 * The ifaddr structure contains information about one address
 * of an interface.  They are maintained by the different address families,
 * are allocated and attached when an address is set, and are linked
 * together so all addresses for an interface can be located.
 *
 * NOTE: a 'struct ifaddr' is always at the beginning of a larger
 * chunk of malloc'ed memory, where we store the three addresses
 * (ifa_addr, ifa_dstaddr and ifa_netmask) referenced here.
 */
struct ifaddr {
	struct	sockaddr *ifa_addr;	/* address of interface */
	struct	sockaddr *ifa_dstaddr;	/* other end of p-to-p link */
#define	ifa_broadaddr	ifa_dstaddr	/* broadcast address interface */
	struct	sockaddr *ifa_netmask;	/* used to determine subnet */
	struct	if_data if_data;	/* not all members are meaningful */
	struct	ifnet *ifa_ifp;		/* back-pointer to interface */
	TAILQ_ENTRY(ifaddr) ifa_link;	/* queue macro glue */
#ifdef __not_yet__
	void	(*ifa_rtrequest)	/* check or clean routes (+ or -)'d */
		(int, struct rtentry *, struct rt_addrinfo *);
#endif
	u_short	ifa_flags;		/* mostly rt_flags for cloning */
	u_int	ifa_refcnt;		/* references to this structure */
	int	ifa_metric;		/* cost of going out this interface */
	int (*ifa_claim_addr)		/* check if an addr goes to this if */
		(struct ifaddr *, struct sockaddr *);
	struct mtx ifa_mtx;
};

struct ifaddr *	ifa_alloc(size_t size, int flags);
void	ifa_free(struct ifaddr *ifa);
void	ifa_ref(struct ifaddr *ifa);


#define	IFA_LOCK_INIT(ifa)	\
    mtx_init(&(ifa)->ifa_mtx, NULL, NULL, MTX_DEF)
#define	IFA_LOCK(ifa)		mtx_lock(&(ifa)->ifa_mtx)
#define	IFA_UNLOCK(ifa)		mtx_unlock(&(ifa)->ifa_mtx)
#define	IFA_DESTROY(ifa)	mtx_destroy(&(ifa)->ifa_mtx)


#ifdef _KERNEL
#define	IFA_LOCK(ifa)		mtx_lock(&(ifa)->ifa_mtx)
#define	IFA_UNLOCK(ifa)		mtx_unlock(&(ifa)->ifa_mtx)

#if 0
extern	struct rw_lock ifnet_rwlock;
#define	IFNET_LOCK_INIT()		rw_lock_init(&ifnet_rwlock, "ifnet rwlock")
#define	IFNET_WLOCK()			rw_lock_write_lock(&ifnet_rwlock)
#define	IFNET_WUNLOCK()			rw_lock_write_unlock(&ifnet_rwlock)
#define	IFNET_RLOCK()			rw_lock_read_lock(&ifnet_rwlock)
#define	IFNET_RLOCK_NOSLEEP()	rw_lock_read_lock(&ifnet_rwlock)
#define	IFNET_RUNLOCK()			rw_lock_read_unlock(&ifnet_rwlock)
#define	IFNET_RUNLOCK_NOSLEEP()	rw_lock_read_unlock(&ifnet_rwlock)
#else
#define	IFNET_WLOCK()			osKernelLock()
#define	IFNET_WUNLOCK()			osKernelUnlock()
#define	IFNET_RLOCK()			osKernelLock()
#define	IFNET_RLOCK_NOSLEEP()		osKernelLock()
#define	IFNET_RUNLOCK()			osKernelUnlock()
#define	IFNET_RUNLOCK_NOSLEEP()		osKernelUnlock()
#endif

extern	const int ifqmaxlen;

struct	ifnet* _if_alloc(u_char);
extern struct ifnet *(*if_alloc)(u_char type);

void	if_attach(struct ifnet *);
void	if_detach(struct ifnet *);
void	_if_free(struct ifnet *);
extern  void (*if_free) (struct ifnet *ifp);
void	_if_initname(struct ifnet *, const char *, int);
extern void (*if_initname)(struct ifnet *ifp, const char *name, int unit);
void	_if_link_state_change(struct ifnet *, int);
extern void (*if_link_state_change)(struct ifnet *ifp, int linkState);
#if 0
int	if_printf(struct ifnet *, const char *, ...) __printflike(2, 3);
#endif
void	if_data_copy(struct ifnet *, struct if_data *);
uint64_t if_get_counter_default(struct ifnet *, ift_counter);
void	_if_inc_counter(struct ifnet *, ift_counter, int64_t);
extern void (*if_inc_counter) (struct ifnet *ifp, ift_counter cnt, int64_t inc);

#define IF_LLADDR(ifp)							\
    LLADDR((struct sockaddr_dl *)((ifp)->if_addr->ifa_addr))

const char *if_getdname(if_t ifp);
int if_setdrvflagbits(if_t ifp, int if_setflags, int clear_flags);
int if_getdrvflags(if_t ifp);
int if_setdrvflags(if_t ifp, int flags);
int if_setsoftc(if_t ifp, void *softc);
void *if_getsoftc(if_t ifp);
int if_setflags(if_t ifp, int flags);
int if_setmtu(if_t ifp, int mtu);
int if_getmtu(if_t ifp);
int if_setflagbits(if_t ifp, int set, int clear);
int if_getflags(if_t ifp);
int if_sendq_empty(if_t ifp);
int if_setsendqready(if_t ifp);
int if_setsendqlen(if_t ifp, int tx_desc_count);
int if_input(if_t ifp, struct mbuf* sendmp);
int if_sendq_prepend(if_t ifp, struct mbuf *m);
struct mbuf *if_dequeue(if_t ifp);
int if_setifheaderlen(if_t ifp, int len);
void if_setrcvif(struct mbuf *m, if_t ifp);

void if_setvtag(struct mbuf *m, u_int16_t tag);
u_int16_t if_getvtag(struct mbuf *m);
caddr_t if_getlladdr(if_t ifp);
int if_setlladdr(if_t, const u_char *, int);
void *if_gethandle(u_char);
#ifdef __not_yet__
void if_bpfmtap(if_t ifp, struct mbuf *m);
void if_etherbpfmtap(if_t ifp, struct mbuf *m);
void if_vlancap(if_t ifp);
#endif

/* Functions */
void if_setinitfn(if_t ifp, void (*)(void *));
void if_setioctlfn(if_t ifp, int (*)(if_t, u_long, caddr_t));
void if_setstartfn(if_t ifp, void (*)(if_t));
void if_settransmitfn(if_t ifp, if_transmit_fn_t);
void if_setqflushfn(if_t ifp, if_qflush_fn_t);

#endif /* _KERNEL */

#endif /* _FBSD_COMPAT_NET_IF_VAR_H_ */
