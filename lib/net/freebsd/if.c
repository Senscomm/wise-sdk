/*
 * Copyright 2018-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Modified for WISE
 */
/*
 * Copyright 2009, Colin Günther, coling@gmx.de.
 * Copyright 2007-2009, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Copyright 2004, Marcus Overhagen. All Rights Reserved.
 *
 * Distributed under the terms of the MIT License.
 */

#include <stdio.h>
#include <stdint.h>

#include "hal/kernel.h"
#include "hal/console.h"

#include "freebsd/compat_types.h"
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
#include "lwip/priv/raw_priv.h"
#include "netif/ethernet.h"
#include "malloc.h"

#ifdef __not_yet__
#include <compat/net/bpf.h>
#endif

#include "cmsis_os.h"

/**
 * FIXME: remove this memo later
 *
 * WISE:
 * The struct ifnet will be eventually combined with lwip netif.
 *
 * The get_xxx_by_index functions are mostly used by the upper
 * protocol layers and are not used for network interface drivers.
 *
 * If a new ifnet is and attached, we should call to netif_add() to
 * register this interface.
 *
 * For the time being, most of functions are commented out.
 */

#ifdef CONFIG_IEEE80211_HOSTED
const int ifqmaxlen = CONFIG_MEMP_NUM_MBUF_DYNA_EXT + CONFIG_MEMP_NUM_MBUF_CACHE;
#else
const int ifqmaxlen = IFQ_MAXLEN;
#endif
static void if_input_default(struct ifnet *, struct mbuf *);

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(if_alloc, &if_alloc, &_if_alloc);
#else
__func_tab__ struct ifnet *(*if_alloc)(u_char type) = _if_alloc;
#endif

struct ifnet *_if_alloc(u_char type)
{
	struct ifnet *ifp = malloc(sizeof(struct ifnet));
	if (ifp == NULL)
		return NULL;
	else
		memset(ifp, 0, sizeof(struct ifnet));

	switch (type) {
		case IFT_ETHER:
		{
			ifp->if_l2com = malloc(sizeof(struct arpcom));
			if (ifp->if_l2com == NULL)
				goto err;
			IFP2AC(ifp)->ac_ifp = ifp;
			break;
		}
		/* FIXME: remove IFT_IEEE80211 */
		case IFT_IEEE80211:
		{
			ifp->if_l2com = malloc(sizeof(struct ieee80211com));
			if (ifp->if_l2com == NULL)
				goto err;
			else
				memset(ifp->if_l2com, 0, sizeof(struct ieee80211com));
			break;
		}
		case IFT_LOOP:
		{
			ifp->if_flags = IFF_BROADCAST | IFF_LOOPBACK | IFF_UP;
			ifp->if_mtu = 1024* 2;
			ifp->if_ioctl = ether_ioctl;
		}
	}


	ifp->if_type = type;

