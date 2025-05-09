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

#include "freebsd/compat_if_types.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include "freebsd/kernel.h"
#include "freebsd/taskqueue.h"
#include "freebsd/ethernet.h"
#include "freebsd/compat_if.h"
#include "freebsd/if_arp.h"
#include "freebsd/if_media.h"
#include "freebsd/if_var.h"
#include "freebsd/mbuf.h"
#include "freebsd/route.h"
#include "freebsd/lwip-glue.h"

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ppp/pppoe.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'r'
#define IFNAME1 't'

/**
 * In this function, the hardware should be initialized.
 * Called from routeif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this routeif
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
	netif->flags = (NETIF_FLAG_UP
			| NETIF_FLAG_ROUTE);
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this routeif
 * @param p the route message to handle
 * @return ERR_OK if the message could be handled
 *         an err_t value if the message couldn't be handled
 *
 */

static err_t
routeif_output(struct netif *netif, struct pbuf *p,
		const ip4_addr_t *ipaddr)
{
	(void)ipaddr;

	struct ifnet *ifp = netif->state;
	struct mbuf *m;

	m = m_frompbuf(p, 0);
	if (m == NULL)
		return ERR_VAL;

	route_output(ifp, m);

	return ERR_OK;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this routeif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
 */
static struct pbuf *
low_level_input(struct netif *netif, struct mbuf *m)
{
	struct pbuf *p;

	/* We get a pbuf chain of pbufs from mbuf. */
	p = m_topbuf(m);

	return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this routeif
 */
err_t
routeif_input(struct netif *netif, struct mbuf *m)
{
	err_t err;
	struct pbuf *p;

	/* move received packet into a new pbuf */
	p = low_level_input(netif, m);
	if (p == NULL) {
		return ERR_MEM;
	}
	/* if no packet could be read, silently ignore this */
	/* pass all packets to ethernet_input, which decides what packets it supports */
	if ((err = netif->input(p, netif)) != ERR_OK) {
		LWIP_DEBUGF(NETIF_DEBUG, ("routeif_input: IP input error\n"));
		/* err != ERR_BUF: netif->input not free the p */
		if (err != ERR_BUF)
			pbuf_free(p);
		p = NULL;
		return ERR_IF;
	}

	return ERR_OK;
}

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this routeif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
routeif_init(struct netif *netif)
{
	struct ifnet *ifp;
	LWIP_ASSERT("netif != NULL", (netif != NULL));
	/* Note : netif->state is initialized in netif_add() before this */
	ifp = netif->state;
	LWIP_ASSERT("ifp != NULL", (ifp != NULL));
#ifdef LWIP_NOASSERT
	UNUSED(ifp);
#endif


#if LWIP_NETIF_HOSTNAME
	/* Initialize interface hostname */
	netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	/* We directly use etharp_output() here to save a function call.
	* You can instead declare your own function an call etharp_output()
	* from it if you have to do some checks before sending (e.g. if link
	* is available...) */
	netif->output = routeif_output;

	/* initialize the hardware */
	low_level_init(netif);

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

/**
 * This is the ioctl function per interface.
 * Called from netif_ioctl().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this routeif
 * @param cmd ioctl command
 * @param argp ioctl parameter
 */
err_t
routeif_ioctl(struct netif *netif, const long cmd,
		const void *argp)
{
	err_t err = ERR_OK;
	int port = 0;
	struct ifreq *ifr = (struct ifreq *)argp;
	struct ifnet *ifp = netif->state;
	u32_t addr;
	ip_addr_t naddr;
	struct sockaddr *si;

	switch (cmd) {
	case SIOCGIFADDR:
		naddr = *(ip_addr_t *)netif_ip_addr4(netif);
		IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_addr),
				(ip4_addr_t *)&naddr, port);
		break;
	case SIOCGIFNETMASK:
		naddr = netif->netmask;
		IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in *)&(ifr->ifr_netmask),
				(ip4_addr_t *)&naddr, port);
		break;
	case SIOCGIFBRDADDR:
		addr = ip4_addr_get_u32(netif_ip4_addr(netif));
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
	case SIOCSIFADDR:
		si = &ifr->ifr_addr;
		SOCKADDR4_TO_IP4ADDR_PORT((struct sockaddr_in *)si, (ip4_addr_t *)&naddr);
		netif_set_ipaddr(netif, (const ip4_addr_t *)&naddr);
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
		break;
	case SIOCGIFSTATUS: {
#if NETIF_STATS
		struct stats_netif *stats;
		if (ifr->ifr_buffer.length >= sizeof(*stats)) {
			stats = (struct stats_netif *)ifr->ifr_buffer.buffer;
			memcpy((void *)stats, &netif->stats, sizeof(*stats));
		} else {
			LWIP_ASSERT("netstats:size invalid\n", false);
			err = ERR_IF;
		}
#else
		LWIP_ASSERT("netstats not supported\n", false);
		err = ERR_IF;
#endif
		break;
	}
	default:
		if (if_ioctl(ifp, cmd, argp))
			err = ERR_IF;
	}
	return err;
}
