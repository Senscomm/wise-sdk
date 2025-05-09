/**
 * @file
 * Network Interface Sequential API module
 *
 * @defgroup netifapi NETIF API
 * @ingroup sequential_api
 * Thread-safe functions to be called from non-TCPIP threads
 *
 * @defgroup netifapi_netif NETIF related
 * @ingroup netifapi
 * To be called from non-TCPIP threads
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

#include "lwip/opt.h"

#if LWIP_NETIF_API /* don't build if not configured for use in lwipopts.h */

#include "lwip/etharp.h"
#include "lwip/netifapi.h"
#include "lwip/memp.h"
#include "lwip/priv/tcpip_priv.h"

#include <string.h> /* strncpy */

#define NETIFAPI_VAR_REF(name)      API_VAR_REF(name)
#define NETIFAPI_VAR_DECLARE(name)  API_VAR_DECLARE(struct netifapi_msg, name)
#define NETIFAPI_VAR_ALLOC(name)    API_VAR_ALLOC(struct netifapi_msg, MEMP_NETIFAPI_MSG, name, ERR_MEM)
#define NETIFAPI_VAR_FREE(name)     API_VAR_FREE(MEMP_NETIFAPI_MSG, name)

/**
 * Call netif_add() inside the tcpip_thread context.
 */
static err_t
netifapi_do_netif_add(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  if (!netif_add( msg->netif,
#if LWIP_IPV4
                  API_EXPR_REF(msg->msg.add.ipaddr),
                  API_EXPR_REF(msg->msg.add.netmask),
                  API_EXPR_REF(msg->msg.add.gw),
#endif /* LWIP_IPV4 */
                  msg->msg.add.state,
                  msg->msg.add.init,
                  msg->msg.add.input)) {
    return ERR_IF;
  } else {
    return ERR_OK;
  }
}

/**
 * Call netif_set_ioctl_fn() inside the tcpip_thread context.
 */
static err_t
netifapi_do_netif_set_ioctl_fn(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  netif_set_ioctl_fn(msg->netif, msg->msg.set_ioctl.ioctl);
  return ERR_OK;
}
#if LWIP_IPV4
/**
 * Call netif_set_addr() inside the tcpip_thread context.
 */
static err_t
netifapi_do_netif_set_addr(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  netif_set_addr( msg->netif,
                  API_EXPR_REF(msg->msg.add.ipaddr),
                  API_EXPR_REF(msg->msg.add.netmask),
                  API_EXPR_REF(msg->msg.add.gw));
  return ERR_OK;
}
#endif /* LWIP_IPV4 */

/**
* Call netif_name_to_index() inside the tcpip_thread context.
*/
static err_t
netifapi_do_name_to_index(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  msg->msg.ifs.index = netif_name_to_index(msg->msg.ifs.name);
  return ERR_OK;
}

/**
* Call netif_name_to_flags() inside the tcpip_thread context.
*/
static err_t
netifapi_do_name_to_flags(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  msg->msg.ifs.flags = netif_name_to_flags(msg->msg.ifs.name);
  return ERR_OK;
}

/**
* Call netif_index_to_name() inside the tcpip_thread context.
*/
static err_t
netifapi_do_index_to_name(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  if (!netif_index_to_name(msg->msg.ifs.index, msg->msg.ifs.name)) {
    /* return failure via empty name */
    msg->msg.ifs.name[0] = '\0';
  }
  return ERR_OK;
}

#ifdef __WISE__

/**
* Call netif_find() inside the tcpip_thread context.
*/
static err_t
netifapi_do_find(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  /* XXX: returning a pointer like this won't protect access into it.
   *      But there is no alternative if we keep the semantics of
   *      netifapi_netif_common calls.
   *      We might need to device a better way down the road.
   */
  msg->netif = netif_find(msg->msg.ifs.name);
  return ERR_OK;
}

#endif

/**
 * Call the "errtfunc" (or the "voidfunc" if "errtfunc" is NULL) inside the
 * tcpip_thread context.
 */