	IF_ADDR_LOCK_INIT(ifp);
	return ifp;

err:
	free(ifp);
	return NULL;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(if_free, &if_free, &_if_free);
#else
__func_tab__ void (*if_free) (struct ifnet *ifp) = _if_free;
#endif

void
_if_free(struct ifnet *ifp)
{
#if 0
	/* FIXME: implement */

	// IEEE80211 devices won't be in this list,
	// so don't try to remove them.
	if (ifp->if_type == IFT_ETHER)
		remove_from_device_name_list(ifp);
#endif
	IF_ADDR_LOCK_DESTROY(ifp);
	switch (ifp->if_type) {
		case IFT_ETHER:
		case IFT_IEEE80211:
			free(ifp->if_l2com);
			break;
	}

	free(ifp);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(if_initname, &if_initname, &_if_initname);
#else
__func_tab__ void
(*if_initname)(struct ifnet *ifp, const char *name, int unit) = _if_initname;
#endif

void
_if_initname(struct ifnet *ifp, const char *name, int unit)
{
	if (name == NULL || name[0] == '\0')
		printk("interface goes unnamed");

	ifp->if_dname = name;
	ifp->if_dunit = unit;

	if (unit == -1)
		snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s", name);
	else
		snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d", name, unit);
}

void
ifq_init(struct ifqueue *ifq, const char *name)
{
	ifq->ifq_head = NULL;
	ifq->ifq_tail = NULL;
	ifq->ifq_len = 0;
	ifq->ifq_maxlen = IFQ_MAXLEN;
	ifq->ifq_drops = 0;

	mtx_init(&ifq->ifq_mtx, name, NULL, MTX_DEF);
}


void
ifq_uninit(struct ifqueue *ifq)
{
	mtx_destroy(&ifq->ifq_mtx);
}


__ilm_wlan_tx__ int
if_transmit(struct ifnet *ifp, struct mbuf *m)
{
	int error;

	IFQ_HANDOFF(ifp, m, error);
	return (error);
}


static void
if_input_default(struct ifnet *ifp __unused, struct mbuf *m)
{

	m_freem(m);
}


/*
 * Flush an interface queue.
 */
void
if_qflush(struct ifnet *ifp)
{
	struct mbuf *m, *n;
	struct ifaltq *ifq;

	ifq = &ifp->if_snd;
	IFQ_LOCK(ifq);
#ifdef ALTQ
	if (ALTQ_IS_ENABLED(ifq))
		ALTQ_PURGE(ifq);
#endif
	n = ifq->ifq_head;
	while ((m = n) != NULL) {
		n = m->m_nextpkt;
		m_freem(m);
	}
	ifq->ifq_head = 0;
	ifq->ifq_tail = 0;
	ifq->ifq_len = 0;
	IFQ_UNLOCK(ifq);
}

#ifdef CONFIG_LWIP

extern err_t routeif_init(struct netif *netif);
extern void ethernetif_input(struct netif *netif, struct mbuf *m);
extern err_t ethernetif_ioctl (struct netif *netif, const long cmd, const void *argp);
extern err_t ethernetif_init(struct netif *netif);
extern void ethernetif_inputpbuf(struct netif *netif, struct pbuf *p);
#endif

void
if_attach(struct ifnet *ifp)
{
	unsigned socksize, ifasize;
	int namelen, masklen;
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;
#if LWIP_NETIF_API
	struct netif *rtif = &ifp->routeif;
#endif
	ip4_addr_t addr;
	ip4_addr_t netmask;
	ip4_addr_t gw;

	IF_ADDR_LOCK_INIT(ifp);

	ifp->if_lladdr.sdl_family = AF_LINK;

	ifq_init((struct ifqueue *) &ifp->if_snd, ifp->if_xname);

	if (ifp->if_transmit == NULL) {
		ifp->if_transmit = if_transmit;
		ifp->if_qflush = if_qflush;
	}
	if (ifp->if_input == NULL)
		ifp->if_input = if_input_default;

	/*
	 * Create a Link Level name for this device.
	 */
	namelen = strlen(ifp->if_xname);
	/*
	 * Always save enough space for any possible name so we
	 * can do a rename in place later.
	 */
	masklen = offsetof(struct sockaddr_dl, sdl_data[0]) + IFNAMSIZ;
	socksize = masklen + ifp->if_addrlen;
	if (socksize < sizeof(*sdl))
		socksize = sizeof(*sdl);
	socksize = roundup2(socksize, sizeof(long));
	ifasize = sizeof(*ifa) + 2 * socksize;
	ifa = ifa_alloc(ifasize, M_WAITOK);
	sdl = (struct sockaddr_dl *)(ifa + 1);
	sdl->sdl_len = socksize;
	sdl->sdl_family = AF_LINK;
	bcopy(ifp->if_xname, sdl->sdl_data, namelen);
	sdl->sdl_nlen = namelen;
	sdl->sdl_index = 0;
	sdl->sdl_type = ifp->if_type;
	ifp->if_addr = ifa;
	ifa->ifa_ifp = ifp;
	//ifa->ifa_rtrequest = link_rtrequest;
	ifa->ifa_addr = (struct sockaddr *)sdl;
	sdl = (struct sockaddr_dl *)(socksize + (caddr_t)sdl);
	ifa->ifa_netmask = (struct sockaddr *)sdl;
	sdl->sdl_len = masklen;
	while (namelen != 0)
		sdl->sdl_data[--namelen] = 0xff;
	if (ifp->if_type == IFT_ETHER) {
		IP4_ADDR(&addr, 127, 0, 0, 1);
		IP4_ADDR(&netmask, 255, 255, 255, 255);
		IP4_ADDR(&gw, 127, 0, 0, 1);

#if LWIP_NETIF_API
		netifapi_netif_add(rtif, &addr, &netmask, &gw, ifp,
				routeif_init, raw_input_rt);
#endif

		/* Announce the interface. */
		rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
	}
}


void
if_detach(struct ifnet *ifp)
{
	/* Announce that the interface is gone. */
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);

	IF_ADDR_LOCK_DESTROY(ifp);
	ifq_uninit((struct ifqueue *) &ifp->if_snd);
}


__ilm_wlan_tx__ void
if_start(struct ifnet *ifp)
{
#ifdef IFF_NEEDSGIANT
	if (ifp->if_flags & IFF_NEEDSGIANT)
	panic("freebsd compat.: unsupported giant requirement");
#endif
	ifp->if_start(ifp);
}

/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
static void
if_unroute(struct ifnet *ifp, int flag, int fam)
{
	KASSERT(flag == IFF_UP, ("if_unroute: flag != IFF_UP"));

	ifp->if_flags &= ~flag;
	ifp->if_qflush(ifp);

	rt_ifmsg(ifp);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 */
static void
if_route(struct ifnet *ifp, int flag, int fam)
{
	KASSERT(flag == IFF_UP, ("if_route: flag != IFF_UP"));

	ifp->if_flags |= flag;

	rt_ifmsg(ifp);
#ifdef INET6
	in6_if_up(ifp);
#endif
}


/*
 * Mark an interface down and notify protocols of
 * the transition.
 */
void
if_down(struct ifnet *ifp)
{
#if LWIP_NETIF_API
	struct netif *netif = &ifp->etherif;
#endif

#if LWIP_NETIF_API
	netifapi_netif_set_down(netif);
#endif

#ifdef __not_yet__
	EVENTHANDLER_INVOKE(ifnet_event, ifp, IFNET_EVENT_DOWN);
#endif
	if_unroute(ifp, IFF_UP, AF_UNSPEC);
}

/*
 * Mark an interface up and notify protocols of
 * the transition.
 */
void
if_up(struct ifnet *ifp)
{
#if LWIP_NETIF_API
	struct netif *netif = &ifp->etherif;
#endif

#if LWIP_NETIF_API
	netifapi_netif_set_up(netif);
#endif

	if_route(ifp, IFF_UP, AF_UNSPEC);
#ifdef __not_yet__
	EVENTHANDLER_INVOKE(ifnet_event, ifp, IFNET_EVENT_UP);
#endif
}

#define KB			(1000)
#define MB			(KB * KB)
#define GB 			(MB * KB)

struct {
	uint32_t divisor;
	char *prefix;
} t[] = {
	{ .divisor = GB, .prefix = "GB" },
	{ .divisor = MB, .prefix = "MB" },
	{ .divisor = KB, .prefix = "KB" },
	{ .divisor = 1,  .prefix = "B" },
};

#ifdef CONFIG_SUPPORT_IF_STATS
static uint32_t get_divisor_and_prefix(uint64_t number, char **prefix)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t) - 1; i++)
		if (number >= t[i].divisor)
			break;

	if (prefix)
		*prefix = t[i].prefix;

	return t[i].divisor;

}
#endif

