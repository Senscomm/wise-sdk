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
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "freebsd/compat_if_types.h"
#include "freebsd/kernel.h"
#include "freebsd/taskqueue.h"
#include "freebsd/ethernet.h"
#include "freebsd/compat_if.h"
#include "freebsd/if_arp.h"
#include "freebsd/if_media.h"
#include "freebsd/if_var.h"
#include "freebsd/mbuf.h"
#include "freebsd/lwip-glue.h"

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
#include "netif/ppp/pppoe.h"

#include "net80211/ieee80211_var.h"

#ifdef CONFIG_SUPPORT_MTSMP
#include "mtsmp.h"
#endif

/* Define those to better describe your network interface. */
#define IFNAME0 'w'
#define IFNAME1 'l'


#ifndef CONFIG_FREEBSD_MBUF_DYNA_EXT
#define LOW_LEVEL_TRX_MEM_SHARE
#endif

#ifdef LOW_LEVEL_TRX_MEM_SHARE
#define MBUF_RX_RESERVED_NUM (1 << CONFIG_RX_BUF_NUM_LOG2)
#define MBUF_TX_RESERVED_NUM CONFIG_TX_BUF_RESERVED_NUM
#if ((CONFIG_MEMP_NUM_MBUF_CACHE - MBUF_RX_RESERVED_NUM) < (MBUF_RX_RESERVED_NUM + MBUF_TX_RESERVED_NUM))
#error "CONFIG_MEMP_NUM_MBUF_CACHE shall be greater than 2^(CONFIG_RX_BUF_NUM_LOG2) + MBUF_TX_RESERVED_NUM"
#endif
#endif

/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
low_level_init(struct netif *netif)
{
	struct ifnet *ifp = netif->state;

	/* set MAC hardware address length */
	netif->hwaddr_len = ifp->if_addrlen;

	/* set MAC hardware address */
	bcopy(IF_LLADDR(ifp), netif->hwaddr, ifp->if_addrlen);

	/* maximum transfer unit */
	netif->mtu = if_getmtu(ifp);

	/* device capabilities */
	/* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
	netif->flags = (NETIF_FLAG_BROADCAST
					| NETIF_FLAG_ETHARP
					| NETIF_FLAG_ETHERNET
					| NETIF_FLAG_IGMP
					/*| NETIF_FLAG_LINK_UP*/);