static err_t
netifapi_do_netif_common(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  if (msg->msg.common.argsfunc != NULL) {
    return msg->msg.common.argsfunc(msg->netif, msg->msg.common.arg);
  } else if (msg->msg.common.errtfunc != NULL) {
    return msg->msg.common.errtfunc(msg->netif);
  } else {
    msg->msg.common.voidfunc(msg->netif);
    return ERR_OK;
  }
}

/**
* Call netif_ioctl() inside the tcpip_thread context.
*/
static err_t
netifapi_do_netif_ioctl(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

#ifdef __WISE__
  return netif_ioctl(msg->msg.ioctl.cmd, msg->msg.ioctl.argp);
#else
  /*FIXME : error handling? */
  netif_ioctl(msg->msg.ioctl.cmd, msg->msg.ioctl.argp);

  return ERR_OK;
#endif
}

#if defined(__WISE__) && defined(LWIP_NETIF_EXT_STATUS_CALLBACK)

/**
 * Call netif_add_ext_callback() inside the tcpip_thread context.
 */
static err_t
netifapi_do_add_ext_callback(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  netif_add_ext_callback(API_EXPR_REF(msg->msg.cb.callback),
                  API_EXPR_REF(msg->msg.cb.callback_fn));
  return ERR_OK;
}

/**
 * Call netif_remove_ext_callback() inside the tcpip_thread context.
 */
static err_t
netifapi_do_remove_ext_callback(struct tcpip_api_call_data *m)
{
  /* cast through void* to silence alignment warnings.
   * We know it works because the structs have been instantiated as struct netifapi_msg */
  struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

  netif_remove_ext_callback(API_EXPR_REF(msg->msg.cb.callback));
  return ERR_OK;
}

#endif

#if LWIP_ARP && LWIP_IPV4
/**
 * @ingroup netifapi_arp
 * Add or update an entry in the ARP cache.
 * For an update, ipaddr is used to find the cache entry.
 *
 * @param ipaddr IPv4 address of cache entry
 * @param ethaddr hardware address mapped to ipaddr
 * @param type type of ARP cache entry
 * @return ERR_OK: entry added/updated, else error from err_t
 */
err_t
netifapi_arp_add(const ip4_addr_t *ipaddr, struct eth_addr *ethaddr, enum netifapi_arp_entry type)
{
  err_t err;

  /* We only support permanent entries currently */
  LWIP_UNUSED_ARG(type);

#if ETHARP_SUPPORT_STATIC_ENTRIES && LWIP_TCPIP_CORE_LOCKING
  LOCK_TCPIP_CORE();
  err = etharp_add_static_entry(ipaddr, ethaddr);
  UNLOCK_TCPIP_CORE();
#else
  /* @todo add new vars to struct netifapi_msg and create a 'do' func */
  LWIP_UNUSED_ARG(ipaddr);
  LWIP_UNUSED_ARG(ethaddr);
  err = ERR_VAL;
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES && LWIP_TCPIP_CORE_LOCKING */

  return err;
}

/**
 * @ingroup netifapi_arp
 * Remove an entry in the ARP cache identified by ipaddr
 *
 * @param ipaddr IPv4 address of cache entry
 * @param type type of ARP cache entry
 * @return ERR_OK: entry removed, else error from err_t
 */
err_t
netifapi_arp_remove(const ip4_addr_t *ipaddr, enum netifapi_arp_entry type)
{
  err_t err;

  /* We only support permanent entries currently */
  LWIP_UNUSED_ARG(type);

#if ETHARP_SUPPORT_STATIC_ENTRIES && LWIP_TCPIP_CORE_LOCKING
  LOCK_TCPIP_CORE();
  err = etharp_remove_static_entry(ipaddr);
  UNLOCK_TCPIP_CORE();
#else
  /* @todo add new vars to struct netifapi_msg and create a 'do' func */
  LWIP_UNUSED_ARG(ipaddr);
  err = ERR_VAL;
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES && LWIP_TCPIP_CORE_LOCKING */

  return err;
}
#endif /* LWIP_ARP && LWIP_IPV4 */