int
if_ioctl(struct ifnet *ifp, const long cmd, const void *argp)
{
	struct ifreq *ifr = (struct ifreq *)argp;
	int new_flags, temp_flags;
	int error = 0, do_ifup = 0;

	switch (cmd) {
	case SIOCGIFFLAGS:
		temp_flags = ifp->if_flags | ifp->if_drv_flags;
		ifr->ifr_flags = temp_flags & 0xffff;
		ifr->ifr_flagshigh = temp_flags >> 16;
		break;
	case SIOCSIFFLAGS:
		/*
		 * Currently, no driver owned flags pass the IFF_CANTCHANGE
		 * check, so we don't need special handling here yet.
		 */
		new_flags = (ifr->ifr_flags & 0xffff) |
		    (ifr->ifr_flagshigh << 16);
		if (ifp->if_flags & IFF_UP &&
		    (new_flags & IFF_UP) == 0) {
			if_down(ifp);
		} else if (new_flags & IFF_UP &&
		    (ifp->if_flags & IFF_UP) == 0) {
			do_ifup = 1;
		}

		/* See if permanently promiscuous mode bit is about to flip */
		if ((ifp->if_flags ^ new_flags) & IFF_PROMISC) {
			if (new_flags & IFF_PROMISC)
				ifp->if_flags |= IFF_PROMISC;
			else
				ifp->if_flags &= ~IFF_PROMISC;
		}

		ifp->if_flags = (ifp->if_flags & IFF_CANTCHANGE) |
			(new_flags &~ IFF_CANTCHANGE);
		if (ifp->if_ioctl) {
			error = (*ifp->if_ioctl)(ifp, cmd, (const caddr_t)argp);
		}
		if (do_ifup)
			if_up(ifp);
		break;
	case SIOCGIFMETRIC:
		ifr->ifr_metric = ifp->if_metric;
		break;
	case SIOCGIFMTU:
		ifr->ifr_mtu = ifp->if_mtu;
		break;
	case SIOCSIFMETRIC:
		ifp->if_metric = ifr->ifr_metric;
		break;
	case SIOCSIFMTU:
	{
		u_long oldmtu = ifp->if_mtu;

		if (ifr->ifr_mtu < IF_MINMTU || ifr->ifr_mtu > IF_MAXMTU)
			return (EINVAL);
		if (ifp->if_ioctl == NULL)
			return (EOPNOTSUPP);
		error = (*ifp->if_ioctl)(ifp, cmd, (const caddr_t)argp);
		if (error == 0) {
			rt_ifmsg(ifp);
		}
		/*
		 * If the link MTU changed, do network layer specific procedure.
		 */
		if (ifp->if_mtu != oldmtu) {
#ifdef __not_yet__
#ifdef INET6
			nd6_setmtu(ifp);
#endif
			rt_updatemtu(ifp);
#endif
		}
		break;
	}
	case SIOCSIFHWADDR:
		error = if_setlladdr(ifp,
				(unsigned char *)ifr->ifr_hwaddr.sa_data,
				ifr->ifr_hwaddr.sa_len);
		ifp->if_addrlen = ifr->ifr_hwaddr.sa_len;
		break;

	case SIOCGIFSTATUS: {
		struct ifstat *ifs = (struct ifstat *) ifr;
		int len = sizeof(ifs->ascii);
		char *start = ifs->ascii, *end __maybe_unused = start + len, *prefix __maybe_unused;
		uint32_t divisor __maybe_unused;

		if (ifp->if_flags & IFF_LOOPBACK)
			return ENOTSUP;

#ifdef CONFIG_SUPPORT_IF_STATS
		divisor = get_divisor_and_prefix(ifp->if_ibytes, &prefix);
		start += snprintk(start, end - start,
				  "\tRX packets %"PRIu64" bytes %"PRIu64" (%u.%01u %s)\n"
				  "\tRX errors %"PRIu64" dropped %"PRIu64" overruns %u frame %u\n",
				  ifp->if_ipackets, ifp->if_ibytes,
				  (unsigned) (ifp->if_ibytes / divisor),
				  (unsigned) (ifp->if_ibytes * 10 / divisor) % 10,
				  prefix,
				  ifp->if_ierrors, ifp->if_iqdrops, 0, 0);

		divisor = get_divisor_and_prefix(ifp->if_obytes, &prefix);
		start += snprintk(start, end - start,
				  "\tTX packets %"PRIu64" bytes %"PRIu64" (%u.%01u %s)\n"
				  "\tTX errors %"PRIu64" dropped %"PRIu64" overruns %u carrier %u \n",
				  ifp->if_opackets, ifp->if_obytes,
				  (unsigned) (ifp->if_obytes / divisor),
				  (unsigned) (ifp->if_obytes*10 /divisor) % 10,
				  prefix,
				  ifp->if_oerrors, ifp->if_oqdrops, 0, 0);
#endif
		error = 0;
		break;
	}
#ifdef __WISE__
	case SIOCGLINKSTATE:
		ifr->ifr_linkstate = (short)ifp->if_link_state;
		break;
#endif
	default:
		if (ifp->if_ioctl) {
			error = (*ifp->if_ioctl)(ifp, cmd, (const caddr_t)argp);
		} else
			error = EINVAL;
		break;
	}

	return error;
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(if_link_state_change, &if_link_state_change, &_if_link_state_change);
#else
__func_tab__ void (*if_link_state_change)(struct ifnet *ifp, int linkState) = _if_link_state_change;
#endif

void
_if_link_state_change(struct ifnet *ifp, int linkState)
{
#if LWIP_NETIF_API
	struct netif *netif = &ifp->etherif;
#endif

	if (ifp->if_link_state == linkState)
		return;

#if LWIP_NETIF_API
	if (linkState == LINK_STATE_UP)
		netifapi_netif_set_link_up(netif);
	else
		netifapi_netif_set_link_down(netif);
#endif

	ifp->if_link_state = linkState;

	/* Notify that the link state has changed. */
	rt_ifmsg(ifp);
}

/*
 * Return counter values from counter(9)s stored in ifnet.
 */
uint64_t
if_get_counter_default(struct ifnet *ifp, ift_counter cnt)
{

	KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));