#if LWIP_IPV6 && LWIP_IPV6_MLD

	netif->flags |= NETIF_FLAG_MLD6;

	/*
	* For hardware/netifs that implement MAC filtering.
	* All-nodes link-local is handled by default, so we must let the hardware know
	* to allow multicast packets in.
	* Should set mld_mac_filter previously. */
	if (netif->mld_mac_filter != NULL) {
		ip6_addr_t ip6_allnodes_ll;
		ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
		netif->mld_mac_filter(netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
	}
#endif /* LWIP_IPV6 && LWIP_IPV6_MLD */

	netif_set_default(netif);
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become available since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_mem_available(struct pbuf *p, struct ifnet *ifp, struct mbuf **m)
{
#ifdef LOW_LEVEL_TRX_MEM_SHARE
	if (memp_available(MEMP_MBUF_CACHE) < MBUF_RX_RESERVED_NUM) {
		return ERR_MEM;
	}
#endif

	/*
	 * FIXME:
	 * we might create a new parameter if_low_level_hdrlen in the furture
	 */
	*m = m_frompbuf(p, ifp->if_hdrlen);

	if (*m == NULL)
		return ERR_MEM;

	return ERR_OK;
}

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
	struct ifnet *ifp = netif->state;
	struct mbuf *m = NULL;
	int retry = 600;
	struct sockaddr dst = {.sa_family = AF_INET};
	bool is_critical = false;

#if ETH_PAD_SIZE
  	pbuf_remove_header(p, ETH_PAD_SIZE); /* drop the padding word */
#endif
try_alloc:

	if (p->flags & PBUF_FLAG_IEEE80211) {
		dst.sa_family = AF_IEEE80211;
	}

	if (p->flags & PBUF_FLAG_CRITICAL) {
		is_critical = true;
	}

	if (low_level_mem_available(p, ifp, &m) != ERR_OK) {
		if (retry-- > 0 || is_critical) {
			osDelay(pdMS_TO_TICKS(1));
			goto try_alloc;
		}
		LWIP_DEBUGF(NETIF_DEBUG, ("low_level_output: no enough mbuf\n"));
#ifdef CONFIG_SCM2010_TX_TIMEOUT_DEBUG
		printk("low_level_output: no enough mbuf\n");
#endif
		return ERR_OK;
	}

#ifdef CONFIG_SUPPORT_MTSMP
	if (m->m_pkthdr.len > PACKET_LEN_THR) {
		if (tsmp_get_count(outcount) >= COUNT_NUM)
			tsmp_set_count(outcount, 0);
		if (!tsmp_valid(out[tsmp_get_count(outcount)])) {
			tsmp_set_idx(m, tsmp_get_count(outcount) + 1);
			tsmp_set_out(m, m_out, (void * )m);
			tsmp_time_out(m, start_t);
		}
		tsmp_inc_count(outcount);
	}
#endif

	ifp->if_output(ifp, m, &dst, NULL);

	MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
	if (((u8_t *)p->payload)[0] & 1) {
		/* broadcast or multicast packet*/
		MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
	} else {
		/* unicast packet */
		MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
	}
	/* increase ifoutdiscards or ifouterrors on error */

#if ETH_PAD_SIZE
	pbuf_add_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

	LINK_STATS_INC(link.xmit);
	NETIF_STATS_INC(netif, xmit);

	return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif, struct mbuf *m)
{
	struct pbuf *p;

	/* We get a pbuf chain of pbufs from mbuf. */
	p = m_topbuf(m);

	if (p != NULL) {
		MIB2_STATS_NETIF_ADD(netif, ifinoctets, p->tot_len);
		if (((u8_t *)p->payload)[0] & 1) {
			/* broadcast or multicast packet*/
			MIB2_STATS_NETIF_INC(netif, ifinnucastpkts);
		} else {
			/* unicast packet*/
			MIB2_STATS_NETIF_INC(netif, ifinucastpkts);
		}
		LINK_STATS_INC(link.recv);
		NETIF_STATS_INC(netif, recv);
	} else {
		LINK_STATS_INC(link.memerr);
		NETIF_STATS_INC(netif, memerr);
		LINK_STATS_INC(link.drop);
		NETIF_STATS_INC(netif, drop);
		MIB2_STATS_NETIF_INC(netif, ifindiscards);
	}
	return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void
ethernetif_input(struct netif *netif, struct mbuf *m)
{
	struct pbuf *p;

	/* move received packet into a new pbuf */
	p = low_level_input(netif, m);
	/* if no packet could be read, silently ignore this */
	if (p != NULL) {

#ifdef CONFIG_SUPPORT_MTSMP
		tsmp_time_in(m, end_t);
		tsmp_set_idx(m, 0);
		tsmp_clean_in(m);
#endif

		/* pass all packets to ethernet_input, which decides what packets it supports */
		if (netif->input(p, netif) != ERR_OK) {
			LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
			pbuf_free(p);
			p = NULL;
		}
	}
}

void
ethernetif_inputpbuf(struct netif *netif, struct pbuf *p)
{
	/* if no packet could be read, silently ignore this */
	if (p != NULL) {
		/* pass all packets to ethernet_input, which decides what packets it supports */
		if (netif->input(p, netif) != ERR_OK) {
			LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
			pbuf_free(p);
			p = NULL;
		}
	}
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
ethernetif_init(struct netif *netif)
{
	struct ifnet *ifp;
	LWIP_ASSERT("netif != NULL", (netif != NULL));
	/* Note : netif->state is initialized in netif_add() before this */
	ifp = netif->state;
	LWIP_ASSERT("ifp != NULL", (ifp != NULL));

#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	/*
	* Initialize the snmp variables and counters inside the struct netif.
	* The last argument should be replaced with your link speed, in units
	* of bits per second.
	*/
	MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, LINK_SPEED_OF_YOUR_NETIF_IN_BPS);

#if 1
	strncpy(netif->name, ifp->if_xname, sizeof(netif->name));
#else
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
#endif
	/* We directly use etharp_output() here to save a function call.
	* You can instead declare your own function an call etharp_output()
	* from it if you have to do some checks before sending (e.g. if link
	* is available...) */
#if LWIP_IPV4 && LWIP_ARP
	netif->output = etharp_output;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif /* LWIP_IPV6 */
	netif->linkoutput = low_level_output;

	/* initialize the hardware */
	low_level_init(netif);

#if LWIP_IPV6
	netif_create_ip6_linklocal_address(netif, 1);
#endif

	return ERR_OK;
}

/*
 * FIXME : ugly because these are already in sockets.c
 */
#define IP4ADDR_PORT_TO_SOCKADDR(sin, ipaddr, port) do { \
	(sin)->sin_len = sizeof(struct sockaddr_in); \
	(sin)->sin_family = AF_INET; \
	(sin)->sin_port = lwip_htons((port)); \
	(sin)->sin_addr.s_addr = ip4_addr_get_u32(ipaddr); \
	memset((sin)->sin_zero, 0, sizeof((sin)->sin_zero)); }while(0)