/**
 * @ingroup netifapi_netif
 * Call netif_add() in a thread-safe way by running that function inside the
 * tcpip_thread context.
 *
 * @note for params @see netif_add()
 */
err_t
netifapi_netif_add(struct netif *netif,
#if LWIP_IPV4
                   const ip4_addr_t *ipaddr, const ip4_addr_t *netmask, const ip4_addr_t *gw,
#endif /* LWIP_IPV4 */
                   void *state, netif_init_fn init, netif_input_fn input)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

#if LWIP_IPV4
  if (ipaddr == NULL) {
    ipaddr = IP4_ADDR_ANY4;
  }
  if (netmask == NULL) {
    netmask = IP4_ADDR_ANY4;
  }
  if (gw == NULL) {
    gw = IP4_ADDR_ANY4;
  }
#endif /* LWIP_IPV4 */

  NETIFAPI_VAR_REF(msg).netif = netif;
#if LWIP_IPV4
  NETIFAPI_VAR_REF(msg).msg.add.ipaddr  = NETIFAPI_VAR_REF(ipaddr);
  NETIFAPI_VAR_REF(msg).msg.add.netmask = NETIFAPI_VAR_REF(netmask);
  NETIFAPI_VAR_REF(msg).msg.add.gw      = NETIFAPI_VAR_REF(gw);
#endif /* LWIP_IPV4 */
  NETIFAPI_VAR_REF(msg).msg.add.state   = state;
  NETIFAPI_VAR_REF(msg).msg.add.init    = init;
  NETIFAPI_VAR_REF(msg).msg.add.input   = input;
  err = tcpip_api_call(netifapi_do_netif_add, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}

/**
 * @ingroup netifapi_netif
 * Call netif_set_ioctl_fn() in a thread-safe way by running that function inside the
 * tcpip_thread context.
 *
 * @note for params @see netif_set_ioctl_fn()
 */
err_t
netifapi_netif_set_ioctl_fn(struct netif *netif,
                   netif_ioctl_fn ioctl)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).netif = netif;
  NETIFAPI_VAR_REF(msg).msg.set_ioctl.ioctl   = ioctl;
  err = tcpip_api_call(netifapi_do_netif_set_ioctl_fn, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}
#if LWIP_IPV4
/**
 * @ingroup netifapi_netif
 * Call netif_set_addr() in a thread-safe way by running that function inside the
 * tcpip_thread context.
 *
 * @note for params @see netif_set_addr()
 */
err_t
netifapi_netif_set_addr(struct netif *netif,
                        const ip4_addr_t *ipaddr,
                        const ip4_addr_t *netmask,
                        const ip4_addr_t *gw)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  if (ipaddr == NULL) {
    ipaddr = IP4_ADDR_ANY4;
  }
  if (netmask == NULL) {
    netmask = IP4_ADDR_ANY4;
  }
  if (gw == NULL) {
    gw = IP4_ADDR_ANY4;
  }

  NETIFAPI_VAR_REF(msg).netif = netif;
  NETIFAPI_VAR_REF(msg).msg.add.ipaddr  = NETIFAPI_VAR_REF(ipaddr);
  NETIFAPI_VAR_REF(msg).msg.add.netmask = NETIFAPI_VAR_REF(netmask);
  NETIFAPI_VAR_REF(msg).msg.add.gw      = NETIFAPI_VAR_REF(gw);
  err = tcpip_api_call(netifapi_do_netif_set_addr, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}
#endif /* LWIP_IPV4 */

/**
 * call the "errtfunc" (or the "voidfunc" if "errtfunc" is NULL) in a thread-safe
 * way by running that function inside the tcpip_thread context.
 *
 * @note use only for functions where there is only "netif" parameter.
 */