#ifdef CONFIG_SUPPORT_IF_STATS
	switch (cnt) {
		case IFCOUNTER_IPACKETS:
			return atomic_get_64((int64_t *)&ifp->if_ipackets);
		case IFCOUNTER_IERRORS:
			return atomic_get_64((int64_t *)&ifp->if_ierrors);
		case IFCOUNTER_OPACKETS:
			return atomic_get_64((int64_t *)&ifp->if_opackets);
		case IFCOUNTER_OERRORS:
			return atomic_get_64((int64_t *)&ifp->if_oerrors);
		case IFCOUNTER_COLLISIONS:
			return 0;
		case IFCOUNTER_IBYTES:
			return atomic_get_64((int64_t *)&ifp->if_ibytes);
		case IFCOUNTER_OBYTES:
			return atomic_get_64((int64_t *)&ifp->if_obytes);
		case IFCOUNTER_IMCASTS:
			return 0;
		case IFCOUNTER_OMCASTS:
			return atomic_get_64((int64_t *)&ifp->if_omcasts);
		case IFCOUNTER_IQDROPS:
			return atomic_get_64((int64_t *)&ifp->if_iqdrops);
		case IFCOUNTER_OQDROPS:
			return atomic_get_64((int64_t *)&ifp->if_oqdrops);
		case IFCOUNTER_NOPROTO:
			return 0;
		case IFCOUNTERS:
			KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));
	}
