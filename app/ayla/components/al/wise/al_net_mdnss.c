/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <ayla/log.h>
#include <al/al_net_mdnss.h>
#include <platform/pfm_net_if.h>
#include <mdns/mdns.h>

void al_mdns_server_up(struct al_net_if *net_if, const char *hostname)
{
	struct netif *netif;
	err_t err;

	(void)net_if; /* ADW doesn't bother to set. */

	netif = pfm_net_if_get(AL_NET_IF_STA);
	if (!netif) {
		log_put(LOG_ERR "%s: netif is NULL", __func__);
		return;
	}

	mdns_resp_init();

	err = mdns_resp_add_netif(netif, hostname);
	if (err != ERR_OK) {
		log_put(LOG_ERR "%s: error(%d) from mdns_resp_add_netif", __func__, err);
	}
}

void al_mdns_server_down(struct al_net_if *net_if)
{
	struct netif *netif;
	err_t err;

	(void)net_if; /* ADW doesn't bother to set. */

	netif = pfm_net_if_get(AL_NET_IF_STA);
	if (!netif) {
		log_put(LOG_ERR "%s: netif is NULL", __func__);
		return;
	}

	err = mdns_resp_remove_netif(netif);
	if (err != ERR_OK) {
		log_put(LOG_ERR "%s: error(%d) from mdns_resp_remove_netif", __func__, err);
	}
}