err_t
netifapi_netif_common(struct netif *netif, netifapi_void_fn voidfunc,
                      netifapi_errt_fn errtfunc,
					  netifapi_args_fn argsfunc, void *arg)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).netif = netif;
  NETIFAPI_VAR_REF(msg).msg.common.voidfunc = voidfunc;
  NETIFAPI_VAR_REF(msg).msg.common.errtfunc = errtfunc;
  NETIFAPI_VAR_REF(msg).msg.common.argsfunc = argsfunc;
  NETIFAPI_VAR_REF(msg).msg.common.arg = arg;
  err = tcpip_api_call(netifapi_do_netif_common, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}

/**
 * @ingroup netifapi_netif
 * Call netif_ioctl() in a thread-safe way by running that function inside the
 * tcpip_thread context.
 *
 * @note for params @see netif_ioctl()
 */
err_t
netifapi_netif_ioctl(const long cmd,
                        const void *argp)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).netif = NULL; /* netif will be determined in netif_ioctl() */
  NETIFAPI_VAR_REF(msg).msg.ioctl.cmd  = NETIFAPI_VAR_REF(cmd);
  NETIFAPI_VAR_REF(msg).msg.ioctl.argp = NETIFAPI_VAR_REF(argp);
  err = tcpip_api_call(netifapi_do_netif_ioctl, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}

#if defined(__WISE__) && defined(LWIP_NETIF_EXT_STATUS_CALLBACK)

/**
 * @ingroup netifapi_netif
 * Call netif_add_ext_callback() in a thread-safe way
 * by running that function inside the tcpip_thread context.
 *
 * @note for params @see netif_add_ext_callback()
 */
err_t
netifapi_add_ext_callback(netif_ext_callback_t *callback,
		netif_ext_callback_fn fn)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).netif = NULL; /* not used */
  NETIFAPI_VAR_REF(msg).msg.cb.callback  = NETIFAPI_VAR_REF(callback);
  NETIFAPI_VAR_REF(msg).msg.cb.callback_fn = NETIFAPI_VAR_REF(fn);
  err = tcpip_api_call(netifapi_do_add_ext_callback, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}

/**
 * @ingroup netifapi_netif
 * Call netif_remov_ext_callback() in a thread-safe way
 * by running that function inside the tcpip_thread context.
 *
 * @note for params @see netif_remov_ext_callback()
 */
err_t
netifapi_remove_ext_callback(netif_ext_callback_t *callback)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).netif = NULL; /* not used */
  NETIFAPI_VAR_REF(msg).msg.cb.callback  = NETIFAPI_VAR_REF(callback);
  err = tcpip_api_call(netifapi_do_remove_ext_callback, &API_VAR_REF(msg).call);
  NETIFAPI_VAR_FREE(msg);
  return err;
}

#endif

/**
* @ingroup netifapi_netif
* Call netif_name_to_index() in a thread-safe way by running that function inside the
* tcpip_thread context.
*
* @param name the interface name of the netif
* @param idx output index of the found netif
*/
err_t
netifapi_netif_name_to_index(const char *name, u8_t *idx)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  *idx = 0;

#if LWIP_MPU_COMPATIBLE
  strncpy(NETIFAPI_VAR_REF(msg).msg.ifs.name, name, NETIF_NAMESIZE - 1);
  NETIFAPI_VAR_REF(msg).msg.ifs.name[NETIF_NAMESIZE - 1] = '\0';
#else
  NETIFAPI_VAR_REF(msg).msg.ifs.name = LWIP_CONST_CAST(char *, name);
#endif /* LWIP_MPU_COMPATIBLE */
  err = tcpip_api_call(netifapi_do_name_to_index, &API_VAR_REF(msg).call);
  if (!err) {
    *idx = NETIFAPI_VAR_REF(msg).msg.ifs.index;
  }
  NETIFAPI_VAR_FREE(msg);
  return err;
}

/**
* @ingroup netifapi_netif
* Call netif_name_to_flags() in a thread-safe way by running that function inside the
* tcpip_thread context.
*
* @param name the interface name of the netif
* @param flags output flags of the found netif
*/
err_t
netifapi_netif_name_to_flags(const char *name, u8_t *flags)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  *flags = 0;

