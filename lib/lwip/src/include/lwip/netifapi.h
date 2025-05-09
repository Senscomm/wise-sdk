/**
 * @file
 * netif API (to be used from non-TCPIP threads)
 */

/*
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 */
#ifndef LWIP_HDR_NETIFAPI_H
#define LWIP_HDR_NETIFAPI_H

#include "lwip/opt.h"

#if LWIP_NETIF_API /* don't build if not configured for use in lwipopts.h */

#include "lwip/sys.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/priv/api_msg.h"
#include "lwip/prot/ethernet.h"

#ifdef __cplusplus
extern "C" {
#endif

/* API for application */
#if LWIP_ARP && LWIP_IPV4
/* Used for netfiapi_arp_* APIs */
enum netifapi_arp_entry {
  NETIFAPI_ARP_PERM /* Permanent entry */
  /* Other entry types can be added here */
};

/** @ingroup netifapi_arp */
err_t netifapi_arp_add(const ip4_addr_t *ipaddr, struct eth_addr *ethaddr, enum netifapi_arp_entry type);
/** @ingroup netifapi_arp */
err_t netifapi_arp_remove(const ip4_addr_t *ipaddr, enum netifapi_arp_entry type);
#endif /* LWIP_ARP && LWIP_IPV4 */

err_t netifapi_netif_add(struct netif *netif,
#if LWIP_IPV4
                         const ip4_addr_t *ipaddr, const ip4_addr_t *netmask, const ip4_addr_t *gw,
#endif /* LWIP_IPV4 */
                         void *state, netif_init_fn init, netif_input_fn input);

err_t netifapi_netif_set_ioctl_fn(struct netif *netif,
			netif_ioctl_fn ioctl);

#if LWIP_IPV4
err_t netifapi_netif_set_addr(struct netif *netif, const ip4_addr_t *ipaddr,
                              const ip4_addr_t *netmask, const ip4_addr_t *gw);
#endif /* LWIP_IPV4*/

err_t netifapi_netif_common(struct netif *netif, netifapi_void_fn voidfunc,
                            netifapi_errt_fn errtfunc,
							netifapi_args_fn argsfunc, void *arg);
err_t netifapi_netif_ioctl(const long cmd, const void *argp);

#if defined(__WISE__) && defined(LWIP_NETIF_EXT_STATUS_CALLBACK)
/** @ingroup netifapi_netif */
err_t netifapi_add_ext_callback(netif_ext_callback_t *callback, netif_ext_callback_fn fn);
/** @ingroup netifapi_netif */
err_t netifapi_remove_ext_callback(netif_ext_callback_t *callback);
#endif

/** @ingroup netifapi_netif */
err_t netifapi_netif_name_to_index(const char *name, u8_t *index);
/** @ingroup netifapi_netif */
err_t netifapi_netif_name_to_flags(const char *name, u8_t *flags);
/** @ingroup netifapi_netif */
err_t netifapi_netif_index_to_name(u8_t index, char *name);
#ifdef __WISE__
/** @ingroup netifapi_netif */
extern struct netif *(*netifapi_netif_find)(const char *name);
#endif

#define argsfn(fn)								((netifapi_args_fn)fn)
#define voidpt(a)								(void *)a

#define netifapi_netif_commonv(n, vfn)		netifapi_netif_common(n, vfn, NULL, NULL, NULL)
#define netifapi_netif_commone(n, efn)		netifapi_netif_common(n, NULL, efn, NULL, NULL)
#define netifapi_netif_commona(n, afn, a)	netifapi_netif_common(n, NULL, NULL, argsfn(afn), voidpt(a))

/** @ingroup netifapi_netif
  * @see netif_remove()
  */
#define netifapi_netif_remove(n)            netifapi_netif_commonv(n, netif_remove)
/** @ingroup netifapi_netif
  * @see netif_set_up()
  */
#define netifapi_netif_set_up(n)            netifapi_netif_commonv(n, netif_set_up)
/** @ingroup netifapi_netif
  * @see netif_set_down()
  */
#define netifapi_netif_set_down(n)          netifapi_netif_commonv(n, netif_set_down)
/** @ingroup netifapi_netif
  * @see netif_set_default()
  */
#define netifapi_netif_set_default(n)       netifapi_netif_commonv(n, netif_set_default)
/** @ingroup netifapi_netif
  * @see netif_set_link_up()
  */
#define netifapi_netif_set_link_up(n)       netifapi_netif_commonv(n, netif_set_link_up)
/** @ingroup netifapi_netif
  * @see netif_set_link_down()
  */
#define netifapi_netif_set_link_down(n)     netifapi_netif_commonv(n, netif_set_link_down)

/** @ingroup netifapi_netif
  * @see netif_set_hostname()
  */
#define netifapi_netif_set_hostname(n, a)     netifapi_netif_commona(n, netif_set_hostname, a)

/**
 * @defgroup netifapi_dhcp4 DHCPv4
 * @ingroup netifapi
 * To be called from non-TCPIP threads
 */
/** @ingroup netifapi_dhcp4
  * @see dhcp_start()
  */
#define netifapi_dhcp_start(n)              netifapi_netif_commone(n, dhcp_start)
/**
 * @ingroup netifapi_dhcp4
  * @see dhcp_stop()
 */
#define netifapi_dhcp_stop(n)               netifapi_netif_commonv(n, dhcp_stop)
/** @ingroup netifapi_dhcp4
  * @see dhcps_start()
  */
#define netifapi_dhcps_start(n)             netifapi_netif_commone(n, dhcps_start)
/**
 * @ingroup netifapi_dhcp4
  * @see dhcps_stop()
 */
#define netifapi_dhcps_stop(n)              netifapi_netif_commone(n, dhcps_stop)
/**
 * @ingroup netifapi_dhcp4
  * @see dhcps_configure()
 */
#define netifapi_dhcps_configure(n, a)      netifapi_netif_commona(n, dhcps_configure, a)
/**
 * @ingroup netifapi_dhcp4
  * @see dhcps_setdns()
 */
#define netifapi_dhcps_setdns(n, a)         netifapi_netif_commona(n, dhcps_setdns, a)
/**
 * @ingroup netifapi_dhcp4
  * @see dhcps_getdns()
 */
#define netifapi_dhcps_getdns(n, a)         netifapi_netif_commona(n, dhcps_getdns, a)
/** @ingroup netifapi_dhcp4
  * @see dhcp_inform()
  */
#define netifapi_dhcp_inform(n)             netifapi_netif_commonv(n, dhcp_inform)
/** @ingroup netifapi_dhcp4
  * @see dhcp_renew()
  */
#define netifapi_dhcp_renew(n)              netifapi_netif_commone(n, dhcp_renew)
/**
 * @ingroup netifapi_dhcp4
 * @deprecated Use netifapi_dhcp_release_and_stop() instead.
 */
#define netifapi_dhcp_release(n)            netifapi_netif_commone(n, dhcp_release)
/** @ingroup netifapi_dhcp4
  * @see dhcp_release_and_stop()
  */
#define netifapi_dhcp_release_and_stop(n)   netifapi_netif_commonv(n, dhcp_release_and_stop)
#ifdef __WISE__
#define netifapi_dhcp_stop_coarse_tmr(n)    netifapi_netif_commonv(n, dhcp_stop_coarse_tmr)
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
#define netifapi_netif_tcp_start_timers(n)  netifapi_netif_commonv(n, tcp_start_timers)
#define netifapi_netif_tcp_stop_timers(n)  netifapi_netif_commonv(n, tcp_stop_timers)
#endif
#endif

/**
 * @defgroup netifapi_autoip AUTOIP
 * @ingroup netifapi
 * To be called from non-TCPIP threads
 */
/** @ingroup netifapi_autoip
  * @see autoip_start()
  */
#define netifapi_autoip_start(n)            netifapi_netif_commone(n, autoip_start)
/** @ingroup netifapi_autoip
  * @see autoip_stop()
  */
#define netifapi_autoip_stop(n)             netifapi_netif_commone(n, autoip_stop)

#ifdef __cplusplus
}
#endif

#endif /* LWIP_NETIF_API */

#endif /* LWIP_HDR_NETIFAPI_H */
