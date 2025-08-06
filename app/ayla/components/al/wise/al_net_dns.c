/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <sys/queue.h>
#include "lwip/opt.h"
#include "lwip/stats.h"
#include "lwip/mem.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/iana.h"
#include <lwip/netdb.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>

#include <ayla/utypes.h>
#include <al/al_utypes.h>
#include <al/al_err.h>
#include <al/al_net_addr.h>
#include <al/al_net_dns.h>
#include <al/al_os_mem.h>
#include <ayla/assert.h>
#include <platform/pfm_net_if.h>
#include <platform/pfm_ada_thread.h>
#include <al/al_os_lock.h>
#include <platform/pfm_endian.h>
#include <ayla/endian.h>
#include <ayla/log.h>
#include <arpa/inet.h>
#include <dhcps/dhcps.h>

/*
 * Private request element.
 */
struct pfm_net_dns_req {
	struct al_net_dns_req *req;
	struct callback cb;			/* callback for response */
};

struct pfm_net_dns {
	struct al_lock *lock;			/* protects req/preq links */
};
static struct pfm_net_dns pfm_net_dns;

void pfm_net_dns_init(void)
{
	struct pfm_net_dns *dns = &pfm_net_dns;

	dns->lock = al_os_lock_create();
	ASSERT(dns->lock);
}

/*
 * DNS resolved callback with lock held.
 */
static void pfm_net_dns_resolved(const char *name,
		const ip_addr_t *ipaddr, void *arg)
{
	struct al_net_dns_req *req = arg;
	struct pfm_net_dns_req *preq;

	preq = req->al_priv;
	if (!preq) {
		return;		/* cancelled */
	}
	if (req->hostname &&
	    !strncasecmp(req->hostname, name, DNS_MAX_NAME_LENGTH) &&
	    ipaddr && IP_IS_V4(ipaddr)) {
		al_net_addr_set_ipv4(&req->addr,
		    ntohl(ip_addr_get_ip4_u32(ipaddr)));
		req->error = AL_ERR_OK;
	} else {
		req->error = AL_ERR_NOT_FOUND;
	}
	pfm_callback_pend(&preq->cb);
}

/*
 * DNS resolved callback from LwIP in the TCP/IP thread.
 */
static void pfm_net_dns_resolved_cb(const char *name,
		const ip_addr_t *ipaddr, void *arg)
{
	struct pfm_net_dns *dns = &pfm_net_dns;

	al_os_lock_lock(dns->lock);
	pfm_net_dns_resolved(name, ipaddr, arg);
	al_os_lock_unlock(dns->lock);
}

/*
 * Issue the non-blocking DNS request in the TCP/IP thread.
 */
static void pfm_net_dns_lookup(struct al_net_dns_req *req)
{
	err_t err;

	ASSERT(req);
	al_net_addr_set_ipv4(&req->addr, 0);
	err = dns_gethostbyname_addrtype(req->hostname,
	    &req->addr.ip_addr, pfm_net_dns_resolved_cb,
	    req, LWIP_DNS_ADDRTYPE_IPV4);
	switch (err) {
	case ERR_INPROGRESS:
		return;
	case ERR_OK:
		req->error = AL_ERR_OK;
		break;
	case ERR_MEM:
		req->error = AL_ERR_ALLOC;
		break;
	case ERR_VAL:
		req->error = AL_ERR_NOT_FOUND;
		break;
	default:
		req->error = AL_ERR_ERR;
		break;
	}
	pfm_net_dns_resolved(req->hostname, &req->addr.ip_addr, req);
}

/*
 * Handle DNS requests.
 * DNS LwIP callback running in the TCP/IP thread.
 */
static void pfm_net_dns_handler(void *arg)
{
	struct pfm_net_dns *dns = &pfm_net_dns;
	struct al_net_dns_req *req = arg;

	al_os_lock_lock(dns->lock);
	if (req->al_priv) {			/* not cancelled */
		pfm_net_dns_lookup(req);
	}
	al_os_lock_unlock(dns->lock);
}

/*
 * Deliver results.
 * Runs from callback in the ADA client thread.
 */
static void pfm_net_dns_deliver(void *arg)
{
	struct pfm_net_dns *dns = &pfm_net_dns;
	struct al_net_dns_req *req;
	struct pfm_net_dns_req *preq = arg;
	void (*callback)(struct al_net_dns_req *);

	al_os_lock_lock(dns->lock);
	req = preq->req;
	al_os_mem_free(preq);
	if (req) {
		req->al_priv = NULL;
	}
	al_os_lock_unlock(dns->lock);
	if (req) {
		callback = req->callback;
		if (callback) {
			callback(req);
		}
	}
}

/*
 * Issue a request for DNS lookup.
 */
enum al_err al_dns_req_ipv4_start(struct al_net_dns_req *req)
{
	struct pfm_net_dns_req *preq;
	err_t err;
	enum al_err al_err = AL_ERR_OK;

	ASSERT(req);
	ASSERT(req->hostname);
	ASSERT(req->callback);