#if LWIP_MPU_COMPATIBLE
  strncpy(NETIFAPI_VAR_REF(msg).msg.ifs.name, name, NETIF_NAMESIZE - 1);
  NETIFAPI_VAR_REF(msg).msg.ifs.name[NETIF_NAMESIZE - 1] = '\0';
#else
  NETIFAPI_VAR_REF(msg).msg.ifs.name = LWIP_CONST_CAST(char *, name);
#endif /* LWIP_MPU_COMPATIBLE */
  err = tcpip_api_call(netifapi_do_name_to_flags, &API_VAR_REF(msg).call);
  if (!err) {
    *flags = NETIFAPI_VAR_REF(msg).msg.ifs.flags;
  }
  NETIFAPI_VAR_FREE(msg);
  return err;
}
/**
* @ingroup netifapi_netif
* Call netif_index_to_name() in a thread-safe way by running that function inside the
* tcpip_thread context.
*
* @param idx the interface index of the netif
* @param name output name of the found netif, empty '\0' string if netif not found.
*             name should be of at least NETIF_NAMESIZE bytes
*/
err_t
netifapi_netif_index_to_name(u8_t idx, char *name)
{
  err_t err;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  NETIFAPI_VAR_REF(msg).msg.ifs.index = idx;
#if !LWIP_MPU_COMPATIBLE
  NETIFAPI_VAR_REF(msg).msg.ifs.name = name;
#endif /* LWIP_MPU_COMPATIBLE */
  err = tcpip_api_call(netifapi_do_index_to_name, &API_VAR_REF(msg).call);
#if LWIP_MPU_COMPATIBLE
  if (!err) {
    strncpy(name, NETIFAPI_VAR_REF(msg).msg.ifs.name, NETIF_NAMESIZE - 1);
    name[NETIF_NAMESIZE - 1] = '\0';
  }
#endif /* LWIP_MPU_COMPATIBLE */
  NETIFAPI_VAR_FREE(msg);
  return err;
}

#ifdef __WISE__

/**
* @ingroup netifapi_netif
* Call netif_find() in a thread-safe way by running that function inside the
* tcpip_thread context.
*
* @param name the interface name of the netif
* @return pointer to a netif instance: if success, else NULL
*/
/* XXX: it might seem to be an overkill to send a message
 *      , when locking is disabled, just to get netif instance.
 *      Hope this function won't get called super frequently.
 */
struct netif *_netifapi_netif_find(const char *name);

#ifdef CONFIG_LINK_TO_ROM
/* FIXME: use which tag means use below codes with next ROM version */
/* PROVIDE(netifapi_netif_find, &netifapi_netif_find, &_netifapi_netif_find); */
struct netif *(*netifapi_netif_find)(const char *name) = _netifapi_netif_find;
#else
__func_tab__ struct netif *(*netifapi_netif_find)(const char *name) = _netifapi_netif_find;
#endif

struct netif *
_netifapi_netif_find(const char *name)
{
  err_t err;
  struct netif *netif;
  NETIFAPI_VAR_DECLARE(msg);
  NETIFAPI_VAR_ALLOC(msg);

  netif = NULL;

#if LWIP_MPU_COMPATIBLE
  strncpy(NETIFAPI_VAR_REF(msg).msg.ifs.name, name, NETIF_NAMESIZE - 1);
  NETIFAPI_VAR_REF(msg).msg.ifs.name[NETIF_NAMESIZE - 1] = '\0';
#else
  NETIFAPI_VAR_REF(msg).msg.ifs.name = LWIP_CONST_CAST(char *, name);
#endif /* LWIP_MPU_COMPATIBLE */
  err = tcpip_api_call(netifapi_do_find, &API_VAR_REF(msg).call);
  if (!err) {
    netif = NETIFAPI_VAR_REF(msg).netif;
  }
  NETIFAPI_VAR_FREE(msg);
  return netif;
}

#endif

#endif /* LWIP_NETIF_API */