#endif
	return 0;
}

int
ether_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	struct route *ro)
{
	int error = 0;
	IFQ_HANDOFF(ifp, m, error);
	return error;
}



static void ether_input(struct ifnet *ifp, struct mbuf *m)
{
#ifdef CONFIG_LWIP
	struct netif *netif = &ifp->etherif;
	ethernetif_input(netif, m);
#else
	m_freem(m);
#endif
}

#ifdef CONFIG_LWIP
void ether_inputpbuf(struct ifnet *ifp, struct pbuf *p)
{
	ethernetif_inputpbuf(&ifp->etherif, p);
}
#endif

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ether_ifattach, &ether_ifattach, &_ether_ifattach);
#else
__func_tab__ void
(*ether_ifattach)(struct ifnet *ifp, const uint8_t *lla) = _ether_ifattach;
#endif

void
_ether_ifattach(struct ifnet *ifp, const uint8_t *lla)
{
	struct netif *netif = &ifp->etherif;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;

	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	if_attach(ifp);
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_input = ether_input;
	ifp->if_broadcastaddr = etherbroadcastaddr;

	ifa = ifp->if_addr;
	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ifp->if_addrlen;
	bcopy(lla, LLADDR(sdl), ifp->if_addrlen);

#if LWIP_NETIF_API
	netifapi_netif_add(netif, NULL, NULL, NULL, ifp,
					  ethernetif_init, tcpip_input);
#endif
	ifp->if_index = netif_get_index(netif);
#if LWIP_NETIF_API
	netifapi_netif_set_ioctl_fn(netif, ethernetif_ioctl);
#endif
}