	preq = al_os_mem_calloc(sizeof(*preq));
	if (!preq) {
		return AL_ERR_ALLOC;
	}
	callback_init(&preq->cb, pfm_net_dns_deliver, preq);
	preq->req = req;
	req->al_priv = preq;
	req->error = AL_ERR_IN_PROGRESS;

	/*
	 * Pend callback to LwIP.
	 * This can fail if the msg_queue is full.
	 */
	err = tcpip_callback(pfm_net_dns_handler, req);
	if (err) {
		log_put(LOG_ERR "%s: callback failed", __func__);
		req->al_priv = NULL;
		al_err = AL_ERR_ALLOC;
		req->error = AL_ERR_ALLOC;
		al_os_mem_free(preq);
		req->callback(req);
	}
	return al_err;
}

void al_dns_req_cancel(struct al_net_dns_req *req)
{
	struct pfm_net_dns *dns = &pfm_net_dns;
	struct pfm_net_dns_req *preq;

	ASSERT(req);

	al_os_lock_lock(dns->lock);
	preq = req->al_priv;
	if (preq && req->error == AL_ERR_IN_PROGRESS) {
		req->error = AL_ERR_CLSD;
		req->al_priv = NULL;
		preq->req = NULL;
		al_os_mem_free(preq);
	}
	al_os_lock_unlock(dns->lock);
}

void al_net_dns_delete_host(const char *hostname)
{
	ASSERT(hostname);
	al_net_dns_cache_clear();
}

void al_net_dns_servers_rotate(void)
{
}

struct dns_server_query {
  u8_t numdns;
  ip_addr_t *server;
};

/*
 * @ingroup dns
 * Query DNS server.
 */

static err_t dns_do_getserver(struct netif *netif, struct dns_server_query *query)
{
  const ip_addr_t *dns = dns_getserver(query->numdns);

  (void)netif;

  ip_addr_set(query->server, dns);

  return ERR_OK;
}

static err_t dns_queryserver(u8_t numdns, ip_addr_t *server)
{
  struct dns_server_query query = {.numdns = numdns, .server = server};

  return netifapi_netif_commona(NULL, dns_do_getserver, &query);
}

enum al_err al_net_dns_server_get(enum al_net_if_type if_type,
		unsigned int dns_index, struct al_net_addr *dns_addr)
{
	struct netif *netif;
	ip_addr_t dns, *dnsaddr = &dns;
	err_t err;

	netif = pfm_net_if_get(if_type);
	if (!netif) {
		return AL_ERR_ERR;
	}

	if (dns_index >= DNS_MAX_SERVERS) {
		return AL_ERR_NOT_FOUND;
	}

	/* Try to get dns from dhcp server first. */
	err = netifapi_dhcps_getdns(netif, dnsaddr);
	if (err != ERR_OK || dnsaddr == IP_ADDR_ANY) {
		/* Now turn to dns servers, if any. */
		err = dns_queryserver(dns_index, dnsaddr);
		if (err != ERR_OK || dnsaddr == IP_ADDR_ANY) {
			return AL_ERR_NOT_FOUND;
		}
	}
	al_net_addr_set_ipv4(dns_addr, ip_addr_get_ip4_u32(dnsaddr));

	return AL_ERR_OK;
}

/*
 * Callback to clear cache contents.
 */
static void pfm_net_dns_cache_clear_cb(void *arg)
{
	dns_cache_clear();
}

/*
 * Clear cache contents.
 */
enum al_err al_net_dns_cache_clear(void)
{
	err_t err;

	/* send call to LwIP thread */
	err = tcpip_callback(pfm_net_dns_cache_clear_cb, NULL);
	if (err) {
		return AL_ERR_ALLOC;
	}
	return AL_ERR_OK;
}

#if 0

#include "cli.h"

#define TCP_LOG(fmt, ...) printf("[TCP] " fmt, ##__VA_ARGS__)

struct al_net_dns_req dns_req;
char peer_name[DNS_MAX_NAME_LENGTH];
char addr_buf[20];

static void test_dns_cb(struct al_net_dns_req *req)
{
    const char *name = req->hostname;

    if (req->error) {
        TCP_LOG("dns cb, err=%d\n", req->error);
        return;
    }

    TCP_LOG("dns cb, host \"%s\" at %s\n",
        name,
        ipaddr_fmt_ipv4_to_str(al_net_addr_get_ipv4(&req->addr), addr_buf, sizeof(addr_buf)));
}

static int do_host(int argc, char *argv[])
{
    enum al_err err;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    strcpy(peer_name, argv[1]);

    dns_req.hostname = peer_name;
    dns_req.callback = test_dns_cb;

    err = al_dns_req_ipv4_start(&dns_req);
    if (AL_ERR_OK != err) {
        TCP_LOG("dns, err=%d\n", err);
    }

    return CMD_RET_SUCCESS;
}

CMD(host, do_host,
        "find host ip address",
        "host name"
);

#endif