#define SOCKADDR4_TO_IP4ADDR_PORT(sin, ipaddr) do { \
	ip4_addr_set_u32(ipaddr, (sin)->sin_addr.s_addr); }while(0)

#if LWIP_IPV6
#define IP6ADDR_PORT_TO_SOCKADDR(sin6, ipaddr, port) do { \
	(sin6)->sin6_len = sizeof(struct sockaddr_in6); \
	(sin6)->sin6_family = AF_INET6; \
	(sin6)->sin6_port = lwip_htons((port)); \
	inet6_addr_from_ip6addr(&(sin6)->sin6_addr, ipaddr); \
	(sin6)->sin6_scope_id = ip6_addr_zone(ipaddr); }while(0)
#define SOCKADDR6_TO_IP6ADDR(sin6, ipaddr) do { \
	inet6_addr_to_ip6addr(ip_2_ip6(ipaddr), &((sin6)->sin6_addr)); \
	if (ip6_addr_has_scope(ip_2_ip6(ipaddr), IP6_UNKNOWN)) { \
		ip6_addr_set_zone(ip_2_ip6(ipaddr), (u8_t)((sin6)->sin6_scope_id)); \
	} }while(0)
#endif /* LWIP_IPV6 */

/**
 * This is the ioctl function per interface.
 * Called from netif_ioctl().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 * @param cmd ioctl command
 * @param argp ioctl parameter
 */
err_t
ethernetif_ioctl(struct netif *netif, const long cmd,
		const void *argp)
{
	err_t err = ERR_OK;
	int port = 0;
	struct ifreq *ifr = (struct ifreq *)argp;
#ifdef CONFIG_LWIP_IPV6
	struct ifreq6 *ifr6 = (struct ifreq6 *)argp;
#endif
	struct ifnet *ifp = netif->state;
	u32_t addr;
	ip_addr_t naddr;
	struct sockaddr *si;

	switch (cmd) {
	case SIOCGIFADDR:
#ifdef CONFIG_LWIP_IPV6
		if (ifr->ifr_addr.sa_family == AF_INET6) {
			if (ifr6->ifr6_ifindex >= LWIP_IPV6_NUM_ADDRESSES) {
				break;
			}
			if (ip6_addr_isinvalid(netif_ip6_addr_state(netif, ifr6->ifr6_ifindex))) {
				break;
			}
			naddr = *(ip_addr_t *)netif_ip_addr6(netif, ifr6->ifr6_ifindex);
			IP6ADDR_PORT_TO_SOCKADDR(&(ifr6->ifr6_addr),
				(ip6_addr_t *)&naddr, port);
		} else
#endif
		{
			naddr = *(ip_addr_t *)netif_ip_addr4(netif);
			IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_addr),
				(ip4_addr_t *)&naddr, port);
		}
		break;
	case SIOCGIFNETMASK:
		naddr = netif->netmask;
		IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_netmask),
				(ip4_addr_t *)&naddr, port);
		break;
	case SIOCGIFBRDADDR:
		addr = ip4_addr_get_u32((ip4_addr_t *)netif_ip_addr4(netif));
		addr &= ip4_addr_get_u32(netif_ip4_netmask(netif));
		addr |= (IPADDR_BROADCAST & ~ip4_addr_get_u32(netif_ip4_netmask(netif)));
		ip4_addr_set_u32((ip4_addr_t *)&naddr, addr);
		IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_broadaddr),
				(ip4_addr_t *)&naddr, port);
		break;
	case SIOCGIFHWADDR:
		bcopy(netif->hwaddr,
		      (caddr_t) ifr->ifr_hwaddr.sa_data, netif->hwaddr_len);
		break;