void
ether_ifdetach(struct ifnet *ifp)
{
	struct netif *netif = &ifp->etherif;
	netif_remove(netif);
	if_detach(ifp);
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(ether_ioctl, &ether_ioctl, &_ether_ioctl);
#else
__func_tab__ int
(*ether_ioctl)(struct ifnet *ifp, u_long command, caddr_t data) = _ether_ioctl;
#endif

int
_ether_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *) data;

	switch (command) {
		case SIOCSIFMTU:
			if (ifr->ifr_mtu > ETHERMTU)
				return EINVAL;
			else
				;
			// need to fix our ifreq to work with C...
			// ifp->ifr_mtu = ifr->ifr_mtu;
			break;
		case SIOCGIFTXQLEN:
			{
				/* FIXME : process this per VAP */
				int len;
				IFQ_LEN(ifp, len);
				ifr->ifr_txqlen = len;
				break;
			}
		default:
			return EINVAL;
	}

	return 0;
}


/*
 * Initialization, destruction and refcounting functions for ifaddrs.
 */
struct ifaddr *
ifa_alloc(size_t size, int flags)
{
	struct ifaddr *ifa;

	KASSERT(size >= sizeof(struct ifaddr),
	    ("%s: invalid size %zu", __func__, size));

	ifa = malloc(size);
	if (ifa == NULL)
		return (NULL);
	else
		memset(ifa, 0, size);

	//refcount_init(&ifa->ifa_refcnt, 1);

	return (ifa);
}

void
ifa_ref(struct ifaddr *ifa)
{
	//refcount_acquire(&ifa->ifa_refcnt);
}

void
ifa_free(struct ifaddr *ifa)
{

	//if (refcount_release(&ifa->ifa_refcnt)) {
	//	free(ifa);
	//}
}

#ifdef CONFIG_LINK_TO_ROM
PROVIDE(if_inc_counter, &if_inc_counter, &_if_inc_counter);
#else
__func_tab__ void (*if_inc_counter) (struct ifnet *ifp, ift_counter cnt, int64_t inc) = _if_inc_counter;
#endif


__ilm_wlan_tx__ void
_if_inc_counter(struct ifnet *ifp, ift_counter cnt, int64_t inc)
{
#ifdef CONFIG_SUPPORT_IF_STATS
	switch (cnt) {
		case IFCOUNTER_IPACKETS:
			atomic_add_64((int64_t *)&ifp->if_ipackets, inc);
			break;
		case IFCOUNTER_IERRORS:
			atomic_add_64((int64_t *)&ifp->if_ierrors, inc);
			break;
		case IFCOUNTER_OPACKETS:
			atomic_add_64((int64_t *)&ifp->if_opackets, inc);
			break;
		case IFCOUNTER_OERRORS:
			atomic_add_64((int64_t *)&ifp->if_oerrors, inc);
			break;
		case IFCOUNTER_COLLISIONS:
			break;
		case IFCOUNTER_IBYTES:
			atomic_add_64((int64_t *)&ifp->if_ibytes, inc);
			break;
		case IFCOUNTER_OBYTES:
			atomic_add_64((int64_t *)&ifp->if_obytes, inc);
			break;
		case IFCOUNTER_IMCASTS:
			break;
		case IFCOUNTER_OMCASTS:
			atomic_add_64((int64_t *)&ifp->if_omcasts, inc);
			break;
		case IFCOUNTER_IQDROPS:
			atomic_add_64((int64_t *)&ifp->if_iqdrops, inc);
			break;
		case IFCOUNTER_OQDROPS:
			atomic_add_64((int64_t *)&ifp->if_oqdrops, inc);
			break;
		case IFCOUNTER_NOPROTO:
			break;
		case IFCOUNTERS:
			KASSERT(cnt < IFCOUNTERS, ("%s: invalid cnt %d", __func__, cnt));
	}
#endif
}

/*
 * Copy data from ifnet to userland API structure if_data.
 */
