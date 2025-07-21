/*
 * Copyright 2022 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <ayla/utypes.h>
#include <ayla/endian.h>
#include <al/al_err.h>
#include <al/al_net_addr.h>
#include <al/al_net_if.h>
#include <al/al_os_mem.h>
#include <platform/pfm_net_if.h>
#include <platform/pfm_net_addr.h>

#include <wise_wifi_types.h>
#include <wise_wifi.h>
#include <wise_err.h>

/*
 * Opaque network interface.
 */
struct al_net_if {
	enum al_net_if_type type;
	struct netif *netif;

	/*
	 * Fields addr and gw are to be used only for the return values of
	 * al_net_if_get_addr() and al_net_if_get_default_gw().
	 */
	struct al_net_addr addr;
	struct al_net_addr gw;
};

static struct al_net_if *al_net_ifs[AL_NET_IF_MAX];

int pfm_net_if_init(void)
{
	return 0;
}

enum al_err pfm_net_if_attach(enum al_net_if_type type, void *netif)
{
	struct al_net_if *nif;

	if ((unsigned)type > ARRAY_LEN(al_net_ifs) ||
	    type == AL_NET_IF_DEF) {
		return AL_ERR_INVAL_TYPE;
	}
	nif = al_net_ifs[type];
	if (!netif) {
		al_os_mem_free(nif);
		al_net_ifs[type] = NULL;
		return AL_ERR_OK;
	}
	if (!nif) {
		nif = al_os_mem_calloc(sizeof(*nif));
		if (!nif) {
			return AL_ERR_ALLOC;
		}
	}
	nif->type = type;
	nif->netif = netif;
	al_net_ifs[type] = nif;

	return AL_ERR_OK;
}

void *pfm_net_if_get(enum al_net_if_type type)
{
	struct al_net_if *nif;

	if ((unsigned)type >= ARRAY_LEN(al_net_ifs)) {
		return NULL;
	}
	nif = al_net_ifs[type];
	if (!nif) {
		return NULL;
	}
	return nif->netif;
}

struct al_net_if *al_net_if_get(enum al_net_if_type type)
{
#ifdef AYLA_MATTER_SUPPORT
	struct netif *netif;
	wise_err_t ret;
#endif
	if ((unsigned)type > ARRAY_LEN(al_net_ifs)) {
		return NULL;
	}

#ifdef AYLA_MATTER_SUPPORT
	if (!al_net_ifs[AL_NET_IF_STA]) {
        ret = wise_wifi_get_netif(WIFI_IF_STA, &netif);
        if (ret) {
            return NULL;
        }
		pfm_net_if_attach(AL_NET_IF_STA, netif);
	}
#endif

	if (type == AL_NET_IF_DEF) {
		type = AL_NET_IF_STA;
	}
	return al_net_ifs[type];
}

enum al_net_if_tech al_net_if_get_technology(struct al_net_if *nif)
{
	return AL_NET_IF_TECH_WIFI;
}

static int pfm_net_if_get_netinfo(struct al_net_if *nif,
		wifi_ip_info_t *info)
{
	uint8_t wifi_if;

	if (!nif || !nif->netif || !netif_is_up(nif->netif)) {
		return -1;
	}

	if (nif->type == AL_NET_IF_STA) {
		wifi_if = WIFI_IF_STA;
	} else if (nif->type == AL_NET_IF_AP) {
		wifi_if = WIFI_IF_AP;
	} else {
		return -1;
	}

	if (wise_wifi_get_ip_info(wifi_if, info)) {
		return -1;
	}
	return 0;
}

u32 al_net_if_get_ipv4(struct al_net_if *nif)
{
	wifi_ip_info_t info;

	if (pfm_net_if_get_netinfo(nif, &info)) {
		return 0;
	}
	return get_ua_be32(&info.ip.addr);
}

u32 al_net_if_get_netmask(struct al_net_if *nif)
{
	wifi_ip_info_t info;

	if (pfm_net_if_get_netinfo(nif, &info)) {
		return 0;
	}
	return get_ua_be32(&info.nm.addr);
}

int al_net_if_get_mac_addr(struct al_net_if *nif, u8 mac_addr[6])
{
	uint8_t wifi_if;

	if (!nif || !nif->netif) {
		return -1;
	}

	if (nif->type == AL_NET_IF_STA) {
		wifi_if = WIFI_IF_STA;
	} else if (nif->type == AL_NET_IF_AP) {
		wifi_if = WIFI_IF_AP;
	} else {
		return -1;
	}

	if (wise_wifi_get_mac(mac_addr, wifi_if)) {
		return -1;
	}

	return 0;
}

struct al_net_addr *al_net_if_get_addr(struct al_net_if *nif)
{
	wifi_ip_info_t info;

	if (pfm_net_if_get_netinfo(nif, &info)) {
		return 0;
	}
	al_net_addr_set_ipv4(&nif->addr, info.ip.addr);
	return &nif->addr;
}

struct al_net_addr *al_net_if_get_default_gw(struct al_net_if *nif)
{
	wifi_ip_info_t info;

	if (pfm_net_if_get_netinfo(nif, &info)) {
		return 0;
	}
	al_net_addr_set_ipv4(&nif->gw, info.gw.addr);
	return &nif->gw;
}

/*
 * Check for local address.
 * Note: this is done portably on top of the other AL APIs.
 * It should be moved to a higher layer and deprecated.
 */
int al_net_if_ipv4_is_local(u32 addr)
{
	struct al_net_if *nif;
	enum al_net_if_type type;
	u32 net;
	u32 netmask;

	if (!addr || !~addr) {
		return 0;	/* 0.0.0.0 or 255.255.255.255 */
	}
	if ((addr & 0xffff0000) == ((169 << 24) | (254 << 16)) &&
	    (addr & 0xffff) != 0 && (addr & 0xffff) != 0xffff) {
		return 1;	/* Auto-IP */
	}
	if (addr == ((127 << 24) | 1)) {
		return 1;	/* Loopback */
	}
	for (type = 0; type < AL_NET_IF_MAX; type++) {
		nif = al_net_if_get(type);
		if (!nif) {
			continue;
		}
		net = al_net_if_get_ipv4(nif);
		if (!net) {
			continue;
		}
		netmask = al_net_if_get_netmask(nif);
		if (!netmask) {
			continue;
		}
		if (!(addr & ~netmask) || (addr & ~netmask) == ~netmask) {
			continue;
		}
		if ((addr & netmask) == (net & netmask)) {
			return 1;
		}
	}
	return 0;
}
