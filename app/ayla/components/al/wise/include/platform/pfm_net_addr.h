/*
 * Copyright 2018 Ayla Networks, Inc.  All rights reserved.
 *
 * Use of the accompanying software is permitted only in accordance
 * with and subject to the terms of the Software License Agreement
 * with Ayla Networks, Inc., a copy of which can be obtained from
 * Ayla Networks, Inc.
 */
#ifndef __AYLA_PFM_ESP_IDF_ADDR_H__
#define __AYLA_PFM_ESP_IDF_ADDR_H__

#include <lwip/ip_addr.h>

struct al_net_addr {
	ip_addr_t ip_addr;		/* LwIP IPv4 or IPv6 address */
};

#endif /* __AYLA_PFM_ESP_IDF_ADDR_H__ */