void
if_data_copy(struct ifnet *ifp, struct if_data *ifd)
{
	ifd->ifi_type = ifp->if_type;
	ifd->ifi_addrlen = ifp->if_addrlen;
	ifd->ifi_hdrlen = ifp->if_hdrlen;
	ifd->ifi_link_state = ifp->if_link_state;
	ifd->ifi_mtu = ifp->if_mtu;
	ifd->ifi_metric = ifp->if_metric;
#if CONFIG_STATS_IN_IF_DATA
	ifd->ifi_ipackets = if_get_counter_default(ifp, IFCOUNTER_IPACKETS);
	ifd->ifi_ierrors = if_get_counter_default(ifp, IFCOUNTER_IERRORS);
	ifd->ifi_opackets = if_get_counter_default(ifp, IFCOUNTER_OPACKETS);
	ifd->ifi_oerrors = if_get_counter_default(ifp, IFCOUNTER_OERRORS);
	ifd->ifi_ibytes = if_get_counter_default(ifp, IFCOUNTER_IBYTES);
	ifd->ifi_obytes = if_get_counter_default(ifp, IFCOUNTER_OBYTES);
	ifd->ifi_omcasts = if_get_counter_default(ifp, IFCOUNTER_OMCASTS);
	ifd->ifi_iqdrops = if_get_counter_default(ifp, IFCOUNTER_IQDROPS);
	ifd->ifi_oqdrops = if_get_counter_default(ifp, IFCOUNTER_OQDROPS);
#endif
}

const char *
if_getdname(if_t ifp)
{
	return ((struct ifnet *)ifp)->if_dname;
}

int
if_setdrvflagbits(if_t ifp, int set_flags, int clear_flags)
{
	((struct ifnet *)ifp)->if_drv_flags |= set_flags;
	((struct ifnet *)ifp)->if_drv_flags &= ~clear_flags;

	return (0);
}

int
if_getdrvflags(if_t ifp)
{
	return ((struct ifnet *)ifp)->if_drv_flags;
}

int
if_setdrvflags(if_t ifp, int flags)
{
	((struct ifnet *)ifp)->if_drv_flags = flags;
	return (0);
}


int
if_setflags(if_t ifp, int flags)
{
	((struct ifnet *)ifp)->if_flags = flags;
	return (0);
}

int
if_setflagbits(if_t ifp, int set, int clear)
{
	((struct ifnet *)ifp)->if_flags |= set;
	((struct ifnet *)ifp)->if_flags &= ~clear;

	return (0);
}

int
if_getflags(if_t ifp)
{
	return ((struct ifnet *)ifp)->if_flags;
}

int
if_setmtu(if_t ifp, int mtu)
{
	((struct ifnet *)ifp)->if_mtu = mtu;
	return (0);
}

int
if_getmtu(if_t ifp)
{
	return ((struct ifnet *)ifp)->if_mtu;
}

int
if_setsoftc(if_t ifp, void *softc)
{
	((struct ifnet *)ifp)->if_softc = softc;
	return (0);
}

void *
if_getsoftc(if_t ifp)
{
	return ((struct ifnet *)ifp)->if_softc;
}

void
if_setrcvif(struct mbuf *m, if_t ifp)
{
	m->m_pkthdr.rcvif = (struct ifnet *)ifp;
}

void
if_setvtag(struct mbuf *m, uint16_t tag)
{
	m->m_pkthdr.ether_vtag = tag;
}

uint16_t
if_getvtag(struct mbuf *m)
{

	return (m->m_pkthdr.ether_vtag);
}

int
if_sendq_empty(if_t ifp)
{
	return IFQ_DRV_IS_EMPTY(&((struct ifnet *)ifp)->if_snd);
}

int
if_setsendqready(if_t ifp)
{
	IFQ_SET_READY(&((struct ifnet *)ifp)->if_snd);
	return (0);
}

int
if_setsendqlen(if_t ifp, int tx_desc_count)
{
	IFQ_SET_MAXLEN(&((struct ifnet *)ifp)->if_snd, tx_desc_count);
	((struct ifnet *)ifp)->if_snd.ifq_drv_maxlen = tx_desc_count;

	return (0);
}

int
if_input(if_t ifp, struct mbuf* sendmp)
{
	(*((struct ifnet *)ifp)->if_input)((struct ifnet *)ifp, sendmp);
	return (0);

}

/* XXX */
#ifndef ETH_ADDR_LEN
#define ETH_ADDR_LEN 6
#endif

struct mbuf *
if_dequeue(if_t ifp)
{
	struct mbuf *m;
	IFQ_DRV_DEQUEUE(&((struct ifnet *)ifp)->if_snd, m);

	return (m);
}

