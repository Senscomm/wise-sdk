/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#include <stdlib.h>
#include <al/al_net_addr.h>
#include <lwip/inet.h>
#include <lwip/ip_addr.h>

u32 al_net_addr_get_ipv4(const struct al_net_addr *addr)
{
	return lwip_ntohl(ip_addr_get_ip4_u32(&addr->ip_addr));
}

void al_net_addr_set_ipv4(struct al_net_addr *addr, u32 IP)
{
	ip_addr_set_ip4_u32(&addr->ip_addr, lwip_htonl(IP));
}

int al_net_addr_is_ipv6(const struct al_net_addr *addr)
{
	return IP_IS_V6(&addr->ip_addr);
}