#ifdef __WISE__
	case SIOCGIFGWADDR:
		naddr = *(ip_addr_t *)netif_ip_gw4(netif);
		IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_gateway),
				(ip4_addr_t *)&naddr, port);
		break;
#endif
	case SIOCSIFADDR:
		if (ifr->ifr_addr.sa_family == AF_INET) {
			si = &ifr->ifr_addr;
			SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)si, (ip4_addr_t *)&naddr);
			netif_set_ipaddr(netif, (const ip4_addr_t *)&naddr);
		}
#ifdef CONFIG_LWIP_IPV6
		else if (ifr->ifr_addr.sa_family == AF_INET6) {
			SOCKADDR6_TO_IP6ADDR(&(ifr6->ifr6_addr), &naddr);
			if (ifr6->ifr6_add) {
				netif_add_ip6_address(netif,  (const ip6_addr_t *)&naddr, NULL);
			} else {
				s8_t i = netif_get_ip6_addr_match(netif, (const ip6_addr_t *)&naddr);
				netif_ip6_addr_set_state(netif, i, IP6_ADDR_INVALID);
				ip_addr_set_zero_ip6(&netif->ip6_addr[i]);
			}
		}
#endif
		break;
	case SIOCSIFBRDADDR:
		LWIP_ASSERT("SIOCSIFBRDADDR not supported\n", false);
		err = ERR_IF;
		break;
	case SIOCSIFNETMASK:
		si = &ifr->ifr_netmask;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)si, (ip4_addr_t *)&naddr);
		netif_set_netmask(netif, (const ip4_addr_t *)&naddr);
		break;
	case SIOCSIFHWADDR:
		si = &ifr->ifr_hwaddr;
		/* set MAC hardware address length */
		netif->hwaddr_len = si->sa_len;
		/* set MAC hardware address */
		bcopy(si->sa_data, netif->hwaddr, netif->hwaddr_len);
		if (if_ioctl(ifp, cmd, argp)) {
			err = ERR_IF;
			break;
		}
#if LWIP_IPV6
		netif_create_ip6_linklocal_address(netif, 1);
#endif

		break;
#ifdef __WISE__
	case SIOCSIFGWADDR:
		si = &ifr->ifr_gateway;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)si,
                (ip4_addr_t *)&naddr);
		netif_set_gw(netif, (const ip4_addr_t *)&naddr);
		break;
#endif
#if 1
	case SIOCGIFSTATUS: {
		struct ifstat *ifs = (struct ifstat *) ifr;
		int len = sizeof(ifs->ascii);
		char *start = ifs->ascii, *end __maybe_unused = start + len;
		if_ioctl(ifp, cmd, argp);
#if !CONFIG_SUPPORT_IF_STATS && NETIF_STATS
		start += snprintf(start, end - start,
				  "\tRX packets %u dropped %u\n\tTX packets %u\n",
				  (unsigned) netif->stats.recv,
				  (unsigned) netif->stats.drop,
				  (unsigned) netif->stats.xmit);
#endif
		break;
	}
#else
	case SIOCGIFSTATUS: {
		struct stats_netif *stats;
#ifdef NETIF_STATS
		if (ifr->ifr_buffer.length >= sizeof(*stats)) {
			stats = (struct stats_netif *)ifr->ifr_buffer.buffer;
			memcpy((void *)stats, &netif->stats, sizeof(*stats));
		} else {
			LWIP_ASSERT("netstats:size invalid\n", false);
			err = ERR_IF;
		}
#else
		stats.xmit = stats.recv = stats.drop = stats.memerr = 0;
		LWIP_ASSERT("netstats not supported\n", false);
		err = ERR_IF;
#endif

		break;
	}
#endif
	default:
		/*
		 * NB: In order to give a chance to interface worker thread
		 * to run to the completion even if they calls to lwip functions
		 * that needs core locking, we temporatily unlock and
		 * relock the tcpip core lock before and after calling if_ioctl().
		 */
		if (if_ioctl(ifp, cmd, argp))
			err = ERR_IF;
	}
	return err;
}