int
if_sendq_prepend(if_t ifp, struct mbuf *m)
{
	IFQ_DRV_PREPEND(&((struct ifnet *)ifp)->if_snd, m);
	return (0);
}

int
if_setifheaderlen(if_t ifp, int len)
{
	((struct ifnet *)ifp)->if_hdrlen = len;
	return (0);
}

caddr_t
if_getlladdr(if_t ifp)
{
	return ((caddr_t)IF_LLADDR((struct ifnet *)ifp));
}

/*
 * Set the link layer address on an interface.
 *
 * At this time we only support certain types of interfaces,
 * and we don't allow the length of the address to change.
 *
 * Set noinline to be dtrace-friendly
 */
int
if_setlladdr(if_t ifp, const u_char *lladdr, int len)
{
	struct sockaddr_dl *sdl;
	struct ifaddr *ifa;
	struct ifreq ifr;

	ifa = ifp->if_addr;
	if (ifa == NULL) {
		return (EINVAL);
	}

	sdl = (struct sockaddr_dl *)ifa->ifa_addr;
	if (sdl == NULL) {
		return (EINVAL);
	}
	if (len != sdl->sdl_alen) {	/* don't allow length to change */
		return (EINVAL);
	}
	switch (ifp->if_type) {
	case IFT_ETHER:
#if 0
	case IFT_FDDI:
	case IFT_XETHER:
	case IFT_ISO88025:
	case IFT_L2VLAN:
	case IFT_BRIDGE:
	case IFT_ARCNET:
	case IFT_IEEE8023ADLAG:
#endif
		bcopy(lladdr, LLADDR(sdl), len);
		break;
	default:
		return (ENODEV);
	}

	/*
	 * If the interface is already up, we need
	 * to re-init it in order to reprogram its
	 * address filter.
	 */
	if ((ifp->if_flags & IFF_UP) != 0) {
		if_down(ifp);
		if (ifp->if_ioctl) {
			ifp->if_flags &= ~IFF_UP;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
			ifp->if_flags |= IFF_UP;
			ifr.ifr_flags = ifp->if_flags & 0xffff;
			ifr.ifr_flagshigh = ifp->if_flags >> 16;
			(*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, (caddr_t)&ifr);
		}
		if_up(ifp);
	}else {
		if_up(ifp); //just notify wpa_supplicant  drv->flags IFF_UP. we not really set up ifp
		if_down(ifp);//notify wpa_supplicant to disable  interface for update addr in the next if_up
		if_up(ifp); //notify wpa_spplicat to update addr
		ifp->if_flags &= ~IFF_UP;//recover ifp flags
	}
#ifdef __not_yet__
	EVENTHANDLER_INVOKE(iflladdr_event, ifp);
#endif
	return (0);
}


void *
if_gethandle(u_char type)
{
	return (if_alloc(type));
}

#ifdef __not_yet__
void
if_bpfmtap(if_t ifh, struct mbuf *m)
{
	struct ifnet *ifp = (struct ifnet *)ifh;

	BPF_MTAP(ifp, m);
}

void
if_etherbpfmtap(if_t ifh, struct mbuf *m)
{
	struct ifnet *ifp = (struct ifnet *)ifh;

	ETHER_BPF_MTAP(ifp, m);
}

void
if_vlancap(if_t ifh)
{
	struct ifnet *ifp = (struct ifnet *)ifh;
	VLAN_CAPABILITIES(ifp);
}
#endif

void
if_setinitfn(if_t ifp, void (*init_fn)(void *))
{
	((struct ifnet *)ifp)->if_init = init_fn;
}

void
if_setioctlfn(if_t ifp, int (*ioctl_fn)(if_t, u_long, caddr_t))
{
	((struct ifnet *)ifp)->if_ioctl = (void *)ioctl_fn;
}

void
if_setstartfn(if_t ifp, void (*start_fn)(if_t))
{
	((struct ifnet *)ifp)->if_start = (void *)start_fn;
}

void
if_settransmitfn(if_t ifp, if_transmit_fn_t start_fn)
{
	((struct ifnet *)ifp)->if_transmit = start_fn;
}

void if_setqflushfn(if_t ifp, if_qflush_fn_t flush_fn)
{
	((struct ifnet *)ifp)->if_qflush = flush_fn;
}
